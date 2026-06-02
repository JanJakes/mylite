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

static int validate_column_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *default_node,
    const struct planned_column *column
) {
    if (default_node == NULL) {
        return MYLITE_OK;
    }
    if (default_node->kind == MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE) {
        return validate_column_default_value(database, default_node, column);
    }
    if (default_node->kind != MYLITE_SQL_AST_COLUMN_DEFAULT_NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (!column->is_nullable) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_column_default_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *default_node,
    const struct planned_column *column
) {
    const struct mylite_sql_ast_node *value_node = child_at(default_node, 0U);

    if (column_default_value_is_current_timestamp_expression(value_node)) {
        if (planned_column_is_datetime(column) || planned_column_is_timestamp(column)) {
            return MYLITE_OK;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (column_default_value_is_current_date_expression(value_node)) {
        if (planned_column_is_date(column)) {
            return MYLITE_OK;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (column_default_value_is_current_time_expression(value_node)) {
        if (planned_column_is_time(column)) {
            return MYLITE_OK;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_text_family(column)) {
        if (column_default_value_is_parenthesized_text_expression(default_node)) {
            return MYLITE_OK;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_binary_string_family(column)) {
        return validate_binary_column_default_value(database, default_node, column);
    }
    if (planned_column_is_spatial(column)) {
        set_json_cant_have_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_json(column)) {
        if (column_default_value_is_parenthesized_expression(default_node)) {
            set_unsupported_error(database, "JSON expression defaults are not yet supported");
            return MYLITE_ERROR;
        }
        set_json_cant_have_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int validate_binary_column_default_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *default_node,
    const struct planned_column *column
) {
    const struct mylite_sql_ast_node *value_node = child_at(default_node, 0U);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (planned_column_is_binary_blob_family(column)) {
        if (column_default_value_is_parenthesized_binary_blob_expression(default_node)) {
            return MYLITE_OK;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(value_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
        literal_kind == MYLITE_SQL_AST_LITERAL_HEX) {
        return MYLITE_OK;
    }
    set_invalid_default_error(database, column->name);
    return MYLITE_ERROR;
}

static int finalize_planned_column_defaults(
    struct mylite_db *database,
    struct planned_column *columns,
    size_t column_count
) {
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        int rc = finalize_planned_column_default(database, &columns[column_index]);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int finalize_planned_column_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    const struct mylite_sql_ast_node *value_node = NULL;

    column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NONE;
    column->default_integer = 0;
    column->default_text[0] = '\0';
    if (column->default_node == NULL ||
        column->default_node->kind == MYLITE_SQL_AST_COLUMN_DEFAULT_NULL) {
        return MYLITE_OK;
    }
    if (column->default_node->kind != MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    value_node = child_at(column->default_node, 0U);
    if (column_default_value_is_current_timestamp_expression(value_node)) {
        if (planned_column_is_datetime(column) || planned_column_is_timestamp(column)) {
            column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP;
            return MYLITE_OK;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (column_default_value_is_current_date_expression(value_node)) {
        if (planned_column_is_date(column)) {
            column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE;
            return MYLITE_OK;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (column_default_value_is_current_time_expression(value_node)) {
        if (planned_column_is_time(column)) {
            column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME;
            return MYLITE_OK;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (value_node != NULL &&
        value_node->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "CURRENT_TIMESTAMP");
        return MYLITE_ERROR;
    }
    if (column_default_value_is_parenthesized_expression(column->default_node)) {
        return finalize_planned_column_parenthesized_default(database, column, value_node);
    }

    return finalize_planned_column_type_default(database, column);
}

static int finalize_planned_column_parenthesized_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
) {
    if (planned_column_is_char_or_varchar(column)) {
        return finalize_planned_column_character_expression_default(database, column, value_node);
    }
    if (planned_column_is_text_family(column)) {
        return finalize_planned_column_text_expression_default(database, column, value_node);
    }
    if (planned_column_is_binary_blob_family(column)) {
        return finalize_planned_column_binary_blob_expression_default(database, column, value_node);
    }
    if (planned_column_is_bit(column)) {
        return finalize_planned_column_bit_expression_default(database, column, value_node);
    }
    if (planned_column_is_year(column)) {
        return finalize_planned_column_year_expression_default(database, column, value_node);
    }
    if (planned_column_is_enum(column) || planned_column_is_set(column) ||
        planned_column_is_json(column) || planned_column_is_spatial(column)) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    return finalize_planned_column_integer_expression_default(database, column, value_node);
}

static int finalize_planned_column_type_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    int64_t default_integer = 0;
    int rc = MYLITE_OK;

    if (planned_column_is_char(column) || planned_column_is_varchar(column)) {
        return finalize_planned_column_string_default(database, column);
    }
    if (planned_column_is_decimal(column)) {
        return finalize_planned_column_decimal_default(database, column);
    }
    if (planned_column_is_approximate(column)) {
        return finalize_planned_column_approximate_default(database, column);
    }
    if (planned_column_is_bit(column)) {
        return finalize_planned_column_bit_default(database, column);
    }
    if (planned_column_is_binary_string_family(column)) {
        return finalize_planned_column_binary_default(database, column);
    }
    if (planned_column_is_year(column)) {
        return finalize_planned_column_year_default(database, column);
    }
    if (planned_column_is_date(column)) {
        return finalize_planned_column_date_default(database, column);
    }
    if (planned_column_is_time(column)) {
        return finalize_planned_column_time_default(database, column);
    }
    if (planned_column_is_datetime(column)) {
        return finalize_planned_column_datetime_default(database, column);
    }
    if (planned_column_is_timestamp(column)) {
        return finalize_planned_column_timestamp_default(database, column);
    }
    if (planned_column_is_enum(column)) {
        return finalize_planned_column_enum_default(database, column);
    }
    if (planned_column_is_set(column)) {
        return finalize_planned_column_set_default(database, column);
    }

    rc = convert_column_default_value(
        database,
        child_at(column->default_node, 0U),
        column,
        &default_integer
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER;
    column->default_integer = default_integer;

    return MYLITE_OK;
}

static int finalize_planned_column_integer_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
) {
    struct scalar_arithmetic_value expression_value = {.is_null = false, .integer = 0};
    int64_t default_integer = 0;
    int rc = evaluate_integer_default_expression(database, value_node, &expression_value);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            return rc;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (expression_value.is_null) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION;
        snprintf(column->default_text, sizeof(column->default_text), "NULL");
        return MYLITE_OK;
    }

    rc = convert_integer_expression_default_result(
        database,
        column,
        expression_value.integer,
        &default_integer
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    rc = copy_planned_integer_expression_default_text(database, column, value_node);
    if (rc != MYLITE_OK) {
        return rc;
    }
    column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION;
    column->default_integer = default_integer;

    return MYLITE_OK;
}

static int finalize_planned_column_bit_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
) {
    struct scalar_arithmetic_value expression_value = {.is_null = false, .integer = 0};
    int64_t default_integer = 0;
    int rc = evaluate_integer_default_expression(database, value_node, &expression_value);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            return rc;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (expression_value.is_null) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION;
        snprintf(column->default_text, sizeof(column->default_text), "NULL");
        return MYLITE_OK;
    }

    rc = convert_bit_expression_default_result(
        database,
        column,
        expression_value.integer,
        &default_integer
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    rc = copy_planned_integer_expression_default_text(database, column, value_node);
    if (rc != MYLITE_OK) {
        return rc;
    }
    column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION;
    column->default_integer = default_integer;

    return MYLITE_OK;
}

static int finalize_planned_column_year_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
) {
    struct scalar_arithmetic_value expression_value = {.is_null = false, .integer = 0};
    int64_t default_integer = 0;
    int rc = evaluate_integer_default_expression(database, value_node, &expression_value);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            return rc;
        }
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (expression_value.is_null) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION;
        snprintf(column->default_text, sizeof(column->default_text), "NULL");
        return MYLITE_OK;
    }

    rc = convert_year_expression_default_result(expression_value.integer, &default_integer);
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    rc = copy_planned_integer_expression_default_text(database, column, value_node);
    if (rc != MYLITE_OK) {
        return rc;
    }
    column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION;
    column->default_integer = default_integer;

    return MYLITE_OK;
}

static int finalize_planned_column_text_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *expression = unwrap_parenthesized_expression(value_node);
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION;
        snprintf(column->default_text, sizeof(column->default_text), "NULL");
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_text_family_literal(database, expression, &descriptor, 1U, false, false, &value);
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static int finalize_planned_column_character_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *expression = unwrap_parenthesized_expression(value_node);
    char *text = NULL;
    size_t text_length = 0U;
    size_t character_count = 0U;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION;
        snprintf(column->default_text, sizeof(column->default_text), "NULL");
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        expression,
        "CHAR/VARCHAR generated defaults support only string literals",
        "CHAR/VARCHAR generated defaults do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK) {
        rc = validate_utf8_text(text, text_length, &character_count);
        if (rc != MYLITE_OK) {
            set_invalid_default_error(database, column->name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_character_expression_default_text(database, column, text, text_length);
    }
    free(text);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION;
    }

    return rc;
}

static int finalize_planned_column_string_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    return finalize_planned_column_string_default_value(
        database,
        column,
        child_at(column->default_node, 0U)
    );
}

static int finalize_planned_column_parenthesized_string_literal_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    const struct mylite_sql_ast_node *expression =
        unwrap_parenthesized_expression(child_at(column->default_node, 0U));

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NONE;
        column->default_text[0] = '\0';
        return MYLITE_OK;
    }

    return finalize_planned_column_string_default_value(database, column, expression);
}

static int finalize_planned_column_string_default_value(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(column, &descriptor);
    if (planned_column_is_char(column)) {
        rc = convert_char_literal(database, value_node, &descriptor, 1U, false, &value);
    } else {
        rc = convert_varchar_literal(database, value_node, &descriptor, 1U, false, &value);
    }
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static int finalize_planned_column_decimal_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    const struct mylite_sql_ast_node *value_node = child_at(column->default_node, 0U);
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(column, &descriptor);
    if (value_node != NULL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    rc = convert_decimal_literal(database, value_node, &descriptor, 1U, false, &value);
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL;
    }

    return rc;
}

static int finalize_planned_column_approximate_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    const struct mylite_sql_ast_node *value_node = child_at(column->default_node, 0U);
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_real = false,
        .integer = 0,
        .real = 0.0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(column, &descriptor);
    if (value_node != NULL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    rc = convert_approximate_literal(database, value_node, &descriptor, 1U, false, &value);
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_approximate_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static int finalize_planned_column_bit_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    const struct mylite_sql_ast_node *value_node = child_at(column->default_node, 0U);
    uint64_t width = 0U;
    uint64_t magnitude = 0U;
    bool is_negative = false;
    bool parse_overflow = false;
    int rc = bit_width_for_logical_type(column->logical_type, &width);

    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (value_node == NULL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        (mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING ||
         mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_HEX)) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    rc = parse_bit_value_magnitude(value_node, &magnitude, &is_negative, &parse_overflow);
    if (rc != MYLITE_OK || parse_overflow || (is_negative && magnitude != 0U) ||
        magnitude > bit_width_max_value(width)) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    rc = format_bit_default_text(
        database,
        column->name,
        magnitude,
        column->default_text,
        sizeof(column->default_text)
    );
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }
    return rc;
}

static int finalize_planned_column_binary_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_blob = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    if (planned_column_is_binary_blob_family(column)) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_binary_string_literal(
        database,
        child_at(column->default_node, 0U),
        &descriptor,
        1U,
        false,
        &value
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_binary_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_BINARY;
    }

    return rc;
}

static int finalize_planned_column_binary_blob_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *expression = unwrap_parenthesized_expression(value_node);
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_blob = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION;
        snprintf(column->default_text, sizeof(column->default_text), "NULL");
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_HEX) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_binary_string_literal(database, expression, &descriptor, 1U, false, &value);
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_binary_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_BINARY;
    }

    return rc;
}

static int finalize_planned_column_year_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_year_literal(
        database,
        child_at(column->default_node, 0U),
        &descriptor,
        1U,
        false,
        &value
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static int finalize_planned_column_date_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_date_literal(
        database,
        child_at(column->default_node, 0U),
        &descriptor,
        1U,
        false,
        &value
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static int finalize_planned_column_time_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_time_literal(
        database,
        child_at(column->default_node, 0U),
        &descriptor,
        1U,
        false,
        &value
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static int finalize_planned_column_datetime_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_datetime_literal(
        database,
        child_at(column->default_node, 0U),
        &descriptor,
        1U,
        false,
        &value
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static int finalize_planned_column_timestamp_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_timestamp_literal(
        database,
        child_at(column->default_node, 0U),
        &descriptor,
        1U,
        false,
        &value
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static int finalize_planned_column_enum_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    if (column->default_node == NULL ||
        column->default_node->kind != MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_enum_string_literal(
        database,
        child_at(column->default_node, 0U),
        &descriptor,
        1U,
        false,
        true,
        ENUM_STRING_TRIM_TRAILING_SPACES,
        &value
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static int finalize_planned_column_set_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    if (column->default_node == NULL ||
        column->default_node->kind != MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    planned_column_descriptor_for_default(column, &descriptor);
    rc = convert_set_string_literal(
        database,
        child_at(column->default_node, 0U),
        &descriptor,
        1U,
        false,
        true,
        true,
        &value
    );
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = copy_planned_default_text(database, column, &value);
    }
    planned_value_deinit(&value);
    if (rc == MYLITE_OK) {
        column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }

    return rc;
}

static bool column_default_value_is_current_date_expression(
    const struct mylite_sql_ast_node *value_node
) {
    value_node = unwrap_parenthesized_expression(value_node);
    return (value_node != NULL && value_node->kind == MYLITE_SQL_AST_CURRENT_DATE_VALUE) != 0;
}

static bool column_default_value_is_current_time_expression(
    const struct mylite_sql_ast_node *value_node
) {
    value_node = unwrap_parenthesized_expression(value_node);
    return (value_node != NULL && value_node->kind == MYLITE_SQL_AST_CURRENT_TIME_VALUE) != 0;
}

static bool column_default_value_is_current_timestamp_expression(
    const struct mylite_sql_ast_node *value_node
) {
    value_node = unwrap_parenthesized_expression(value_node);
    return (value_node != NULL && value_node->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE) != 0;
}

static void planned_column_descriptor_for_default(
    const struct planned_column *column,
    struct mylite_catalog_column_descriptor *out_descriptor
) {
    memset(out_descriptor, 0, sizeof(*out_descriptor));
    snprintf(out_descriptor->name, sizeof(out_descriptor->name), "%s", column->name);
    snprintf(
        out_descriptor->logical_type,
        sizeof(out_descriptor->logical_type),
        "%s",
        column->logical_type
    );
    snprintf(
        out_descriptor->physical_type,
        sizeof(out_descriptor->physical_type),
        "%s",
        column->physical_type
    );
    out_descriptor->is_nullable = column->is_nullable;
}

static int copy_planned_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    struct planned_value *value
) {
    if (value->text == NULL || value->text_length >= sizeof(column->default_text)) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    memcpy(column->default_text, value->text, value->text_length);
    column->default_text[value->text_length] = '\0';

    return MYLITE_OK;
}

static int copy_planned_character_expression_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    const char *text,
    size_t text_length
) {
    if (text == NULL || text_length >= sizeof(column->default_text)) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    memcpy(column->default_text, text, text_length);
    column->default_text[text_length] = '\0';
    return MYLITE_OK;
}

static int copy_planned_binary_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    const struct planned_value *value
) {
    char *hex_text = NULL;
    size_t hex_length = 0U;
    int rc = MYLITE_OK;

    if (value == NULL || !value->is_blob || (value->text == NULL && value->text_length != 0U)) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    rc = format_hex_bytes(
        database,
        (const unsigned char *)value->text,
        value->text_length,
        &hex_text
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    hex_length = strlen(hex_text);
    if (hex_length >= sizeof(column->default_text)) {
        free(hex_text);
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    memcpy(column->default_text, hex_text, hex_length + 1U);
    free(hex_text);
    return MYLITE_OK;
}

static int copy_planned_default_approximate_text(
    struct mylite_db *database,
    struct planned_column *column,
    const struct planned_value *value
) {
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    int rc = approximate_type_info_for_logical_type(column->logical_type, &info);

    if (rc == MYLITE_OK && (value == NULL || !value->is_real)) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = format_approximate_value_text(
            database,
            value->real,
            &info,
            "default approximate",
            column->default_text,
            sizeof(column->default_text)
        );
    }
    if (rc != MYLITE_OK) {
        set_invalid_default_error(database, column->name);
    }

    return rc;
}

static int copy_planned_integer_expression_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *expression
) {
    struct mylite_dynamic_string text = {0};
    int rc = MYLITE_OK;

    mylite_dynamic_string_init(&text);
    rc = append_integer_expression_default_text(&text, expression);
    if (rc == MYLITE_OK && (text.length == 0U || text.length >= sizeof(column->default_text))) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        memcpy(column->default_text, text.text, text.length);
        column->default_text[text.length] = '\0';
    } else if (rc != MYLITE_NOMEM) {
        set_invalid_default_error(database, column->name);
        rc = MYLITE_ERROR;
    }

    mylite_dynamic_string_deinit(&text);
    return rc;
}

static int append_integer_expression_default_text(
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *expression
) {
    struct integer_expression_default_text_stack stack = {
        .items = NULL,
        .count = 0U,
        .capacity = 0U,
    };
    int rc = MYLITE_OK;

    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    rc = push_integer_expression_default_text_item(
        &stack,
        INTEGER_EXPRESSION_DEFAULT_TEXT_NODE,
        expression,
        NULL
    );
    while (rc == MYLITE_OK && stack.count > 0U) {
        struct integer_expression_default_text_work_item item = stack.items[--stack.count];

        rc = append_integer_expression_default_text_item(string, &stack, &item);
    }

    free(stack.items);
    return rc;
}

static int append_integer_expression_default_text_item(
    struct mylite_dynamic_string *string,
    struct integer_expression_default_text_stack *stack,
    const struct integer_expression_default_text_work_item *item
) {
    if (item->kind == INTEGER_EXPRESSION_DEFAULT_TEXT_BYTES) {
        return mylite_dynamic_string_append(string, item->text);
    }
    if (item->kind == INTEGER_EXPRESSION_DEFAULT_TEXT_NODE) {
        return append_integer_expression_default_node_text(string, stack, item->expression);
    }
    return MYLITE_ERROR;
}

static int append_integer_expression_default_node_text(
    struct mylite_dynamic_string *string,
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return append_integer_expression_default_literal_text(string, expression);
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
        return append_integer_expression_default_parenthesized_text(stack, expression);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return append_integer_expression_default_unary_text(stack, expression);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return append_integer_expression_default_binary_text(stack, expression);
    case MYLITE_SQL_AST_MOD_FUNCTION:
        return append_integer_expression_default_mod_text(stack, expression);
    default:
        return MYLITE_ERROR;
    }
}

static int append_integer_expression_default_parenthesized_text(
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    int rc = push_integer_expression_default_text_item(
        stack,
        INTEGER_EXPRESSION_DEFAULT_TEXT_BYTES,
        NULL,
        ")"
    );

    if (rc == MYLITE_OK) {
        rc = push_integer_expression_default_text_item(
            stack,
            INTEGER_EXPRESSION_DEFAULT_TEXT_NODE,
            child_at(expression, 0U),
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = push_integer_expression_default_text_item(
            stack,
            INTEGER_EXPRESSION_DEFAULT_TEXT_BYTES,
            NULL,
            "("
        );
    }
    return rc;
}

static int append_integer_expression_default_unary_text(
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
    const char *operator_text = NULL;
    int rc = MYLITE_OK;

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
        operator_text = "+";
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        operator_text = "-";
    } else {
        return MYLITE_ERROR;
    }

    rc = push_integer_expression_default_text_item(
        stack,
        INTEGER_EXPRESSION_DEFAULT_TEXT_NODE,
        child_at(expression, 0U),
        NULL
    );
    if (rc == MYLITE_OK) {
        rc = push_integer_expression_default_text_item(
            stack,
            INTEGER_EXPRESSION_DEFAULT_TEXT_BYTES,
            NULL,
            operator_text
        );
    }
    return rc;
}

static int append_integer_expression_default_binary_text(
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    const char *operator_text =
        integer_expression_default_binary_operator_text(mylite_sql_ast_node_operator(expression));
    int rc = MYLITE_OK;

    if (operator_text == NULL) {
        return MYLITE_ERROR;
    }

    rc = push_integer_expression_default_text_item(
        stack,
        INTEGER_EXPRESSION_DEFAULT_TEXT_NODE,
        child_at(expression, 1U),
        NULL
    );
    if (rc == MYLITE_OK) {
        rc = push_integer_expression_default_text_item(
            stack,
            INTEGER_EXPRESSION_DEFAULT_TEXT_BYTES,
            NULL,
            operator_text
        );
    }
    if (rc == MYLITE_OK) {
        rc = push_integer_expression_default_text_item(
            stack,
            INTEGER_EXPRESSION_DEFAULT_TEXT_NODE,
            child_at(expression, 0U),
            NULL
        );
    }
    return rc;
}

static int append_integer_expression_default_mod_text(
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_child_count(expression) != 2U) {
        return MYLITE_ERROR;
    }

    rc = push_integer_expression_default_text_item(
        stack,
        INTEGER_EXPRESSION_DEFAULT_TEXT_NODE,
        child_at(expression, 1U),
        NULL
    );
    if (rc == MYLITE_OK) {
        rc = push_integer_expression_default_text_item(
            stack,
            INTEGER_EXPRESSION_DEFAULT_TEXT_BYTES,
            NULL,
            " % "
        );
    }
    if (rc == MYLITE_OK) {
        rc = push_integer_expression_default_text_item(
            stack,
            INTEGER_EXPRESSION_DEFAULT_TEXT_NODE,
            child_at(expression, 0U),
            NULL
        );
    }
    return rc;
}

static const char *integer_expression_default_binary_operator_text(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
        return " + ";
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        return " - ";
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return " * ";
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
        return " DIV ";
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        return " % ";
    default:
        return NULL;
    }
}

