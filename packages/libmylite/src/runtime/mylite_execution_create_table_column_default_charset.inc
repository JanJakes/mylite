
static int apply_create_table_default_binary_charset_to_column(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *column
) {
    if (!planned_create_table_default_charset_is_binary(plan)) {
        return MYLITE_OK;
    }
    if (create_table_column_has_explicit_charset_or_collation(column_node)) {
        return MYLITE_OK;
    }
    if (planned_column_is_national_char_or_varchar(column)) {
        return MYLITE_OK;
    }
    if (!planned_column_allows_charset_collation_attributes(column)) {
        return MYLITE_OK;
    }

    return apply_default_binary_charset_to_column(database, plan->default_charset, column);
}

static int normalize_text_length_argument_for_default_charset(
    struct mylite_db *database,
    struct planned_column *column,
    const char *default_charset
) {
    const char *effective_charset = default_charset;
    const char *logical_type = NULL;

    if (column == NULL || !column->has_text_length_argument) {
        return MYLITE_OK;
    }
    if (!planned_column_is_text_family(column)) {
        return MYLITE_OK;
    }
    if (column->character_set_name[0] != '\0') {
        effective_charset = column->character_set_name;
    }
    if (effective_charset == NULL || effective_charset[0] == '\0') {
        effective_charset = MYLITE_CATALOG_DEFAULT_TABLE_CHARSET;
    }

    logical_type = text_family_logical_type_for_length(
        column->text_length_argument,
        max_bytes_per_character_for_charset(effective_charset)
    );
    return assign_text_family_descriptor_type(database, column, logical_type);
}

static int apply_default_binary_charset_to_column(
    struct mylite_db *database,
    const char *default_charset,
    struct planned_column *column
) {
    int rc = MYLITE_OK;

    if (!charset_name_is_binary(default_charset)) {
        return MYLITE_OK;
    }
    if (column->character_set_name[0] != '\0' || column->collation_name[0] != '\0') {
        return MYLITE_OK;
    }
    if (planned_column_is_national_char_or_varchar(column)) {
        return MYLITE_OK;
    }
    if (!planned_column_allows_charset_collation_attributes(column)) {
        return MYLITE_OK;
    }

    rc = normalize_text_length_argument_for_default_charset(database, column, default_charset);
    if (rc == MYLITE_OK) {
        rc = normalize_column_to_binary_string_descriptor(database, column);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    column->character_set_name[0] = '\0';
    column->collation_name[0] = '\0';
    return validate_column_default(database, column->default_node, column);
}

static int apply_column_binary_collation_shorthand(
    struct mylite_db *database,
    const char *default_charset,
    struct planned_column *column
) {
    const char *effective_charset = default_charset;
    int rc = MYLITE_OK;

    if (column == NULL || !column->has_binary_collation_attribute) {
        return MYLITE_OK;
    }
    if (column->collation_name[0] != '\0') {
        return MYLITE_OK;
    }
    if (column->character_set_name[0] != '\0') {
        effective_charset = column->character_set_name;
    }
    if (effective_charset == NULL || effective_charset[0] == '\0') {
        effective_charset = MYLITE_CATALOG_DEFAULT_TABLE_CHARSET;
    }
    if (charset_name_is_binary(effective_charset)) {
        return apply_default_binary_charset_to_column(database, effective_charset, column);
    }

    rc = copy_normalized_charset_name(
        database,
        effective_charset,
        column->character_set_name,
        sizeof(column->character_set_name)
    );
    if (rc == MYLITE_OK) {
        rc = copy_binary_collation_for_charset(
            database,
            effective_charset,
            column->collation_name,
            sizeof(column->collation_name)
        );
    }
    return rc;
}

static bool create_table_column_has_explicit_charset_or_collation(
    const struct mylite_sql_ast_node *column_node
) {
    return (child_with_kind(column_node, MYLITE_SQL_AST_COLUMN_CHARSET_ATTRIBUTE) != NULL ||
            child_with_kind(column_node, MYLITE_SQL_AST_COLUMN_COLLATION_ATTRIBUTE) != NULL ||
            child_with_kind(
                column_node,
                MYLITE_SQL_AST_COLUMN_BINARY_COLLATION_ATTRIBUTE
            ) != NULL) != 0;
}

static bool planned_create_table_default_charset_is_binary(
    const struct planned_create_table *plan
) {
    if (plan == NULL) {
        return false;
    }
    if (!charset_name_is_binary(plan->default_charset)) {
        return false;
    }
    return collation_name_is_binary(plan->default_collation);
}