static int push_integer_expression_default_text_item(
    struct integer_expression_default_text_stack *stack,
    enum integer_expression_default_text_work_item_kind kind,
    const struct mylite_sql_ast_node *expression,
    const char *text
) {
    struct integer_expression_default_text_work_item *grown_items = NULL;
    size_t new_capacity = 0U;

    if (stack->count == stack->capacity) {
        new_capacity = stack->capacity == 0U
                           ? integer_expression_default_text_initial_capacity
                           : stack->capacity * integer_expression_default_text_growth_factor;
        if (new_capacity < stack->capacity || new_capacity > SIZE_MAX / sizeof(*stack->items)) {
            return MYLITE_NOMEM;
        }
        grown_items = realloc(stack->items, new_capacity * sizeof(*grown_items));
        if (grown_items == NULL) {
            return MYLITE_NOMEM;
        }
        stack->items = grown_items;
        stack->capacity = new_capacity;
    }

    stack->items[stack->count] = (struct integer_expression_default_text_work_item){
        .kind = kind,
        .expression = expression,
        .text = text,
    };
    ++stack->count;
    return MYLITE_OK;
}

static int append_integer_expression_default_literal_text(
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL || expression->span.text == NULL || expression->span.length == 0U) {
        return MYLITE_ERROR;
    }
    return mylite_dynamic_string_append_bytes(
        string,
        expression->span.text,
        expression->span.length
    );
}

static int convert_integer_expression_default_result(
    struct mylite_db *database,
    const struct planned_column *column,
    int64_t value,
    int64_t *out_value
) {
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct integer_column_range range = {0};
    uint64_t magnitude = 0U;
    bool is_negative = value < 0;
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(column, &descriptor);
    rc = integer_range_for_column(
        database,
        &descriptor,
        "DEFAULT expression supports only baseline integer columns",
        &range
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (is_negative) {
        magnitude = value == INT64_MIN ? bigint_signed_negative_abs_max : (uint64_t)(-value);
        if (range.negative_abs_max == 0U || magnitude > range.negative_abs_max) {
            return MYLITE_ERROR;
        }
    } else {
        magnitude = (uint64_t)value;
        if (magnitude > range.positive_max) {
            return MYLITE_ERROR;
        }
    }

    *out_value = value;
    return MYLITE_OK;
}

static int convert_bit_expression_default_result(
    struct mylite_db *database,
    const struct planned_column *column,
    int64_t value,
    int64_t *out_value
) {
    uint64_t width = 0U;
    uint64_t magnitude = 0U;
    int rc = bit_width_for_logical_type(column == NULL ? NULL : column->logical_type, &width);

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (rc != MYLITE_OK || value < 0) {
        return MYLITE_ERROR;
    }

    magnitude = (uint64_t)value;
    if (magnitude > bit_width_max_value(width)) {
        return MYLITE_ERROR;
    }

    *out_value = value;
    (void)database;
    return MYLITE_OK;
}

static int convert_year_expression_default_result(int64_t value, int64_t *out_value) {
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;
    uint64_t magnitude = 0U;
    uint32_t year = 0U;
    bool is_negative = value < 0;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    magnitude = value == INT64_MIN ? bigint_signed_negative_abs_max : (uint64_t)llabs(value);
    if (!convert_year_numeric_magnitude(magnitude, is_negative, &year)) {
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)year;
    return MYLITE_OK;
}

static int convert_column_default_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct planned_column *column,
    int64_t *out_value
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct integer_column_range range = {0};
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = parse_column_default_integer_magnitude(
        database,
        value_node,
        column,
        &magnitude,
        &is_negative
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    planned_column_descriptor_for_default(column, &descriptor);
    rc = integer_range_for_column(
        database,
        &descriptor,
        "DEFAULT supports only baseline integer columns",
        &range
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return finish_column_default_integer_value(
        database,
        column,
        &range,
        magnitude,
        is_negative,
        out_value
    );
}

static int parse_column_default_integer_magnitude(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct planned_column *column,
    uint64_t *out_magnitude,
    bool *out_is_negative
) {
    const struct mylite_sql_ast_node *literal = value_node;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool has_unary = false;
    int rc = MYLITE_OK;

    if (value_node == NULL || column == NULL || out_magnitude == NULL || out_is_negative == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    *out_magnitude = 0U;
    *out_is_negative = false;
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        has_unary = true;
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            *out_is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_invalid_default_error(database, column->name);
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (boolean_literal_magnitude(literal, out_magnitude)) {
        return MYLITE_OK;
    }

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        rc = parse_unsigned_integer_literal(&literal->span, out_magnitude);
    } else if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING && !has_unary) {
        rc = parse_column_default_integer_string(database, literal, out_magnitude, out_is_negative);
    } else {
        rc = MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        if (rc != MYLITE_NOMEM) {
            set_invalid_default_error(database, column->name);
        }
        return rc;
    }

    return MYLITE_OK;
}

static int finish_column_default_integer_value(
    struct mylite_db *database,
    const struct planned_column *column,
    const struct integer_column_range *range,
    uint64_t magnitude,
    bool is_negative,
    int64_t *out_value
) {
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;

    if (column == NULL || range == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (is_negative) {
        if ((range->negative_abs_max == 0U && magnitude != 0U) ||
            magnitude > range->negative_abs_max) {
            set_invalid_default_error(database, column->name);
            return MYLITE_ERROR;
        }
        *out_value = magnitude == bigint_signed_negative_abs_max ? INT64_MIN : -(int64_t)magnitude;
        return MYLITE_OK;
    }
    if (magnitude > range->positive_max) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)magnitude;
    return MYLITE_OK;
}

static int parse_column_default_integer_string(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_magnitude,
    bool *out_is_negative
) {
    char *text = NULL;
    size_t text_length = 0U;
    enum mylite_execution_dml_numeric_parse_result parse_result =
        MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    int rc = decode_sql_string_literal(
        database,
        literal,
        "DEFAULT supports only baseline integer literals and exact quoted integer strings",
        "invalid integer default string literal",
        &text,
        &text_length
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    parse_result = mylite_execution_parse_signed_integer_text(
        text,
        text_length,
        out_magnitude,
        out_is_negative
    );
    if (parse_result != MYLITE_EXECUTION_DML_NUMERIC_PARSE_OK) {
        rc = MYLITE_ERROR;
    }

    free(text);
    return rc;
}

static int evaluate_integer_default_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
) {
    struct scalar_arithmetic_eval_stack expression_stack = {0};
    struct scalar_arithmetic_value_stack value_stack = {0};
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){.is_null = false, .integer = 0};
    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    rc = scalar_arithmetic_eval_stack_push(
        database,
        &expression_stack,
        SCALAR_ARITHMETIC_EVAL_ENTER,
        expression,
        MYLITE_SQL_AST_OPERATOR_NONE
    );
    while (rc == MYLITE_OK && expression_stack.count != 0U) {
        struct scalar_arithmetic_eval_frame frame =
            expression_stack.items[--expression_stack.count];

        rc = evaluate_integer_default_frame(database, &expression_stack, &value_stack, &frame);
    }
    if (rc == MYLITE_OK) {
        rc = finish_integer_default_result(&value_stack, out_value);
    }
    scalar_arithmetic_eval_stack_deinit(&expression_stack);
    scalar_arithmetic_value_stack_deinit(&value_stack);
    return rc;
}

static int evaluate_integer_default_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_arithmetic_eval_frame *frame
) {
    if (frame == NULL) {
        return MYLITE_MISUSE;
    }
    if (frame->kind == SCALAR_ARITHMETIC_EVAL_APPLY) {
        return evaluate_integer_default_apply_frame(database, value_stack, frame->operator_kind);
    }
    if (frame->kind == SCALAR_ARITHMETIC_EVAL_APPLY_UNARY) {
        return evaluate_integer_default_apply_unary_frame(
            database,
            value_stack,
            frame->operator_kind
        );
    }
    return evaluate_integer_default_enter_frame(
        database,
        expression_stack,
        value_stack,
        frame->expression
    );
}

static int evaluate_integer_default_apply_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
) {
    struct scalar_arithmetic_value left = {.is_null = false, .integer = 0};
    struct scalar_arithmetic_value right = {.is_null = false, .integer = 0};
    struct scalar_arithmetic_value result = {.is_null = false, .integer = 0};
    int64_t integer_result = 0;
    bool overflow = false;

    if (!scalar_arithmetic_value_stack_pop(value_stack, &right) ||
        !scalar_arithmetic_value_stack_pop(value_stack, &left)) {
        return MYLITE_ERROR;
    }
    if (left.is_null || right.is_null) {
        result.is_null = true;
        return scalar_arithmetic_value_stack_push(database, value_stack, result);
    }

    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
        overflow = checked_int64_add(left.integer, right.integer, &integer_result);
        break;
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        overflow = checked_int64_subtract(left.integer, right.integer, &integer_result);
        break;
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        overflow = checked_int64_multiply(left.integer, right.integer, &integer_result);
        break;
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        if (right.integer == 0 || (left.integer == INT64_MIN && right.integer == -1)) {
            return MYLITE_ERROR;
        }
        integer_result = left.integer % right.integer;
        break;
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
        if (right.integer == 0 || (left.integer == INT64_MIN && right.integer == -1)) {
            return MYLITE_ERROR;
        }
        integer_result = left.integer / right.integer;
        break;
    default:
        return MYLITE_ERROR;
    }
    if (overflow) {
        return MYLITE_ERROR;
    }

    result.integer = integer_result;
    return scalar_arithmetic_value_stack_push(database, value_stack, result);
}

static int evaluate_integer_default_apply_unary_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
) {
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    int64_t result = 0;

    if (!scalar_arithmetic_value_stack_pop(value_stack, &value)) {
        return MYLITE_ERROR;
    }
    if (value.is_null || operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
        return scalar_arithmetic_value_stack_push(database, value_stack, value);
    }
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE ||
        checked_int64_negate(value.integer, &result)) {
        return MYLITE_ERROR;
    }

    value.integer = result;
    return scalar_arithmetic_value_stack_push(database, value_stack, value);
}

static int evaluate_integer_default_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return evaluate_integer_default_literal_enter_frame(database, value_stack, expression);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return evaluate_integer_default_unary_enter_frame(
            database,
            expression_stack,
            value_stack,
            expression
        );
    case MYLITE_SQL_AST_MOD_FUNCTION:
        return evaluate_integer_default_mod_enter_frame(database, expression_stack, expression);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return evaluate_integer_default_binary_enter_frame(database, expression_stack, expression);
    default:
        return MYLITE_ERROR;
    }
}

static int evaluate_integer_default_literal_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    int rc = parse_integer_default_literal_value(expression, false, &value);

    if (rc == MYLITE_OK) {
        rc = scalar_arithmetic_value_stack_push(database, value_stack, value);
    }
    return rc;
}

static int evaluate_integer_default_unary_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
    const struct mylite_sql_ast_node *operand =
        unwrap_parenthesized_expression(child_at(expression, 0U));
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    int rc = MYLITE_OK;

    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        return MYLITE_ERROR;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE && operand != NULL &&
        operand->kind == MYLITE_SQL_AST_LITERAL) {
        rc = parse_integer_default_literal_value(operand, true, &value);
        if (rc == MYLITE_OK) {
            rc = scalar_arithmetic_value_stack_push(database, value_stack, value);
        }
        return rc;
    }
    rc = scalar_arithmetic_eval_stack_push(
        database,
        expression_stack,
        SCALAR_ARITHMETIC_EVAL_APPLY_UNARY,
        NULL,
        operator_kind
    );
    if (rc == MYLITE_OK) {
        rc = scalar_arithmetic_eval_stack_push(
            database,
            expression_stack,
            SCALAR_ARITHMETIC_EVAL_ENTER,
            operand,
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    return rc;
}

static int evaluate_integer_default_mod_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression
) {
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_child_count(expression) != 2U) {
        return MYLITE_ERROR;
    }
    rc = scalar_arithmetic_eval_stack_push(
        database,
        expression_stack,
        SCALAR_ARITHMETIC_EVAL_APPLY,
        NULL,
        MYLITE_SQL_AST_OPERATOR_MODULO
    );
    if (rc == MYLITE_OK) {
        rc = scalar_arithmetic_eval_stack_push(
            database,
            expression_stack,
            SCALAR_ARITHMETIC_EVAL_ENTER,
            child_at(expression, 1U),
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    if (rc == MYLITE_OK) {
        rc = scalar_arithmetic_eval_stack_push(
            database,
            expression_stack,
            SCALAR_ARITHMETIC_EVAL_ENTER,
            child_at(expression, 0U),
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    return rc;
}

static int evaluate_integer_default_binary_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
    int rc = MYLITE_OK;

    if (!integer_default_binary_operator_is_supported(operator_kind)) {
        return MYLITE_ERROR;
    }
    rc = scalar_arithmetic_eval_stack_push(
        database,
        expression_stack,
        SCALAR_ARITHMETIC_EVAL_APPLY,
        NULL,
        operator_kind
    );
    if (rc == MYLITE_OK) {
        rc = scalar_arithmetic_eval_stack_push(
            database,
            expression_stack,
            SCALAR_ARITHMETIC_EVAL_ENTER,
            child_at(expression, 1U),
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    if (rc == MYLITE_OK) {
        rc = scalar_arithmetic_eval_stack_push(
            database,
            expression_stack,
            SCALAR_ARITHMETIC_EVAL_ENTER,
            child_at(expression, 0U),
            MYLITE_SQL_AST_OPERATOR_NONE
        );
    }
    return rc;
}

static bool integer_default_binary_operator_is_supported(
    enum mylite_sql_ast_operator operator_kind
) {
    return (operator_kind == MYLITE_SQL_AST_OPERATOR_ADD ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_SUBTRACT ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_MULTIPLY ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_MODULO ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE) != 0;
}

static int finish_integer_default_result(
    struct scalar_arithmetic_value_stack *value_stack,
    struct scalar_arithmetic_value *out_value
) {
    if (!scalar_arithmetic_value_stack_pop(value_stack, out_value)) {
        return MYLITE_ERROR;
    }
    if (value_stack->count != 0U) {
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int parse_integer_default_literal_value(
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    struct scalar_arithmetic_value *out_value
) {
    const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    uint64_t limit = (uint64_t)INT64_MAX;
    uint64_t magnitude = 0U;

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_value->is_null = true;
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_ERROR;
    }
    if (is_negative) {
        limit = int64_min_magnitude;
    }
    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        magnitude > limit) {
        return MYLITE_ERROR;
    }
    if (is_negative && magnitude == int64_min_magnitude) {
        out_value->integer = INT64_MIN;
    } else if (is_negative && magnitude != 0U) {
        out_value->integer = -(int64_t)magnitude;
    } else {
        out_value->integer = (int64_t)magnitude;
    }
    return MYLITE_OK;
}

static bool column_default_value_is_parenthesized_expression(
    const struct mylite_sql_ast_node *default_node
) {
    const struct mylite_sql_ast_node *value_node = child_at(default_node, 0U);

    return (value_node != NULL && value_node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) != 0;
}

static bool column_default_value_is_parenthesized_text_expression(
    const struct mylite_sql_ast_node *default_node
) {
    const struct mylite_sql_ast_node *value_node = child_at(default_node, 0U);
    const struct mylite_sql_ast_node *expression = unwrap_parenthesized_expression(value_node);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_INTEGER;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION ||
        expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    return (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
}

static bool column_default_value_is_parenthesized_binary_blob_expression(
    const struct mylite_sql_ast_node *default_node
) {
    const struct mylite_sql_ast_node *value_node = child_at(default_node, 0U);
    const struct mylite_sql_ast_node *expression = unwrap_parenthesized_expression(value_node);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_INTEGER;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION ||
        expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    return (literal_kind == MYLITE_SQL_AST_LITERAL_HEX ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
}

static int check_duplicate_column_names(
    struct mylite_db *database,
    const struct planned_column *columns,
    size_t column_count
) {
    for (size_t left = 0U; left < column_count; ++left) {
        for (size_t right = left + 1U; right < column_count; ++right) {
            if (text_equals_ascii_case_insensitive(columns[left].name, columns[right].name)) {
                set_duplicate_column_error(database, columns[right].name);
                return MYLITE_ERROR;
            }
        }
    }

    return MYLITE_OK;
}

static bool text_equals_ascii_case_insensitive(const char *left, const char *right) {
    size_t index = 0U;

    if (left == NULL || right == NULL) {
        return false;
    }
    while (left[index] != '\0' && right[index] != '\0') {
        if (ascii_lower((unsigned char)left[index]) != ascii_lower((unsigned char)right[index])) {
            return false;
        }
        ++index;
    }

    if (left[index] != '\0') {
        return false;
    }
    if (right[index] != '\0') {
        return false;
    }

    return true;
}

static bool text_has_ascii_case_insensitive_prefix(const char *text, const char *prefix) {
    size_t index = 0U;

    if (text == NULL || prefix == NULL) {
        return false;
    }
    while (prefix[index] != '\0') {
        if (text[index] == '\0' ||
            ascii_lower((unsigned char)text[index]) != ascii_lower((unsigned char)prefix[index])) {
            return false;
        }
        ++index;
    }

    return true;
}

static char ascii_lower(unsigned char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte + ('a' - 'A'));
    }

    return (char)byte;
}

static bool enum_label_equals_ascii_case_insensitive(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length
) {
    if (left == NULL || right == NULL || left_length != right_length) {
        return false;
    }
    for (size_t index = 0U; index < left_length; ++index) {
        if (ascii_lower((unsigned char)left[index]) != ascii_lower((unsigned char)right[index])) {
            return false;
        }
    }
    return true;
}

static int map_column_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    if (type_node == NULL || out_column == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (type_node->kind == MYLITE_SQL_AST_INTEGER_TYPE) {
        return map_integer_type(
            database,
            type_node,
            column_name,
            &out_column->logical_type,
            &out_column->physical_type
        );
    }
    if (type_node->kind == MYLITE_SQL_AST_VARCHAR_TYPE) {
        return map_varchar_type(database, type_node, column_name, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_CHAR_TYPE) {
        (void)column_name;
        return map_char_type(database, type_node, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_TEXT_TYPE) {
        return map_text_family_type(database, column_name, type_node, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_JSON_TYPE) {
        (void)database;
        (void)column_name;
        return map_json_type(out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_SPATIAL_TYPE) {
        (void)column_name;
        return map_spatial_type(database, type_node, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_ENUM_TYPE) {
        return map_enum_type(database, type_node, column_name, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_SET_TYPE) {
        return map_set_type(database, type_node, column_name, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_BINARY_STRING_TYPE) {
        return map_binary_string_type(database, type_node, column_name, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_BIT_TYPE) {
        return map_bit_type(database, type_node, column_name, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_YEAR_TYPE) {
        return map_year_type(database, type_node, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_DECIMAL_TYPE) {
        return map_decimal_type(database, type_node, column_name, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_APPROXIMATE_TYPE) {
        return map_approximate_type(database, type_node, column_name, out_column);
    }
    if (ast_node_is_temporal_column_type(type_node)) {
        return map_temporal_column_type(database, type_node, column_name, out_column);
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static bool ast_node_is_temporal_column_type(const struct mylite_sql_ast_node *type_node) {
    if (type_node == NULL) {
        return false;
    }

    return (type_node->kind == MYLITE_SQL_AST_DATE_TYPE ||
            type_node->kind == MYLITE_SQL_AST_TIME_TYPE ||
            type_node->kind == MYLITE_SQL_AST_DATETIME_TYPE ||
            type_node->kind == MYLITE_SQL_AST_TIMESTAMP_TYPE) != 0;
}

static int map_temporal_column_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    const char *logical_type = "DATE";

    (void)database;
    (void)column_name;

    if (type_node->kind == MYLITE_SQL_AST_TIME_TYPE) {
        logical_type = "TIME";
    } else if (type_node->kind == MYLITE_SQL_AST_DATETIME_TYPE) {
        logical_type = "DATETIME";
    } else if (type_node->kind == MYLITE_SQL_AST_TIMESTAMP_TYPE) {
        logical_type = "TIMESTAMP";
    }

    snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "%s",
        logical_type
    );
    snprintf(out_column->physical_type_storage, sizeof(out_column->physical_type_storage), "TEXT");
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static int map_varchar_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    struct mylite_sql_source_span length_span =
        mylite_sql_ast_node_varchar_type_length_span(type_node);
    uint64_t length = 0U;
    bool is_national = mylite_sql_ast_node_varchar_type_is_national(type_node) != 0;
    int rc = MYLITE_OK;
    int written = 0;

    rc = parse_unsigned_integer_literal(&length_span, &length);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "VARCHAR supports only lengths 0 through 16383");
        return MYLITE_ERROR;
    }
    if (length > varchar_max_length) {
        set_column_length_too_big_error(database, column_name, varchar_max_length);
        return MYLITE_ERROR;
    }

    written = snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "%s(%" PRIu64 ")",
        is_national ? "NVARCHAR" : "VARCHAR",
        length
    );
    if (written < 0 || (size_t)written >= sizeof(out_column->logical_type_storage)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    snprintf(
        out_column->physical_type_storage,
        sizeof(out_column->physical_type_storage),
        "%s",
        "TEXT"
    );
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;

    if (is_national) {
        rc = append_national_character_set_warning(database);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int map_char_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    struct planned_column *out_column
) {
    uint64_t length = char_default_length;
    bool is_national = mylite_sql_ast_node_char_type_is_national(type_node) != 0;
    int rc = MYLITE_OK;
    int written = 0;

    if (mylite_sql_ast_node_char_type_has_explicit_length(type_node)) {
        struct mylite_sql_source_span length_span =
            mylite_sql_ast_node_char_type_length_span(type_node);

        rc = parse_unsigned_integer_literal(&length_span, &length);
        if (rc != MYLITE_OK) {
            set_unsupported_error(database, "CHAR supports only lengths 0 through 255");
            return MYLITE_ERROR;
        }
    }
    if (length > char_max_length) {
        set_unsupported_error(database, "CHAR supports only lengths 0 through 255");
        return MYLITE_ERROR;
    }

    written = snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "%s(%" PRIu64 ")",
        is_national ? "NCHAR" : "CHAR",
        length
    );
    if (written < 0 || (size_t)written >= sizeof(out_column->logical_type_storage)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    snprintf(
        out_column->physical_type_storage,
        sizeof(out_column->physical_type_storage),
        "%s",
        "TEXT"
    );
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;

    if (is_national) {
        rc = append_national_character_set_warning(database);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int append_national_character_set_warning(struct mylite_db *database) {
    int rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_national_character_set_deprecated,
        "HY000",
        national_character_set_warning_message
    );

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int map_text_family_type(
    struct mylite_db *database,
    const char *column_name,
    const struct mylite_sql_ast_node *type_node,
    struct planned_column *out_column
) {
    enum mylite_sql_ast_text_type text_type = mylite_sql_ast_node_text_type(type_node);
    const char *logical_type = NULL;
    int rc = MYLITE_OK;

    switch (text_type) {
    case MYLITE_SQL_AST_TEXT_TYPE_NONE:
        break;
    case MYLITE_SQL_AST_TEXT_TYPE_TINYTEXT:
        logical_type = "TINYTEXT";
        break;
    case MYLITE_SQL_AST_TEXT_TYPE_TEXT:
        logical_type = "TEXT";
        break;
    case MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT:
        logical_type = "MEDIUMTEXT";
        break;
    case MYLITE_SQL_AST_TEXT_TYPE_LONGTEXT:
        logical_type = "LONGTEXT";
        break;
    }
    if (logical_type == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = assign_text_family_descriptor_type(database, out_column, logical_type);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_sql_ast_node_text_type_has_length(type_node) != 0) {
        struct mylite_sql_source_span length_span =
            mylite_sql_ast_node_text_type_length_span(type_node);
        uint64_t length = 0U;

        rc = parse_unsigned_integer_literal(&length_span, &length);
        if (rc != MYLITE_OK || length > longtext_max_length) {
            set_text_display_width_out_of_range_error(database, column_name);
            return MYLITE_ERROR;
        }
        out_column->has_text_length_argument = true;
        out_column->text_length_argument = length;
    }

    return MYLITE_OK;
}

static int assign_text_family_descriptor_type(
    struct mylite_db *database,
    struct planned_column *out_column,
    const char *logical_type
) {
    int written = snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "%s",
        logical_type == NULL ? "" : logical_type
    );

    if (written < 0 || (size_t)written >= sizeof(out_column->logical_type_storage)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    snprintf(out_column->physical_type_storage, sizeof(out_column->physical_type_storage), "TEXT");
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static const char *text_family_logical_type_for_length(
    uint64_t character_length,
    uint64_t max_bytes_per_character
) {
    uint64_t byte_requirement =
        saturating_text_byte_requirement(character_length, max_bytes_per_character);

    if (byte_requirement <= tinytext_max_length) {
        return "TINYTEXT";
    }
    if (byte_requirement <= text_max_length) {
        return "TEXT";
    }
    if (byte_requirement <= mediumtext_max_length) {
        return "MEDIUMTEXT";
    }
    return "LONGTEXT";
}

static uint64_t saturating_text_byte_requirement(
    uint64_t character_length,
    uint64_t max_bytes_per_character
) {
    if (max_bytes_per_character == 0U) {
        max_bytes_per_character = utf8mb4_max_bytes_per_character;
    }
    if (character_length > longtext_max_length / max_bytes_per_character) {
        return longtext_max_length;
    }
    return character_length * max_bytes_per_character;
}

static uint64_t max_bytes_per_character_for_charset(const char *charset_name) {
    const struct mylite_execution_catalog_character_set *charset = NULL;
    struct mylite_sql_source_span maxlen_span = {0};
    uint64_t maxlen = 0U;

    if (charset_name == NULL || charset_name[0] == '\0') {
        return utf8mb4_max_bytes_per_character;
    }
    if (charset_name_is_binary(charset_name)) {
        return 1U;
    }

    charset = character_set_by_name(charset_name);
    if (charset == NULL) {
        return utf8mb4_max_bytes_per_character;
    }
    maxlen_span = (struct mylite_sql_source_span){
        .text = charset->maxlen,
        .length = strlen(charset->maxlen),
    };
    if (parse_unsigned_integer_literal(&maxlen_span, &maxlen) == MYLITE_OK && maxlen > 0U) {
        return maxlen;
    }

    return utf8mb4_max_bytes_per_character;
}

static int map_json_type(struct planned_column *out_column) {
    snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "%s",
        "JSON"
    );
    snprintf(
        out_column->physical_type_storage,
        sizeof(out_column->physical_type_storage),
        "%s",
        "TEXT"
    );
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static int map_spatial_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    struct planned_column *out_column
) {
    const char *logical_type =
        mylite_sql_ast_spatial_type_name(mylite_sql_ast_node_spatial_type(type_node));

    if (strcmp(logical_type, "none") == 0 || strcmp(logical_type, "unknown") == 0) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "%s",
        logical_type
    );
    snprintf(
        out_column->physical_type_storage,
        sizeof(out_column->physical_type_storage),
        "%s",
        "BLOB"
    );
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static int map_enum_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    const struct mylite_sql_ast_node *label_list = child_at(type_node, 0U);
    struct enum_type_info info = {0};
    struct mylite_dynamic_string descriptor;
    int rc = MYLITE_OK;

    mylite_dynamic_string_init(&descriptor);
    if (label_list == NULL || label_list->kind != MYLITE_SQL_AST_ENUM_LABEL_LIST ||
        mylite_sql_ast_node_child_count(label_list) == 0U) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&descriptor, "ENUM(");
    }
    for (size_t label_index = 0U;
         rc == MYLITE_OK && label_index < mylite_sql_ast_node_child_count(label_list);
         ++label_index) {
        rc = map_enum_label(
            database,
            column_name,
            child_at(label_list, label_index),
            label_index,
            &info,
            &descriptor
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&descriptor, ')');
    }
    if (rc == MYLITE_OK && (descriptor.text == NULL ||
                            descriptor.length >= sizeof(out_column->logical_type_storage))) {
        set_unsupported_error(database, "ENUM definition is too large for this MyLite build");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        memcpy(out_column->logical_type_storage, descriptor.text, descriptor.length + 1U);
        snprintf(
            out_column->physical_type_storage,
            sizeof(out_column->physical_type_storage),
            "TEXT"
        );
        out_column->logical_type = out_column->logical_type_storage;
        out_column->physical_type = out_column->physical_type_storage;
    }

    mylite_dynamic_string_deinit(&descriptor);
    return rc;
}

static int map_enum_label(
    struct mylite_db *database,
    const char *column_name,
    const struct mylite_sql_ast_node *label_node,
    size_t label_index,
    struct enum_type_info *info,
    struct mylite_dynamic_string *descriptor
) {
    char *text = NULL;
    size_t text_length = 0U;
    size_t character_length = 0U;
    int rc = MYLITE_OK;

    if (info->label_count >= enum_label_count_capacity) {
        set_unsupported_error(database, "ENUM supports at most 255 labels in this build");
        return MYLITE_ERROR;
    }
    rc = decode_sql_string_literal(
        database,
        label_node,
        "ENUM labels support only string literals",
        "ENUM labels do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK) {
        trim_trailing_ascii_spaces(text, &text_length);
    }
    if (rc == MYLITE_OK && validate_utf8_text(text, text_length, &character_length) != MYLITE_OK) {
        set_unsupported_error(database, "ENUM labels must be valid UTF-8");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = check_enum_label_unique(database, text, text_length, info, column_name);
    }
    if (rc == MYLITE_OK) {
        rc = store_enum_label(database, text, text_length, character_length, info);
    }
    if (rc == MYLITE_OK) {
        rc = append_enum_descriptor_label(descriptor, text, text_length, label_index != 0U);
    }

    free(text);
    return rc;
}

static int check_enum_label_unique(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct enum_type_info *info,
    const char *column_name
) {
    for (size_t previous = 0U; previous < info->label_count; ++previous) {
        if (enum_label_equals_ascii_case_insensitive(
                info->labels[previous].text,
                info->labels[previous].text_length,
                text,
                text_length
            )) {
            set_duplicated_enum_value_error(
                database,
                column_name,
                info->labels[previous].text,
                info->labels[previous].text_length
            );
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static int store_enum_label(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    size_t character_length,
    struct enum_type_info *info
) {
    char *stored = NULL;

    if (info->decoded_length + text_length + 1U > sizeof(info->decoded_storage)) {
        set_unsupported_error(database, "ENUM definition is too large for this MyLite build");
        return MYLITE_ERROR;
    }

    stored = &info->decoded_storage[info->decoded_length];
    memcpy(stored, text, text_length);
    stored[text_length] = '\0';
    info->labels[info->label_count] = (struct enum_label_descriptor){
        .text = stored,
        .text_length = text_length,
        .character_length = character_length,
    };
    ++info->label_count;
    info->decoded_length += text_length + 1U;
    if (info->max_label_character_length < character_length) {
        info->max_label_character_length = character_length;
    }
    return MYLITE_OK;
}

static int append_enum_descriptor_label(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_length,
    bool needs_comma
) {
    int rc = MYLITE_OK;

    if (needs_comma) {
        rc = mylite_dynamic_string_append_char(string, ',');
    }
    if (rc == MYLITE_OK) {
        rc = append_enum_quoted_label(string, text, text_length);
    }
    return rc;
}

static int append_enum_quoted_label(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_length
) {
    int rc = mylite_dynamic_string_append_char(string, '\'');

    for (size_t index = 0U; rc == MYLITE_OK && index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (byte == '\'') {
            rc = mylite_dynamic_string_append(string, "''");
        } else if (byte == '\\') {
            rc = mylite_dynamic_string_append(string, "\\\\");
        } else if (byte == '\n') {
            rc = mylite_dynamic_string_append(string, "\\n");
        } else if (byte == '\r') {
            rc = mylite_dynamic_string_append(string, "\\r");
        } else if (byte == '\t') {
            rc = mylite_dynamic_string_append(string, "\\t");
        } else {
            rc = mylite_dynamic_string_append_char(string, (char)byte);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }
    return rc;
}

static int map_set_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    const struct mylite_sql_ast_node *member_list = child_at(type_node, 0U);
    struct set_type_info info = {0};
    struct mylite_dynamic_string descriptor;
    int rc = MYLITE_OK;

    mylite_dynamic_string_init(&descriptor);
    if (member_list == NULL || member_list->kind != MYLITE_SQL_AST_SET_MEMBER_LIST ||
        mylite_sql_ast_node_child_count(member_list) == 0U) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&descriptor, "SET(");
    }
    for (size_t member_index = 0U;
         rc == MYLITE_OK && member_index < mylite_sql_ast_node_child_count(member_list);
         ++member_index) {
        rc = map_set_member(
            database,
            column_name,
            child_at(member_list, member_index),
            member_index,
            &info,
            &descriptor
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&descriptor, ')');
    }
    if (rc == MYLITE_OK && (descriptor.text == NULL ||
                            descriptor.length >= sizeof(out_column->logical_type_storage))) {
        set_unsupported_error(database, "SET definition is too large for this MyLite build");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        memcpy(out_column->logical_type_storage, descriptor.text, descriptor.length + 1U);
        snprintf(
            out_column->physical_type_storage,
            sizeof(out_column->physical_type_storage),
            "TEXT"
        );
        out_column->logical_type = out_column->logical_type_storage;
        out_column->physical_type = out_column->physical_type_storage;
    }

    mylite_dynamic_string_deinit(&descriptor);
    return rc;
}

static int map_set_member(
    struct mylite_db *database,
    const char *column_name,
    const struct mylite_sql_ast_node *member_node,
    size_t member_index,
    struct set_type_info *info,
    struct mylite_dynamic_string *descriptor
) {
    char *text = NULL;
    size_t text_length = 0U;
    size_t character_length = 0U;
    int rc = MYLITE_OK;

    if (info->member_count >= set_member_count_capacity) {
        set_unsupported_error(database, "SET supports at most 64 members in this build");
        return MYLITE_ERROR;
    }
    rc = decode_sql_string_literal(
        database,
        member_node,
        "SET members support only string literals",
        "SET members do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK) {
        trim_trailing_ascii_spaces(text, &text_length);
    }
    if (rc == MYLITE_OK && text_length == 0U) {
        set_unsupported_error(database, "SET empty-string members are not yet supported");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && set_member_contains_comma(text, text_length)) {
        set_illegal_set_value_error(database, text, text_length);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && validate_utf8_text(text, text_length, &character_length) != MYLITE_OK) {
        set_unsupported_error(database, "SET members must be valid UTF-8");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = check_set_member_unique(database, text, text_length, info, column_name);
    }
    if (rc == MYLITE_OK) {
        rc = store_set_member(database, text, text_length, character_length, info);
    }
    if (rc == MYLITE_OK) {
        rc = append_enum_descriptor_label(descriptor, text, text_length, member_index != 0U);
    }

    free(text);
    return rc;
}

static int check_set_member_unique(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct set_type_info *info,
    const char *column_name
) {
    for (size_t previous = 0U; previous < info->member_count; ++previous) {
        if (enum_label_equals_ascii_case_insensitive(
                info->members[previous].text,
                info->members[previous].text_length,
                text,
                text_length
            )) {
            set_duplicated_set_value_error(
                database,
                column_name,
                info->members[previous].text,
                info->members[previous].text_length
            );
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static bool set_member_contains_comma(const char *text, size_t text_length) {
    if (text == NULL) {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        if (text[index] == ',') {
            return true;
        }
    }
    return false;
}

static int store_set_member(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    size_t character_length,
    struct set_type_info *info
) {
    char *stored = NULL;

    if (info->decoded_length + text_length + 1U > sizeof(info->decoded_storage)) {
        set_unsupported_error(database, "SET definition is too large for this MyLite build");
        return MYLITE_ERROR;
    }

    stored = &info->decoded_storage[info->decoded_length];
    memcpy(stored, text, text_length);
    stored[text_length] = '\0';
    info->members[info->member_count] = (struct enum_label_descriptor){
        .text = stored,
        .text_length = text_length,
        .character_length = character_length,
    };
    ++info->member_count;
    info->decoded_length += text_length + 1U;
    info->max_display_character_length += character_length;
    if (info->member_count > 1U) {
        ++info->max_display_character_length;
    }
    return MYLITE_OK;
}

static int map_binary_string_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    switch (mylite_sql_ast_node_binary_string_type(type_node)) {
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_NONE:
        break;
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_BINARY:
        return map_binary_type(database, type_node, column_name, out_column);
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_VARBINARY:
        return map_varbinary_type(database, type_node, column_name, out_column);
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_TINYBLOB:
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_BLOB:
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_MEDIUMBLOB:
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_LONGBLOB:
        return map_blob_family_type(database, type_node, column_name, out_column);
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int map_binary_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    uint64_t length = binary_default_length;
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_binary_string_type_has_length(type_node) != 0) {
        struct mylite_sql_source_span length_span =
            mylite_sql_ast_node_binary_string_type_length_span(type_node);

        rc = parse_unsigned_integer_literal(&length_span, &length);
        if (rc != MYLITE_OK) {
            set_unsupported_error(database, "BINARY supports only lengths 0 through 255");
            return MYLITE_ERROR;
        }
    }
    if (length > binary_max_length) {
        set_column_length_too_big_error(database, column_name, binary_max_length);
        return MYLITE_ERROR;
    }

    return assign_binary_descriptor_type(database, out_column, "BINARY(%" PRIu64 ")", length);
}

static int map_varbinary_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    struct mylite_sql_source_span length_span =
        mylite_sql_ast_node_binary_string_type_length_span(type_node);
    uint64_t length = 0U;
    int rc = parse_unsigned_integer_literal(&length_span, &length);

    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "VARBINARY supports only lengths 0 through 65535");
        return MYLITE_ERROR;
    }
    if (length > varbinary_max_length) {
        set_column_length_too_big_error(database, column_name, varbinary_max_length);
        return MYLITE_ERROR;
    }
    if (length > varbinary_row_size_max_length) {
        set_row_size_too_large_error(database);
        return MYLITE_ERROR;
    }

    return assign_binary_descriptor_type(database, out_column, "VARBINARY(%" PRIu64 ")", length);
}

static int map_blob_family_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    enum mylite_sql_ast_binary_string_type binary_type =
        mylite_sql_ast_node_binary_string_type(type_node);
    const uint64_t mediumblob_max_length = 16777215ULL;
    const uint64_t longblob_max_length = 4294967295ULL;
    const char *logical_type = NULL;
    int rc = MYLITE_OK;

    if (binary_type == MYLITE_SQL_AST_BINARY_STRING_TYPE_BLOB &&
        mylite_sql_ast_node_binary_string_type_has_length(type_node) != 0) {
        struct mylite_sql_source_span length_span =
            mylite_sql_ast_node_binary_string_type_length_span(type_node);
        uint64_t length = 0U;

        rc = parse_unsigned_integer_literal(&length_span, &length);
        if (rc != MYLITE_OK || length > longblob_max_length) {
            set_column_length_too_big_error(database, column_name, longblob_max_length);
            return MYLITE_ERROR;
        }
        if (length <= binary_max_length) {
            logical_type = "TINYBLOB";
        } else if (length <= varbinary_max_length) {
            logical_type = "BLOB";
        } else if (length <= mediumblob_max_length) {
            logical_type = "MEDIUMBLOB";
        } else {
            logical_type = "LONGBLOB";
        }
    } else {
        switch (binary_type) {
        case MYLITE_SQL_AST_BINARY_STRING_TYPE_TINYBLOB:
            logical_type = "TINYBLOB";
            break;
        case MYLITE_SQL_AST_BINARY_STRING_TYPE_BLOB:
            logical_type = "BLOB";
            break;
        case MYLITE_SQL_AST_BINARY_STRING_TYPE_MEDIUMBLOB:
            logical_type = "MEDIUMBLOB";
            break;
        case MYLITE_SQL_AST_BINARY_STRING_TYPE_LONGBLOB:
            logical_type = "LONGBLOB";
            break;
        case MYLITE_SQL_AST_BINARY_STRING_TYPE_NONE:
        case MYLITE_SQL_AST_BINARY_STRING_TYPE_BINARY:
        case MYLITE_SQL_AST_BINARY_STRING_TYPE_VARBINARY:
            break;
        }
    }
    if (logical_type == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "%s",
        logical_type
    );
    snprintf(out_column->physical_type_storage, sizeof(out_column->physical_type_storage), "BLOB");
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static int assign_binary_descriptor_type(
    struct mylite_db *database,
    struct planned_column *out_column,
    const char *format,
    uint64_t length
) {
    int written = snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        format,
        length
    );

    if (written < 0 || (size_t)written >= sizeof(out_column->logical_type_storage)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    snprintf(out_column->physical_type_storage, sizeof(out_column->physical_type_storage), "BLOB");
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static int map_bit_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    uint64_t length = bit_default_length;
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_bit_type_has_length(type_node) != 0) {
        struct mylite_sql_source_span length_span =
            mylite_sql_ast_node_bit_type_length_span(type_node);

        rc = parse_unsigned_integer_literal(&length_span, &length);
        if (rc != MYLITE_OK) {
            set_bit_display_width_out_of_range_error(database, column_name);
            return MYLITE_ERROR;
        }
    }
    if (length == 0U) {
        set_invalid_column_size_error(database, column_name);
        return MYLITE_ERROR;
    }
    if (length > bit_max_length) {
        set_bit_display_width_out_of_range_error(database, column_name);
        return MYLITE_ERROR;
    }

    return assign_bit_descriptor_type(database, out_column, length);
}

static int map_year_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    struct planned_column *out_column
) {
    if (mylite_sql_ast_node_year_type_has_width(type_node) != 0) {
        struct mylite_sql_source_span width_span =
            mylite_sql_ast_node_year_type_width_span(type_node);
        uint64_t width = 0U;
        int rc = parse_unsigned_integer_literal(&width_span, &width);

        if (rc != MYLITE_OK || width != 4U) {
            set_invalid_year_display_width_error(database);
            return MYLITE_ERROR;
        }
        rc = append_year_display_width_warning(database);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    snprintf(out_column->logical_type_storage, sizeof(out_column->logical_type_storage), "YEAR");
    snprintf(out_column->physical_type_storage, sizeof(out_column->physical_type_storage), "TEXT");
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static int assign_bit_descriptor_type(
    struct mylite_db *database,
    struct planned_column *out_column,
    uint64_t length
) {
    int written = snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "BIT(%" PRIu64 ")",
        length
    );

    if (written < 0 || (size_t)written >= sizeof(out_column->logical_type_storage)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    snprintf(out_column->physical_type_storage, sizeof(out_column->physical_type_storage), "BLOB");
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static int map_decimal_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    uint64_t precision = decimal_default_precision;
    uint64_t scale = decimal_default_scale;
    int written = 0;
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_decimal_type_has_precision(type_node) != 0) {
        struct mylite_sql_source_span precision_span =
            mylite_sql_ast_node_decimal_type_precision_span(type_node);

        rc = parse_unsigned_integer_literal(&precision_span, &precision);
        if (rc != MYLITE_OK) {
            set_decimal_precision_too_big_error(database, column_name, precision);
            return MYLITE_ERROR;
        }
        if (mylite_sql_ast_node_decimal_type_has_scale(type_node) != 0) {
            struct mylite_sql_source_span scale_span =
                mylite_sql_ast_node_decimal_type_scale_span(type_node);

            rc = parse_unsigned_integer_literal(&scale_span, &scale);
            if (rc != MYLITE_OK) {
                set_decimal_scale_too_big_error(database, column_name, scale);
                return MYLITE_ERROR;
            }
        } else {
            scale = decimal_default_scale;
        }
    }
    if (precision == 0U && scale == 0U) {
        precision = decimal_default_precision;
    }
    if (precision > decimal_max_precision) {
        set_decimal_precision_too_big_error(database, column_name, precision);
        return MYLITE_ERROR;
    }
    if (scale > decimal_max_scale) {
        set_decimal_scale_too_big_error(database, column_name, scale);
        return MYLITE_ERROR;
    }
    if (scale > precision) {
        set_decimal_scale_greater_than_precision_error(database, column_name);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_decimal_type_is_unsigned(type_node) != 0) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_decimal_unsigned_deprecated,
            "HY000",
            "UNSIGNED for decimal and floating point data types is deprecated and support for it "
            "will be removed in a future release."
        );
        if (rc != MYLITE_OK) {
            if (rc == MYLITE_NOMEM) {
                set_nomem_error(database);
            }
            return rc;
        }
    }

    written = snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "DECIMAL(%" PRIu64 ",%" PRIu64 ")%s",
        precision,
        scale,
        mylite_sql_ast_node_decimal_type_is_unsigned(type_node) != 0 ? " UNSIGNED" : ""
    );
    if (written < 0 || (size_t)written >= sizeof(out_column->logical_type_storage)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    snprintf(
        out_column->physical_type_storage,
        sizeof(out_column->physical_type_storage),
        "%s",
        "TEXT"
    );
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static int map_approximate_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    enum approximate_type_class type_class = APPROXIMATE_TYPE_FLOAT;
    uint64_t precision = 0U;
    int rc = MYLITE_OK;
    int written = 0;

    switch (mylite_sql_ast_node_approximate_type(type_node)) {
    case MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT:
    case MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT4:
        type_class = APPROXIMATE_TYPE_FLOAT;
        break;
    case MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT8:
    case MYLITE_SQL_AST_APPROXIMATE_TYPE_DOUBLE:
        type_class = APPROXIMATE_TYPE_DOUBLE;
        break;
    case MYLITE_SQL_AST_APPROXIMATE_TYPE_REAL:
        if (session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_REAL_AS_FLOAT)) {
            type_class = APPROXIMATE_TYPE_FLOAT;
        } else {
            type_class = APPROXIMATE_TYPE_DOUBLE;
        }
        break;
    }

    if (mylite_sql_ast_node_approximate_type_has_precision(type_node) != 0) {
        struct mylite_sql_source_span precision_span =
            mylite_sql_ast_node_approximate_type_precision_span(type_node);

        rc = parse_unsigned_integer_literal(&precision_span, &precision);
        if (rc != MYLITE_OK || precision > float_max_precision) {
            set_incorrect_column_specifier_error(database, column_name);
            return MYLITE_ERROR;
        }
        type_class = precision <= float_double_precision_threshold ? APPROXIMATE_TYPE_FLOAT
                                                                   : APPROXIMATE_TYPE_DOUBLE;
    }
    if (mylite_sql_ast_node_approximate_type_is_unsigned(type_node) != 0) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_decimal_unsigned_deprecated,
            "HY000",
            "UNSIGNED for decimal and floating point data types is deprecated and support for it "
            "will be removed in a future release."
        );
        if (rc != MYLITE_OK) {
            if (rc == MYLITE_NOMEM) {
                set_nomem_error(database);
            }
            return rc;
        }
    }

    const char *base_type = type_class == APPROXIMATE_TYPE_FLOAT ? "FLOAT" : "DOUBLE";

    written = snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "%s%s",
        base_type,
        mylite_sql_ast_node_approximate_type_is_unsigned(type_node) != 0 ? " UNSIGNED" : ""
    );
    if (written < 0 || (size_t)written >= sizeof(out_column->logical_type_storage)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    snprintf(out_column->physical_type_storage, sizeof(out_column->physical_type_storage), "REAL");
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static int map_integer_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    const char **out_logical_type,
    const char **out_physical_type
) {
    enum mylite_sql_ast_integer_type type = mylite_sql_ast_node_integer_type(type_node);
    int is_unsigned = mylite_sql_ast_node_integer_type_is_unsigned(type_node);
    bool has_display_width = false;
    uint64_t display_width = 0U;
    const char *logical_type = NULL;
    int rc = MYLITE_OK;

    if (out_logical_type == NULL || out_physical_type == NULL) {
        return MYLITE_MISUSE;
    }

    rc = map_integer_display_width(
        database,
        type_node,
        column_name,
        &has_display_width,
        &display_width
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    logical_type = logical_type_for_mapped_integer((struct mapped_integer_type){
        .type = type,
        .is_unsigned = is_unsigned,
        .has_display_width = has_display_width,
        .is_bool_alias = mylite_sql_ast_node_integer_type_is_bool_alias(type_node) != 0,
        .display_width = display_width,
    });
    if (logical_type == NULL) {
        return MYLITE_ERROR;
    }
    *out_logical_type = logical_type;
    *out_physical_type = "INTEGER";

    return MYLITE_OK;
}

static int map_integer_display_width(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    bool *out_has_display_width,
    uint64_t *out_display_width
) {
    enum { max_integer_display_width = 255 };

    struct mylite_sql_source_span display_width_span = {0};
    int rc = MYLITE_OK;

    *out_has_display_width = mylite_sql_ast_node_integer_type_has_display_width(type_node) != 0;
    *out_display_width = 0U;
    if (!*out_has_display_width) {
        return MYLITE_OK;
    }

    display_width_span = mylite_sql_ast_node_integer_type_display_width_span(type_node);
    rc = parse_unsigned_integer_literal(&display_width_span, out_display_width);
    if (rc != MYLITE_OK || *out_display_width > max_integer_display_width) {
        set_display_width_out_of_range_error(database, column_name);
        return MYLITE_ERROR;
    }

    rc = append_integer_display_width_warning(database);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }

    return rc;
}

static int append_integer_display_width_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_integer_display_width_deprecated,
        "HY000",
        "Integer display width is deprecated and will be removed in a future release."
    );
}

static int append_year_display_width_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_year_display_width_deprecated,
        "HY000",
        "'YEAR(4)' is deprecated and will be removed in a future release. Please use YEAR instead"
    );
}

static const char *logical_type_for_mapped_integer(struct mapped_integer_type integer_type) {
    switch (integer_type.type) {
    case MYLITE_SQL_AST_INTEGER_TYPE_NONE:
        return NULL;
    case MYLITE_SQL_AST_INTEGER_TYPE_TINYINT:
        if (integer_type.is_unsigned != 0) {
            return "TINYINT UNSIGNED";
        }
        if (integer_type.is_bool_alias) {
            return "TINYINT(1)";
        }
        if (integer_type.has_display_width && integer_type.display_width == 1U) {
            return "TINYINT(1)";
        }
        return "TINYINT";
    case MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT:
        return integer_type.is_unsigned == 0 ? "SMALLINT" : "SMALLINT UNSIGNED";
    case MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT:
        return integer_type.is_unsigned == 0 ? "MEDIUMINT" : "MEDIUMINT UNSIGNED";
    case MYLITE_SQL_AST_INTEGER_TYPE_INT:
        return integer_type.is_unsigned == 0 ? "INT" : "INT UNSIGNED";
    case MYLITE_SQL_AST_INTEGER_TYPE_BIGINT:
        return integer_type.is_unsigned == 0 ? "BIGINT" : "BIGINT UNSIGNED";
    }

    return NULL;
}

static bool logical_type_is_varchar(const char *logical_type) {
    return (logical_type != NULL &&
            (strncmp(logical_type, "VARCHAR(", varchar_logical_prefix_length) == 0 ||
             strncmp(logical_type, "NVARCHAR(", nvarchar_logical_prefix_length) == 0)) != 0;
}

static bool logical_type_is_char(const char *logical_type) {
    return (logical_type != NULL &&
            (strncmp(logical_type, "CHAR(", char_logical_prefix_length) == 0 ||
             strncmp(logical_type, "NCHAR(", nchar_logical_prefix_length) == 0)) != 0;
}

static bool logical_type_is_national_varchar(const char *logical_type) {
    return (logical_type != NULL &&
            strncmp(logical_type, "NVARCHAR(", nvarchar_logical_prefix_length) == 0) != 0;
}

static bool logical_type_is_national_char(const char *logical_type) {
    return (logical_type != NULL &&
            strncmp(logical_type, "NCHAR(", nchar_logical_prefix_length) == 0) != 0;
}

static bool logical_type_is_national_char_or_varchar(const char *logical_type) {
    return (logical_type_is_national_char(logical_type) ||
            logical_type_is_national_varchar(logical_type)) != 0;
}

static size_t char_descriptor_prefix_length(const char *logical_type) {
    if (logical_type_is_national_char(logical_type)) {
        return nchar_logical_prefix_length;
    }
    if (logical_type != NULL && strncmp(logical_type, "CHAR(", char_logical_prefix_length) == 0) {
        return char_logical_prefix_length;
    }
    return 0U;
}

static size_t char_descriptor_syntax_overhead(const char *logical_type) {
    if (logical_type_is_national_char(logical_type)) {
        return nchar_logical_syntax_overhead;
    }
    return char_logical_syntax_overhead;
}

static size_t varchar_descriptor_prefix_length(const char *logical_type) {
    if (logical_type_is_national_varchar(logical_type)) {
        return nvarchar_logical_prefix_length;
    }
    if (logical_type != NULL &&
        strncmp(logical_type, "VARCHAR(", varchar_logical_prefix_length) == 0) {
        return varchar_logical_prefix_length;
    }
    return 0U;
}

static size_t varchar_descriptor_syntax_overhead(const char *logical_type) {
    if (logical_type_is_national_varchar(logical_type)) {
        return nvarchar_logical_syntax_overhead;
    }
    return varchar_logical_syntax_overhead;
}

static uint64_t logical_type_max_bytes_per_character(const char *logical_type) {
    if (logical_type_is_national_char_or_varchar(logical_type)) {
        return utf8mb3_max_bytes_per_character;
    }
    return utf8mb4_max_bytes_per_character;
}

static bool planned_column_is_varchar(const struct planned_column *column) {
    bool has_varchar_logical_type = false;
    bool has_text_physical_type = false;

    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }

    if (logical_type_is_varchar(column->logical_type)) {
        has_varchar_logical_type = true;
    }
    if (strcmp(column->physical_type, "TEXT") == 0) {
        has_text_physical_type = true;
    }
    if (!has_varchar_logical_type) {
        return false;
    }
    if (!has_text_physical_type) {
        return false;
    }
    return true;
}

static bool planned_column_is_char(const struct planned_column *column) {
    bool has_char_logical_type = false;
    bool has_text_physical_type = false;

    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }

    if (logical_type_is_char(column->logical_type)) {
        has_char_logical_type = true;
    }
    if (strcmp(column->physical_type, "TEXT") == 0) {
        has_text_physical_type = true;
    }
    if (!has_char_logical_type) {
        return false;
    }
    if (!has_text_physical_type) {
        return false;
    }
    return true;
}

static bool planned_column_is_char_or_varchar(const struct planned_column *column) {
    return (planned_column_is_char(column) || planned_column_is_varchar(column)) != 0;
}

static bool planned_column_is_text_family(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    if (strcmp(column->physical_type, "TEXT") != 0) {
        return false;
    }
    return text_family_type_info_for_logical_type(column->logical_type) != NULL;
}

static bool planned_column_is_string_family(const struct planned_column *column) {
    return (planned_column_is_char(column) || planned_column_is_varchar(column) ||
            planned_column_is_text_family(column)) != 0;
}

static bool planned_column_is_json(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    return (strcmp(column->logical_type, "JSON") == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool planned_column_is_spatial(const struct planned_column *column) {
    struct mylite_catalog_column_descriptor descriptor = {0};

    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    snprintf(descriptor.logical_type, sizeof(descriptor.logical_type), "%s", column->logical_type);
    snprintf(
        descriptor.physical_type,
        sizeof(descriptor.physical_type),
        "%s",
        column->physical_type
    );
    return column_descriptor_is_spatial(&descriptor);
}

static bool planned_column_is_enum(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    return (strncmp(column->logical_type, "ENUM(", enum_logical_prefix_length) == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool planned_column_is_set(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    return (strncmp(column->logical_type, "SET(", set_logical_prefix_length) == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool planned_column_is_binary_string_family(const struct planned_column *column) {
    struct mylite_catalog_column_descriptor descriptor = {0};

    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }

    snprintf(descriptor.logical_type, sizeof(descriptor.logical_type), "%s", column->logical_type);
    snprintf(
        descriptor.physical_type,
        sizeof(descriptor.physical_type),
        "%s",
        column->physical_type
    );
    return column_descriptor_is_binary_string_family(&descriptor);
}

static bool planned_column_is_binary_blob_family(const struct planned_column *column) {
    struct mylite_catalog_column_descriptor descriptor = {0};

    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }

    snprintf(descriptor.logical_type, sizeof(descriptor.logical_type), "%s", column->logical_type);
    snprintf(
        descriptor.physical_type,
        sizeof(descriptor.physical_type),
        "%s",
        column->physical_type
    );
    return column_descriptor_is_binary_blob_family(&descriptor);
}

static bool planned_column_is_bit(const struct planned_column *column) {
    struct mylite_catalog_column_descriptor descriptor = {0};

    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }

    snprintf(descriptor.logical_type, sizeof(descriptor.logical_type), "%s", column->logical_type);
    snprintf(
        descriptor.physical_type,
        sizeof(descriptor.physical_type),
        "%s",
        column->physical_type
    );
    return column_descriptor_is_bit(&descriptor);
}

static bool planned_column_is_year(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    return (strcmp(column->logical_type, "YEAR") == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool planned_column_is_decimal(const struct planned_column *column) {
    struct decimal_type_info info = {0};

    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    if (strcmp(column->physical_type, "TEXT") != 0) {
        return false;
    }
    return decimal_type_info_for_logical_type(column->logical_type, &info) == MYLITE_OK;
}

static bool planned_column_is_approximate(const struct planned_column *column) {
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };

    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    if (strcmp(column->physical_type, "REAL") != 0) {
        return false;
    }
    return approximate_type_info_for_logical_type(column->logical_type, &info) == MYLITE_OK;
}

static bool planned_column_is_date(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    return (strcmp(column->logical_type, "DATE") == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool planned_column_is_time(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    return (strcmp(column->logical_type, "TIME") == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool planned_column_is_datetime(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    return (strcmp(column->logical_type, "DATETIME") == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool planned_column_is_timestamp(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    return (strcmp(column->logical_type, "TIMESTAMP") == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool column_default_kind_has_integer_value(
    enum mylite_catalog_column_default_kind default_kind
) {
    return (default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION) != 0;
}

static bool column_default_kind_has_text_value(
    enum mylite_catalog_column_default_kind default_kind
) {
    return (default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION) != 0;
}

static bool column_default_kind_is_expression(
    enum mylite_catalog_column_default_kind default_kind
) {
    return (default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION) != 0;
}

static bool column_default_kind_materializes_value(
    enum mylite_catalog_column_default_kind default_kind
) {
    return (default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME) != 0;
}

static bool column_descriptor_is_varchar(const struct mylite_catalog_column_descriptor *column) {
    bool has_varchar_logical_type = false;
    bool has_text_physical_type = false;

    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }

    if (logical_type_is_varchar(column->logical_type)) {
        has_varchar_logical_type = true;
    }
    if (strcmp(column->physical_type, "TEXT") == 0) {
        has_text_physical_type = true;
    }
    if (!has_varchar_logical_type) {
        return false;
    }
    if (!has_text_physical_type) {
        return false;
    }
    return true;
}

static bool column_descriptor_is_char(const struct mylite_catalog_column_descriptor *column) {
    bool has_char_logical_type = false;
    bool has_text_physical_type = false;

    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }

    if (logical_type_is_char(column->logical_type)) {
        has_char_logical_type = true;
    }
    if (strcmp(column->physical_type, "TEXT") == 0) {
        has_text_physical_type = true;
    }
    if (!has_char_logical_type) {
        return false;
    }
    if (!has_text_physical_type) {
        return false;
    }
    return true;
}

static bool column_descriptor_is_national_char_or_varchar(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    if (strcmp(column->physical_type, "TEXT") != 0) {
        return false;
    }
    return logical_type_is_national_char_or_varchar(column->logical_type);
}

static bool column_descriptor_is_char_or_varchar(
    const struct mylite_catalog_column_descriptor *column
) {
    return (column_descriptor_is_char(column) || column_descriptor_is_varchar(column)) != 0;
}

static bool column_descriptor_is_text_family(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    if (strcmp(column->physical_type, "TEXT") != 0) {
        return false;
    }
    return text_family_type_info_for_logical_type(column->logical_type) != NULL;
}

static bool column_descriptor_is_string_family(
    const struct mylite_catalog_column_descriptor *column
) {
    return (column_descriptor_is_char(column) || column_descriptor_is_varchar(column) ||
            column_descriptor_is_text_family(column)) != 0;
}

static bool column_descriptor_is_json(const struct mylite_catalog_column_descriptor *column) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    return (strcmp(column->logical_type, "JSON") == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool column_descriptor_is_spatial(const struct mylite_catalog_column_descriptor *column) {
    static const char *const spatial_types[] = {
        "geometry",
        "point",
        "linestring",
        "polygon",
        "multipoint",
        "multilinestring",
        "multipolygon",
        "geomcollection",
    };

    if (column == NULL || strcmp(column->physical_type, "BLOB") != 0) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(spatial_types) / sizeof(spatial_types[0]); ++index) {
        if (strcmp(column->logical_type, spatial_types[index]) == 0) {
            return true;
        }
    }
    return false;
}

static bool column_descriptor_is_enum(const struct mylite_catalog_column_descriptor *column) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    return (strncmp(column->logical_type, "ENUM(", enum_logical_prefix_length) == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool column_descriptor_uses_enum_implicit_missing_default(
    const struct mylite_catalog_column_descriptor *column
) {
    if (!column_descriptor_is_enum(column) || column->is_nullable) {
        return false;
    }
    return (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NONE ||
            column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT) != 0;
}

static bool column_descriptor_is_set(const struct mylite_catalog_column_descriptor *column) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    return (strncmp(column->logical_type, "SET(", set_logical_prefix_length) == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool column_descriptor_is_binary_string_family(
    const struct mylite_catalog_column_descriptor *column
) {
    struct binary_string_type_info storage = {0};

    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    if (strcmp(column->physical_type, "BLOB") != 0) {
        return false;
    }
    return binary_string_type_info_for_logical_type(column->logical_type, &storage) != NULL;
}

static bool column_descriptor_is_binary_blob_family(
    const struct mylite_catalog_column_descriptor *column
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info = NULL;

    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    if (strcmp(column->physical_type, "BLOB") != 0) {
        return false;
    }
    info = binary_string_type_info_for_logical_type(column->logical_type, &storage);
    return (info != NULL && info->blob_family) != 0;
}

static bool column_descriptor_is_bit(const struct mylite_catalog_column_descriptor *column) {
    uint64_t width = 0U;

    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    if (strcmp(column->physical_type, "BLOB") != 0) {
        return false;
    }
    return bit_width_for_logical_type(column->logical_type, &width) == MYLITE_OK;
}

static bool column_descriptor_is_year(const struct mylite_catalog_column_descriptor *column) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    return (strcmp(column->logical_type, "YEAR") == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool column_descriptor_is_decimal(const struct mylite_catalog_column_descriptor *column) {
    struct decimal_type_info info = {0};

    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    if (strcmp(column->physical_type, "TEXT") != 0) {
        return false;
    }
    return decimal_type_info_for_logical_type(column->logical_type, &info) == MYLITE_OK;
}

static bool column_descriptor_is_approximate(
    const struct mylite_catalog_column_descriptor *column
) {
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };

    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    if (strcmp(column->physical_type, "REAL") != 0) {
        return false;
    }
    return approximate_type_info_for_logical_type(column->logical_type, &info) == MYLITE_OK;
}

static bool column_descriptor_is_date(const struct mylite_catalog_column_descriptor *column) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    return (strcmp(column->logical_type, "DATE") == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool column_descriptor_is_time(const struct mylite_catalog_column_descriptor *column) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    return (strcmp(column->logical_type, "TIME") == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool column_descriptor_is_datetime(const struct mylite_catalog_column_descriptor *column) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    return (strcmp(column->logical_type, "DATETIME") == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool column_descriptor_is_timestamp(const struct mylite_catalog_column_descriptor *column) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    return (strcmp(column->logical_type, "TIMESTAMP") == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static int enum_type_info_for_logical_type(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    struct enum_type_info *out_info
) {
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    size_t index = enum_logical_prefix_length;
    int rc = MYLITE_OK;

    if (logical_type == NULL || unsupported_message == NULL || out_info == NULL ||
        logical_length <= enum_logical_syntax_overhead ||
        strncmp(logical_type, "ENUM(", enum_logical_prefix_length) != 0 ||
        logical_type[logical_length - 1U] != ')') {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    *out_info = (struct enum_type_info){0};
    while (rc == MYLITE_OK && index < logical_length - 1U) {
        if (out_info->label_count >= enum_label_count_capacity) {
            set_unsupported_error(database, unsupported_message);
            return MYLITE_ERROR;
        }
        rc = parse_enum_descriptor_label(
            database,
            logical_type,
            logical_length - 1U,
            &index,
            out_info
        );
        if (rc == MYLITE_OK && index < logical_length - 1U) {
            if (logical_type[index] != ',') {
                set_unsupported_error(database, unsupported_message);
                return MYLITE_ERROR;
            }
            ++index;
        }
    }
    if (rc == MYLITE_OK && out_info->label_count == 0U) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    return rc;
}

static int parse_enum_descriptor_label(
    struct mylite_db *database,
    const char *logical_type,
    size_t end_index,
    size_t *index,
    struct enum_type_info *out_info
) {
    size_t label_start = out_info->decoded_length;

    if (*index >= end_index || logical_type[*index] != '\'') {
        set_unsupported_error(database, "invalid ENUM descriptor");
        return MYLITE_ERROR;
    }
    ++*index;
    while (*index < end_index) {
        char byte = logical_type[*index];

        ++*index;
        if (byte == '\'') {
            if (*index < end_index && logical_type[*index] == '\'') {
                ++*index;
                if (enum_type_info_append_byte(database, out_info, '\'') != MYLITE_OK) {
                    return MYLITE_ERROR;
                }
                continue;
            }
            return finish_parse_enum_descriptor_label(database, label_start, out_info);
        }
        if (byte == '\\') {
            if (read_enum_descriptor_escaped_byte(
                    database,
                    logical_type,
                    end_index,
                    index,
                    &byte
                ) != MYLITE_OK) {
                return MYLITE_ERROR;
            }
        }
        if (enum_type_info_append_byte(database, out_info, byte) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
    }

    set_unsupported_error(database, "invalid ENUM descriptor");
    return MYLITE_ERROR;
}

static int finish_parse_enum_descriptor_label(
    struct mylite_db *database,
    size_t label_start,
    struct enum_type_info *out_info
) {
    size_t character_length = 0U;
    size_t text_length = 0U;

    if (enum_type_info_append_byte(database, out_info, '\0') != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    text_length = out_info->decoded_length - label_start - 1U;
    if (validate_utf8_text(
            &out_info->decoded_storage[label_start],
            text_length,
            &character_length
        ) != MYLITE_OK) {
        set_unsupported_error(database, "invalid ENUM descriptor");
        return MYLITE_ERROR;
    }
    out_info->labels[out_info->label_count] = (struct enum_label_descriptor){
        .text = &out_info->decoded_storage[label_start],
        .text_length = text_length,
        .character_length = character_length,
    };
    if (out_info->max_label_character_length < character_length) {
        out_info->max_label_character_length = character_length;
    }
    ++out_info->label_count;
    return MYLITE_OK;
}

static int read_enum_descriptor_escaped_byte(
    struct mylite_db *database,
    const char *logical_type,
    size_t end_index,
    size_t *index,
    char *out_byte
) {
    char byte = '\0';

    if (*index >= end_index) {
        set_unsupported_error(database, "invalid ENUM descriptor");
        return MYLITE_ERROR;
    }
    byte = logical_type[*index];
    ++*index;
    if (byte == 'n') {
        byte = '\n';
    } else if (byte == 'r') {
        byte = '\r';
    } else if (byte == 't') {
        byte = '\t';
    }
    *out_byte = byte;
    return MYLITE_OK;
}

static int enum_type_info_append_byte(
    struct mylite_db *database,
    struct enum_type_info *info,
    char byte
) {
    if (info->decoded_length >= sizeof(info->decoded_storage)) {
        set_unsupported_error(database, "ENUM definition is too large for this MyLite build");
        return MYLITE_ERROR;
    }
    info->decoded_storage[info->decoded_length] = byte;
    ++info->decoded_length;
    return MYLITE_OK;
}

static bool enum_find_label(
    const struct enum_type_info *info,
    const char *text,
    size_t text_length,
    const struct enum_label_descriptor **out_label
) {
    if (out_label != NULL) {
        *out_label = NULL;
    }
    if (info == NULL || text == NULL) {
        return false;
    }
    for (size_t label_index = 0U; label_index < info->label_count; ++label_index) {
        if (enum_label_equals_ascii_case_insensitive(
                info->labels[label_index].text,
                info->labels[label_index].text_length,
                text,
                text_length
            )) {
            if (out_label != NULL) {
                *out_label = &info->labels[label_index];
            }
            return true;
        }
    }
    return false;
}

static bool enum_find_ordinal(
    const struct enum_type_info *info,
    uint64_t ordinal,
    const struct enum_label_descriptor **out_label
) {
    if (out_label != NULL) {
        *out_label = NULL;
    }
    if (info == NULL || ordinal == 0U || ordinal > info->label_count) {
        return false;
    }
    if (out_label != NULL) {
        *out_label = &info->labels[(size_t)ordinal - 1U];
    }
    return true;
}

static bool enum_text_is_unsigned_integer(
    const char *text,
    size_t text_length,
    uint64_t *out_value
) {
    uint64_t value = 0U;

    if (text == NULL || text_length == 0U || out_value == NULL) {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        uint64_t digit = 0U;

        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
        digit = (uint64_t)(text[index] - '0');
        if (value > (UINT64_MAX - digit) / decimal_base) {
            return false;
        }
        value = (value * decimal_base) + digit;
    }
    *out_value = value;
    return true;
}

static int set_type_info_for_logical_type(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    struct set_type_info *out_info
) {
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    size_t index = set_logical_prefix_length;
    int rc = MYLITE_OK;

    if (logical_type == NULL || unsupported_message == NULL || out_info == NULL ||
        logical_length <= set_logical_syntax_overhead ||
        strncmp(logical_type, "SET(", set_logical_prefix_length) != 0 ||
        logical_type[logical_length - 1U] != ')') {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    *out_info = (struct set_type_info){0};
    while (rc == MYLITE_OK && index < logical_length - 1U) {
        if (out_info->member_count >= set_member_count_capacity) {
            set_unsupported_error(database, unsupported_message);
            return MYLITE_ERROR;
        }
        rc = parse_set_descriptor_member(
            database,
            logical_type,
            logical_length - 1U,
            &index,
            out_info
        );
        if (rc == MYLITE_OK && index < logical_length - 1U) {
            if (logical_type[index] != ',') {
                set_unsupported_error(database, unsupported_message);
                return MYLITE_ERROR;
            }
            ++index;
        }
    }
    if (rc == MYLITE_OK && out_info->member_count == 0U) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    return rc;
}

static int parse_set_descriptor_member(
    struct mylite_db *database,
    const char *logical_type,
    size_t end_index,
    size_t *index,
    struct set_type_info *out_info
) {
    size_t member_start = out_info->decoded_length;

    if (*index >= end_index || logical_type[*index] != '\'') {
        set_unsupported_error(database, "invalid SET descriptor");
        return MYLITE_ERROR;
    }
    ++*index;
    while (*index < end_index) {
        char byte = logical_type[*index];

        ++*index;
        if (byte == '\'') {
            if (*index < end_index && logical_type[*index] == '\'') {
                ++*index;
                if (set_type_info_append_byte(database, out_info, '\'') != MYLITE_OK) {
                    return MYLITE_ERROR;
                }
                continue;
            }
            return finish_parse_set_descriptor_member(database, member_start, out_info);
        }
        if (byte == '\\') {
            if (read_set_descriptor_escaped_byte(database, logical_type, end_index, index, &byte) !=
                MYLITE_OK) {
                return MYLITE_ERROR;
            }
        }
        if (set_type_info_append_byte(database, out_info, byte) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
    }

    set_unsupported_error(database, "invalid SET descriptor");
    return MYLITE_ERROR;
}

static int finish_parse_set_descriptor_member(
    struct mylite_db *database,
    size_t member_start,
    struct set_type_info *out_info
) {
    size_t character_length = 0U;
    size_t text_length = 0U;

    if (set_type_info_append_byte(database, out_info, '\0') != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    text_length = out_info->decoded_length - member_start - 1U;
    if (validate_utf8_text(
            &out_info->decoded_storage[member_start],
            text_length,
            &character_length
        ) != MYLITE_OK) {
        set_unsupported_error(database, "invalid SET descriptor");
        return MYLITE_ERROR;
    }
    out_info->members[out_info->member_count] = (struct enum_label_descriptor){
        .text = &out_info->decoded_storage[member_start],
        .text_length = text_length,
        .character_length = character_length,
    };
    out_info->max_display_character_length += character_length;
    if (out_info->member_count > 0U) {
        ++out_info->max_display_character_length;
    }
    ++out_info->member_count;
    return MYLITE_OK;
}

static int read_set_descriptor_escaped_byte(
    struct mylite_db *database,
    const char *logical_type,
    size_t end_index,
    size_t *index,
    char *out_byte
) {
    char byte = '\0';

    if (*index >= end_index) {
        set_unsupported_error(database, "invalid SET descriptor");
        return MYLITE_ERROR;
    }
    byte = logical_type[*index];
    ++*index;
    if (byte == 'n') {
        byte = '\n';
    } else if (byte == 'r') {
        byte = '\r';
    } else if (byte == 't') {
        byte = '\t';
    }
    *out_byte = byte;
    return MYLITE_OK;
}

static int set_type_info_append_byte(
    struct mylite_db *database,
    struct set_type_info *info,
    char byte
) {
    if (info->decoded_length >= sizeof(info->decoded_storage)) {
        set_unsupported_error(database, "SET definition is too large for this MyLite build");
        return MYLITE_ERROR;
    }
    info->decoded_storage[info->decoded_length] = byte;
    ++info->decoded_length;
    return MYLITE_OK;
}

static bool set_find_member(
    const struct set_type_info *info,
    const char *text,
    size_t text_length,
    size_t *out_index
) {
    if (out_index != NULL) {
        *out_index = 0U;
    }
    if (info == NULL || text == NULL) {
        return false;
    }
    for (size_t member_index = 0U; member_index < info->member_count; ++member_index) {
        if (enum_label_equals_ascii_case_insensitive(
                info->members[member_index].text,
                info->members[member_index].text_length,
                text,
                text_length
            )) {
            if (out_index != NULL) {
                *out_index = member_index;
            }
            return true;
        }
    }
    return false;
}

static bool set_bitmap_is_valid(const struct set_type_info *info, uint64_t bitmap) {
    if (info == NULL || info->member_count == 0U ||
        info->member_count > set_member_count_capacity) {
        return false;
    }
    if (info->member_count == set_member_count_capacity) {
        return true;
    }
    return (bitmap >> info->member_count) == 0U;
}

static int decimal_type_info_for_logical_type(
    const char *logical_type,
    struct decimal_type_info *out_info
) {
    static const char prefix[] = "DECIMAL(";
    static const char unsigned_suffix[] = " UNSIGNED";
    struct mylite_sql_source_span span = {0};
    const char *comma = NULL;
    const char *close = NULL;
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    int rc = MYLITE_OK;

    if (logical_type == NULL || out_info == NULL || logical_length <= sizeof(prefix) ||
        strncmp(logical_type, prefix, sizeof(prefix) - 1U) != 0) {
        return MYLITE_ERROR;
    }

    comma = strchr(logical_type + sizeof(prefix) - 1U, ',');
    close = comma == NULL ? NULL : strchr(comma + 1, ')');
    if (comma == NULL || close == NULL) {
        return MYLITE_ERROR;
    }

    *out_info = (struct decimal_type_info){0};
    span = (struct mylite_sql_source_span){
        .text = logical_type + sizeof(prefix) - 1U,
        .length = (size_t)(comma - (logical_type + sizeof(prefix) - 1U)),
    };
    rc = parse_unsigned_integer_literal(&span, &out_info->precision);
    if (rc != MYLITE_OK) {
        return rc;
    }
    span = (struct mylite_sql_source_span){
        .text = comma + 1,
        .length = (size_t)(close - (comma + 1)),
    };
    rc = parse_unsigned_integer_literal(&span, &out_info->scale);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (strcmp(close + 1, "") == 0) {
        out_info->is_unsigned = false;
        return MYLITE_OK;
    }
    if (strcmp(close + 1, unsigned_suffix) == 0) {
        out_info->is_unsigned = true;
        return MYLITE_OK;
    }

    return MYLITE_ERROR;
}

static int approximate_type_info_for_logical_type(
    const char *logical_type,
    struct approximate_type_info *out_info
) {
    if (logical_type == NULL || out_info == NULL) {
        return MYLITE_ERROR;
    }
    if (strcmp(logical_type, "FLOAT") == 0) {
        *out_info = (struct approximate_type_info){
            .type_class = APPROXIMATE_TYPE_FLOAT,
            .is_unsigned = false,
        };
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "FLOAT UNSIGNED") == 0) {
        *out_info = (struct approximate_type_info){
            .type_class = APPROXIMATE_TYPE_FLOAT,
            .is_unsigned = true,
        };
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "DOUBLE") == 0) {
        *out_info = (struct approximate_type_info){
            .type_class = APPROXIMATE_TYPE_DOUBLE,
            .is_unsigned = false,
        };
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "DOUBLE UNSIGNED") == 0) {
        *out_info = (struct approximate_type_info){
            .type_class = APPROXIMATE_TYPE_DOUBLE,
            .is_unsigned = true,
        };
        return MYLITE_OK;
    }

    return MYLITE_ERROR;
}

static const struct text_family_type_info *text_family_type_info_for_logical_type(
    const char *logical_type
) {
    static const struct text_family_type_info text_types[] = {
        {"TINYTEXT", "tinytext", tinytext_max_length},
        {"TEXT", "text", text_max_length},
        {"MEDIUMTEXT", "mediumtext", mediumtext_max_length},
        {"LONGTEXT", "longtext", longtext_max_length},
    };

    if (logical_type == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < sizeof(text_types) / sizeof(text_types[0]); ++index) {
        if (strcmp(logical_type, text_types[index].logical_type) == 0) {
            return &text_types[index];
        }
    }
    return NULL;
}

static const struct binary_string_type_info *binary_string_type_info_for_logical_type(
    const char *logical_type,
    struct binary_string_type_info *storage
) {
    static const struct binary_string_type_info blob_types[] = {
        {"TINYBLOB", "tinyblob", 255U, false, true},
        {"BLOB", "blob", 65535U, false, true},
        {"MEDIUMBLOB", "mediumblob", 16777215U, false, true},
        {"LONGBLOB", "longblob", 4294967295ULL, false, true},
    };
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);

    if (logical_type == NULL || storage == NULL) {
        return NULL;
    }
    if (strncmp(logical_type, "BINARY(", binary_logical_prefix_length) == 0) {
        uint64_t length = 0U;
        struct mylite_sql_source_span span = {0};

        if (logical_length <= binary_logical_syntax_overhead ||
            logical_type[logical_length - 1U] != ')') {
            return NULL;
        }
        span = (struct mylite_sql_source_span){
            .text = &logical_type[binary_logical_prefix_length],
            .length = logical_length - binary_logical_syntax_overhead,
        };
        if (parse_unsigned_integer_literal(&span, &length) != MYLITE_OK ||
            length > binary_max_length) {
            return NULL;
        }
        *storage = (struct binary_string_type_info){
            .logical_type = logical_type,
            .display_type = NULL,
            .maximum_length = length,
            .fixed_length = true,
            .blob_family = false,
        };
        return storage;
    }
    if (strncmp(logical_type, "VARBINARY(", varbinary_logical_prefix_length) == 0) {
        uint64_t length = 0U;
        struct mylite_sql_source_span span = {0};

        if (logical_length <= varbinary_logical_syntax_overhead ||
            logical_type[logical_length - 1U] != ')') {
            return NULL;
        }
        span = (struct mylite_sql_source_span){
            .text = &logical_type[varbinary_logical_prefix_length],
            .length = logical_length - varbinary_logical_syntax_overhead,
        };
        if (parse_unsigned_integer_literal(&span, &length) != MYLITE_OK ||
            length > varbinary_max_length) {
            return NULL;
        }
        *storage = (struct binary_string_type_info){
            .logical_type = logical_type,
            .display_type = NULL,
            .maximum_length = length,
            .fixed_length = false,
            .blob_family = false,
        };
        return storage;
    }
    for (size_t index = 0U; index < sizeof(blob_types) / sizeof(blob_types[0]); ++index) {
        if (strcmp(logical_type, blob_types[index].logical_type) == 0) {
            return &blob_types[index];
        }
    }
    return NULL;
}

static int parse_binary_descriptor_length(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    size_t *out_length
) {
    struct mylite_sql_source_span length_span = {0};
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    uint64_t length = 0U;
    int rc = MYLITE_OK;

    if (out_length == NULL || logical_type == NULL || unsupported_message == NULL ||
        logical_length <= sizeof("BINARY(") || logical_type[logical_length - 1U] != ')' ||
        strncmp(logical_type, "BINARY(", binary_logical_prefix_length) != 0) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    length_span = (struct mylite_sql_source_span){
        .text = &logical_type[binary_logical_prefix_length],
        .length = logical_length - binary_logical_syntax_overhead,
    };
    rc = parse_unsigned_integer_literal(&length_span, &length);
    if (rc != MYLITE_OK || length > binary_max_length) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    *out_length = (size_t)length;
    return MYLITE_OK;
}

static int parse_varbinary_descriptor_length(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    size_t *out_length
) {
    struct mylite_sql_source_span length_span = {0};
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    uint64_t length = 0U;
    int rc = MYLITE_OK;

    if (out_length == NULL || logical_type == NULL || unsupported_message == NULL ||
        logical_length <= sizeof("VARBINARY(") || logical_type[logical_length - 1U] != ')' ||
        strncmp(logical_type, "VARBINARY(", varbinary_logical_prefix_length) != 0) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    length_span = (struct mylite_sql_source_span){
        .text = &logical_type[varbinary_logical_prefix_length],
        .length = logical_length - varbinary_logical_syntax_overhead,
    };
    rc = parse_unsigned_integer_literal(&length_span, &length);
    if (rc != MYLITE_OK || length > varbinary_max_length) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    *out_length = (size_t)length;
    return MYLITE_OK;
}

static int bit_width_for_logical_type(const char *logical_type, uint64_t *out_width) {
    struct mylite_sql_source_span span = {0};
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    uint64_t width = 0U;

    if (logical_type == NULL || out_width == NULL ||
        logical_length <= bit_logical_syntax_overhead || logical_type[logical_length - 1U] != ')' ||
        strncmp(logical_type, "BIT(", bit_logical_prefix_length) != 0) {
        return MYLITE_ERROR;
    }
    span = (struct mylite_sql_source_span){
        .text = &logical_type[bit_logical_prefix_length],
        .length = logical_length - bit_logical_syntax_overhead,
    };
    if (parse_unsigned_integer_literal(&span, &width) != MYLITE_OK || width == 0U ||
        width > bit_max_length) {
        return MYLITE_ERROR;
    }

    *out_width = width;
    return MYLITE_OK;
}

static size_t bit_byte_width(uint64_t width) {
    return (size_t)((width + bit_byte_rounding_offset) / byte_bit_count);
}

static uint64_t bit_width_max_value(uint64_t width) {
    if (width >= bit_max_length) {
        return UINT64_MAX;
    }
    return (UINT64_C(1) << width) - 1U;
}

static bool column_descriptor_is_auto_increment(
    const struct mylite_catalog_column_descriptor *column
) {
    return (column != NULL && column->is_auto_increment) != 0;
}

static int validate_planned_table_row_size(
    struct mylite_db *database,
    const struct planned_column *columns,
    size_t column_count
) {
    uint64_t total = 0U;
    int rc = MYLITE_OK;

    if (columns == NULL && column_count != 0U) {
        set_runtime_error(database, "invalid planned columns");
        return MYLITE_ERROR;
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        rc = add_planned_column_row_size(database, &columns[column_index], &total);
    }

    return rc;
}

static int validate_added_column_row_size(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const struct planned_column *added_column
) {
    uint64_t total = 0U;
    int rc = MYLITE_OK;

    if ((columns == NULL && column_count != 0U) || added_column == NULL) {
        set_runtime_error(database, "invalid add-column row-size plan");
        return MYLITE_ERROR;
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        rc = add_catalog_column_row_size(database, &columns[column_index], &total);
    }
    if (rc == MYLITE_OK) {
        rc = add_planned_column_row_size(database, added_column, &total);
    }

    return rc;
}

static int add_planned_column_row_size(
    struct mylite_db *database,
    const struct planned_column *column,
    uint64_t *total
) {
    uint64_t size = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || total == NULL) {
        set_runtime_error(database, "invalid planned column row-size input");
        return MYLITE_ERROR;
    }
    rc = row_size_for_column_descriptor(
        database,
        column->logical_type,
        column->physical_type,
        "row-size validation supports only current baseline column descriptors",
        &size
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return add_row_size_bytes(database, size, total);
}

static int add_catalog_column_row_size(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    uint64_t *total
) {
    uint64_t size = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || total == NULL) {
        set_runtime_error(database, "invalid catalog column row-size input");
        return MYLITE_ERROR;
    }
    rc = row_size_for_column_descriptor(
        database,
        column->logical_type,
        column->physical_type,
        "row-size validation supports only current baseline column descriptors",
        &size
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return add_row_size_bytes(database, size, total);
}

static int row_size_for_column_descriptor(
    struct mylite_db *database,
    const char *logical_type,
    const char *physical_type,
    const char *unsupported_message,
    uint64_t *out_size
) {
    struct approximate_type_info approximate_info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    uint64_t size = 0U;
    int rc = MYLITE_OK;

    if (logical_type == NULL || physical_type == NULL || unsupported_message == NULL ||
        out_size == NULL) {
        set_runtime_error(database, "invalid row-size descriptor");
        return MYLITE_ERROR;
    }
    if (strcmp(physical_type, "INTEGER") == 0) {
        rc = integer_row_size_bytes(logical_type, &size);
        if (rc != MYLITE_OK) {
            set_unsupported_error(database, unsupported_message);
            return MYLITE_ERROR;
        }
        *out_size = size;
        return MYLITE_OK;
    }
    if (strcmp(physical_type, "REAL") == 0) {
        rc = approximate_type_info_for_logical_type(logical_type, &approximate_info);
        if (rc != MYLITE_OK) {
            set_unsupported_error(database, unsupported_message);
            return MYLITE_ERROR;
        }
        return approximate_row_size_bytes(&approximate_info, out_size);
    }
    if (strcmp(physical_type, "BLOB") == 0) {
        uint64_t bit_width = 0U;
        const char *spatial_type = NULL;

        if (spatial_logical_type_display_text(logical_type, &spatial_type)) {
            *out_size = blob_family_row_size_contribution;
            return MYLITE_OK;
        }
        if (bit_width_for_logical_type(logical_type, &bit_width) == MYLITE_OK) {
            *out_size = (uint64_t)bit_byte_width(bit_width);
            return MYLITE_OK;
        }
        return binary_string_row_size_bytes(database, logical_type, out_size, unsupported_message);
    }
    if (strcmp(physical_type, "TEXT") != 0) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    return text_backed_row_size_bytes(database, logical_type, out_size, unsupported_message);
}

static int binary_string_row_size_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t *out_size,
    const char *unsupported_message
) {
    struct binary_string_type_info info = {0};
    const struct binary_string_type_info *resolved_info =
        binary_string_type_info_for_logical_type(logical_type, &info);

    if (resolved_info == NULL) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (resolved_info->fixed_length) {
        *out_size = resolved_info->maximum_length;
        return MYLITE_OK;
    }
    if (!resolved_info->blob_family) {
        *out_size = resolved_info->maximum_length;
        if (resolved_info->maximum_length <= varchar_one_byte_length_prefix_max) {
            *out_size += 1U;
        } else {
            *out_size += 2U;
        }
        return MYLITE_OK;
    }
    *out_size = blob_family_row_size_contribution;
    return MYLITE_OK;
}

static int text_backed_row_size_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t *out_size,
    const char *unsupported_message
) {
    struct decimal_type_info decimal_info = {0};
    size_t length = 0U;
    uint64_t size = 0U;
    int rc = MYLITE_OK;

    if (logical_type_is_varchar(logical_type)) {
        rc = parse_varchar_descriptor_length(database, logical_type, unsupported_message, &length);
        if (rc != MYLITE_OK) {
            return rc;
        }
        size = (uint64_t)length * logical_type_max_bytes_per_character(logical_type);
        *out_size = size;
        if (size <= varchar_one_byte_length_prefix_max) {
            *out_size += 1U;
        } else {
            *out_size += 2U;
        }
        return MYLITE_OK;
    }
    if (logical_type_is_char(logical_type)) {
        rc = parse_char_descriptor_length(database, logical_type, unsupported_message, &length);
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_size = (uint64_t)length * logical_type_max_bytes_per_character(logical_type);
        return MYLITE_OK;
    }
    if (decimal_type_info_for_logical_type(logical_type, &decimal_info) == MYLITE_OK) {
        return decimal_row_size_bytes(&decimal_info, out_size);
    }
    if (strncmp(logical_type, "ENUM(", enum_logical_prefix_length) == 0) {
        struct enum_type_info enum_info = {0};

        rc = enum_type_info_for_logical_type(
            database,
            logical_type,
            unsupported_message,
            &enum_info
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_size = enum_info.label_count <= enum_one_byte_label_count_max ? 1U : 2U;
        return MYLITE_OK;
    }
    if (strncmp(logical_type, "SET(", set_logical_prefix_length) == 0) {
        return set_row_size_bytes(database, logical_type, out_size, unsupported_message);
    }
    if (strcmp(logical_type, "YEAR") == 0) {
        *out_size = year_row_size_bytes;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "DATE") == 0) {
        *out_size = date_row_size_bytes;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "TIME") == 0) {
        *out_size = time_row_size_bytes;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "DATETIME") == 0) {
        *out_size = datetime_row_size_bytes;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "TIMESTAMP") == 0) {
        *out_size = timestamp_row_size_bytes;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "JSON") == 0) {
        *out_size = text_family_row_size_contribution;
        return MYLITE_OK;
    }
    if (text_family_type_info_for_logical_type(logical_type) != NULL) {
        *out_size = text_family_row_size_contribution;
        return MYLITE_OK;
    }

    set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static int set_row_size_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t *out_size,
    const char *unsupported_message
) {
    struct set_type_info info = {0};
    int rc = set_type_info_for_logical_type(database, logical_type, unsupported_message, &info);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (info.member_count <= set_one_byte_storage_member_count_max) {
        *out_size = set_one_byte_storage_size;
    } else if (info.member_count <= set_two_byte_storage_member_count_max) {
        *out_size = set_two_byte_storage_size;
    } else if (info.member_count <= set_three_byte_storage_member_count_max) {
        *out_size = set_three_byte_storage_size;
    } else if (info.member_count <= set_four_byte_storage_member_count_max) {
        *out_size = set_four_byte_storage_size;
    } else {
        *out_size = set_eight_byte_storage_size;
    }
    return MYLITE_OK;
}

static int integer_row_size_bytes(const char *logical_type, uint64_t *out_size) {
    if (logical_type == NULL || out_size == NULL) {
        return MYLITE_MISUSE;
    }
    if (strcmp(logical_type, "TINYINT") == 0 || strcmp(logical_type, "TINYINT(1)") == 0 ||
        strcmp(logical_type, "TINYINT UNSIGNED") == 0) {
        *out_size = tinyint_row_size_bytes;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "SMALLINT") == 0 || strcmp(logical_type, "SMALLINT UNSIGNED") == 0) {
        *out_size = smallint_row_size_bytes;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "MEDIUMINT") == 0 || strcmp(logical_type, "MEDIUMINT UNSIGNED") == 0) {
        *out_size = mediumint_row_size_bytes;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "INT") == 0 || strcmp(logical_type, "INT UNSIGNED") == 0) {
        *out_size = int_row_size_bytes;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "BIGINT") == 0 || strcmp(logical_type, "BIGINT UNSIGNED") == 0) {
        *out_size = bigint_row_size_bytes;
        return MYLITE_OK;
    }

    return MYLITE_ERROR;
}

static int decimal_row_size_bytes(const struct decimal_type_info *info, uint64_t *out_size) {
    uint64_t integer_digits = 0U;
    uint64_t fractional_digits = 0U;

    if (info == NULL || out_size == NULL || info->scale > info->precision) {
        return MYLITE_MISUSE;
    }

    integer_digits = info->precision - info->scale;
    fractional_digits = info->scale;
    *out_size =
        decimal_digit_group_size(integer_digits) + decimal_digit_group_size(fractional_digits);
    return MYLITE_OK;
}

static int approximate_row_size_bytes(
    const struct approximate_type_info *info,
    uint64_t *out_size
) {
    if (info == NULL || out_size == NULL) {
        return MYLITE_MISUSE;
    }
    if (info->type_class == APPROXIMATE_TYPE_FLOAT) {
        *out_size = float_row_size_bytes;
        return MYLITE_OK;
    }
    if (info->type_class == APPROXIMATE_TYPE_DOUBLE) {
        *out_size = double_row_size_bytes;
        return MYLITE_OK;
    }

    return MYLITE_ERROR;
}

static uint64_t decimal_digit_group_size(uint64_t digits) {
    static const uint64_t leftover_digit_bytes[] = {0U, 1U, 1U, 2U, 2U, 3U, 3U, 4U, 4U};
    uint64_t full_groups = digits / decimal_digits_per_storage_group;
    uint64_t leftover_digits = digits % decimal_digits_per_storage_group;

    return (full_groups * 4U) + leftover_digit_bytes[leftover_digits];
}

static int add_row_size_bytes(struct mylite_db *database, uint64_t addition, uint64_t *total) {
    if (total == NULL) {
        set_runtime_error(database, "invalid row-size accumulator");
        return MYLITE_ERROR;
    }
    if (*total > UINT64_MAX - addition) {
        set_row_size_too_large_error(database);
        return MYLITE_ERROR;
    }
    *total += addition;
    if (*total > mysql_max_row_size) {
        set_row_size_too_large_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_planned_string_key_column(
    struct mylite_db *database,
    const struct planned_column *column
) {
    size_t length = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || !planned_column_is_char_or_varchar(column)) {
        set_runtime_error(database, "invalid string key column");
        return MYLITE_ERROR;
    }
    if (planned_column_is_char(column)) {
        rc = parse_char_descriptor_length(
            database,
            column->logical_type,
            "string keys support only baseline CHAR descriptors",
            &length
        );
    } else {
        rc = parse_varchar_descriptor_length(
            database,
            column->logical_type,
            "string keys support only baseline VARCHAR descriptors",
            &length
        );
    }
    if (rc == MYLITE_OK && length == 0U) {
        set_storage_engine_cant_index_column_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && planned_column_is_varchar(column) &&
        length > varchar_string_key_max_length) {
        set_storage_engine_cant_index_column_error(database, column->name);
        return MYLITE_ERROR;
    }

    return rc;
}

static int validate_descriptor_string_key_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    size_t length = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || !column_descriptor_is_char_or_varchar(column)) {
        set_runtime_error(database, "invalid string key column descriptor");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_char(column)) {
        rc = char_length_for_column(database, column, &length);
    } else {
        rc = varchar_length_for_column(database, column, &length);
    }
    if (rc == MYLITE_OK && length == 0U) {
        set_storage_engine_cant_index_column_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && column_descriptor_is_varchar(column) &&
        length > varchar_string_key_max_length) {
        set_storage_engine_cant_index_column_error(database, column->name);
        return MYLITE_ERROR;
    }

    return rc;
}

static int validate_string_key_value(
    struct mylite_db *database,
    const struct planned_value *value
) {
    if (value == NULL) {
        set_runtime_error(database, "invalid string key value");
        return MYLITE_ERROR;
    }
    if (value->is_null) {
        return MYLITE_OK;
    }
    if (!value->is_text || !text_value_is_supported_string_key(value->text, value->text_length)) {
        set_non_ascii_string_key_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static bool loaded_index_part_requires_string_key_validation(const struct loaded_index_part *part) {
    if (part == NULL) {
        return false;
    }
    if (column_descriptor_is_char_or_varchar(&part->column)) {
        return true;
    }

    if (!part->index_column.has_prefix_length) {
        return false;
    }

    return column_descriptor_is_text_family(&part->column);
}

static bool text_value_is_supported_string_key(const char *text, size_t text_length) {
    if (text == NULL && text_length != 0U) {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (byte == '\0' || byte > ascii_max_byte) {
            return false;
        }
    }

    return true;
}

static bool modify_column_integer_value_domain_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    const char *original_logical_type = original_column->logical_type;
    const char *replacement_logical_type = replacement_column->logical_type;

    if (original_logical_type == NULL || replacement_logical_type == NULL) {
        return false;
    }
    if (strcmp(original_logical_type, "TINYINT(1)") == 0) {
        original_logical_type = "TINYINT";
    }
    if (strcmp(replacement_logical_type, "TINYINT(1)") == 0) {
        replacement_logical_type = "TINYINT";
    }

    return strcmp(original_logical_type, replacement_logical_type) == 0;
}

static bool modify_column_char_varchar_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    return (column_descriptor_is_char_or_varchar(original_column) &&
            planned_column_is_char_or_varchar(replacement_column)) != 0;
}

static bool modify_column_text_family_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    return (column_descriptor_is_string_family(original_column) &&
            planned_column_is_text_family(replacement_column)) != 0;
}

static bool modify_column_binary_string_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    bool original_is_string = (column_descriptor_is_string_family(original_column) ||
                               column_descriptor_is_binary_string_family(original_column)) != 0;

    return (original_is_string && planned_column_is_binary_string_family(replacement_column)) != 0;
}

static bool modify_column_temporal_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    bool original_is_temporal = (column_descriptor_is_datetime(original_column) ||
                                 column_descriptor_is_timestamp(original_column)) != 0;
    bool replacement_is_temporal = (planned_column_is_datetime(replacement_column) ||
                                    planned_column_is_timestamp(replacement_column)) != 0;

    return (original_is_temporal && replacement_is_temporal) != 0;
}

static bool column_is_nullable(const struct mylite_sql_ast_node *nullability_node) {
    return mylite_sql_ast_node_nullability(nullability_node) != MYLITE_SQL_AST_NULLABILITY_NOT_NULL;
}
