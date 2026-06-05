static void planned_column_from_catalog_descriptor(
    const struct mylite_catalog_column_descriptor *descriptor,
    const struct mylite_sql_ast_node *default_node,
    struct planned_column *out_column
) {
    *out_column = (struct planned_column){0};
    snprintf(out_column->name, sizeof(out_column->name), "%s", descriptor->name);
    snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "%s",
        descriptor->logical_type
    );
    snprintf(
        out_column->physical_type_storage,
        sizeof(out_column->physical_type_storage),
        "%s",
        descriptor->physical_type
    );
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    out_column->is_nullable = descriptor->is_nullable;
    out_column->is_visible = descriptor->is_visible;
    out_column->is_auto_increment = descriptor->is_auto_increment;
    out_column->on_update_current_timestamp = descriptor->on_update_current_timestamp;
    out_column->default_node = default_node;
    out_column->default_kind = descriptor->default_kind;
    out_column->default_integer = descriptor->default_integer;
    snprintf(
        out_column->default_text,
        sizeof(out_column->default_text),
        "%s",
        descriptor->default_text
    );
    snprintf(
        out_column->character_set_name,
        sizeof(out_column->character_set_name),
        "%s",
        descriptor->character_set_name
    );
    snprintf(
        out_column->collation_name,
        sizeof(out_column->collation_name),
        "%s",
        descriptor->collation_name
    );
    snprintf(out_column->comment, sizeof(out_column->comment), "%s", descriptor->comment);
    out_column->is_generated = descriptor->is_generated;
    out_column->generated_kind = descriptor->generated_kind;
    snprintf(
        out_column->generation_expression,
        sizeof(out_column->generation_expression),
        "%s",
        descriptor->generation_expression
    );
    snprintf(
        out_column->sqlite_generation_expression,
        sizeof(out_column->sqlite_generation_expression),
        "%s",
        descriptor->sqlite_generation_expression
    );
}

static int resolve_alter_table_column_replacement_plan(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    int rc = resolve_alter_table_modify_target_table(database, out_plan);

    if (rc == MYLITE_OK) {
        rc = finalize_alter_table_modify_column_definition(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = reject_alter_table_modify_restricted_tables(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = resolve_alter_table_modify_original_column(database, columns, column_count, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = load_alter_table_modify_indexes(database, columns, column_count, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = apply_alter_table_modify_primary_key_nullability(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = validate_alter_table_modify_indexes(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = validate_alter_table_modify_auto_increment(database, columns, column_count, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = complete_alter_table_modify_column_plan(database, columns, out_plan);
    }
    if (rc == MYLITE_OK && out_plan->is_noop &&
        (out_plan->adds_inline_primary_key || out_plan->adds_inline_unique_key)) {
        out_plan->is_noop = false;
        out_plan->is_metadata_only = true;
    }
    if (rc == MYLITE_OK) {
        rc = complete_alter_table_modify_inline_key_columns(database, columns, column_count, out_plan);
    }

    free(columns);

    return rc;
}

static int resolve_alter_table_modify_target_table(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *out_plan
) {
    int rc = mylite_catalog_read_table_by_name(
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
        return MYLITE_ERROR;
    }
    if (out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, out_plan->unsupported_object_message);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int finalize_alter_table_modify_column_definition(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *out_plan
) {
    int rc = normalize_text_length_argument_for_default_charset(
        database,
        &out_plan->column,
        out_plan->table.default_charset
    );

    if (rc == MYLITE_OK) {
        rc = apply_default_binary_charset_to_column(
            database,
            out_plan->table.default_charset,
            &out_plan->column
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_column_default(database, out_plan->column.default_node, &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        rc = finalize_planned_column_default(database, &out_plan->column);
    }
    return rc;
}

static int reject_alter_table_modify_restricted_tables(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan
) {
    int rc = mylite_execution_reject_check_constraint_table_alter(
        database,
        plan->table.table_id,
        "ALTER TABLE column replacement does not yet support CHECK-constrained tables"
    );

    if (rc == MYLITE_OK) {
        rc = reject_foreign_key_table_alter(
            database,
            plan->table.table_id,
            "ALTER TABLE column replacement does not yet support foreign-key constrained tables"
        );
    }
    return rc;
}

static int resolve_alter_table_modify_original_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_modify_column *out_plan
) {
    size_t column_index = 0U;
    int rc = find_column_index(columns, column_count, out_plan->lookup_column_name, &column_index);

    if (rc != MYLITE_OK) {
        set_unknown_column_in_table_error(
            database,
            out_plan->lookup_column_name,
            out_plan->table.name
        );
        return MYLITE_ERROR;
    }
    if (out_plan->checks_duplicate_replacement) {
        size_t replacement_index = 0U;

        if (find_column_index(columns, column_count, out_plan->column.name, &replacement_index) ==
                MYLITE_OK &&
            replacement_index != column_index) {
            set_duplicate_column_error(database, columns[replacement_index].name);
            return MYLITE_ERROR;
        }
    }

    out_plan->original_column = columns[column_index];
    out_plan->column_index = column_index;
    out_plan->column_count = column_count;
    rc = resolve_alter_table_modify_column_position(database, columns, column_count, out_plan);
    if (rc == MYLITE_OK) {
        rc = collect_no_zero_date_default_warnings(
            database,
            columns,
            column_count,
            (struct zero_temporal_default_warning_filter){
                .skipped_column_id = out_plan->original_column.column_id,
            },
            &out_plan->zero_temporal_default_warnings
        );
    }
    return rc;
}

static int load_alter_table_modify_indexes(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_modify_column *out_plan
) {
    int rc = mylite_execution_load_table_index_infos(
        database,
        out_plan->table.table_id,
        columns,
        column_count,
        &out_plan->indexes,
        &out_plan->index_count
    );

    if (rc != MYLITE_OK || !out_plan->adds_inline_primary_key) {
        return rc;
    }
    for (size_t index = 0U; index < out_plan->index_count; ++index) {
        if (out_plan->indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
            set_multiple_primary_key_error(database);
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static int complete_alter_table_modify_inline_key_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_modify_column *out_plan
) {
    struct mylite_catalog_column_descriptor target_column = {0};
    size_t planned_column_count = 0U;
    int rc = MYLITE_OK;

    if ((!out_plan->adds_inline_primary_key && !out_plan->adds_inline_unique_key) ||
        out_plan->columns != NULL) {
        return MYLITE_OK;
    }

    make_modify_target_descriptor(out_plan, &target_column);
    rc = make_inline_key_column_descriptors(
        database,
        columns,
        column_count,
        out_plan->column_index,
        &target_column,
        true,
        &out_plan->columns,
        &planned_column_count
    );
    if (rc == MYLITE_OK) {
        out_plan->column_count = planned_column_count;
        out_plan->target_column_index = out_plan->column_index;
    }
    return rc;
}

static int apply_alter_table_modify_primary_key_nullability(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *out_plan
) {
    for (size_t index = 0U; index < out_plan->index_count; ++index) {
        const struct loaded_index_info *key = &out_plan->indexes[index];

        if (key->index.kind != MYLITE_CATALOG_INDEX_KIND_PRIMARY ||
            !loaded_index_contains_column_id(key, out_plan->original_column.column_id)) {
            continue;
        }
        if (out_plan->column.nullability == MYLITE_SQL_AST_NULLABILITY_NULL) {
            set_primary_key_part_null_error(database);
            return MYLITE_ERROR;
        }
        if (out_plan->column.nullability == MYLITE_SQL_AST_NULLABILITY_UNSPECIFIED) {
            out_plan->column.is_nullable = false;
        }
        return MYLITE_OK;
    }

    return MYLITE_OK;
}

static int validate_alter_table_modify_indexes(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *plan
) {
    int rc = refresh_alter_table_modify_index_parts(plan);

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_count; ++index) {
        const struct loaded_index_info *key = &plan->indexes[index];

        if (key->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
            rc = validate_alter_table_modify_primary_index(database, key);
        } else if (key->index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY) {
            rc = validate_loaded_index_part_list(
                database,
                key->parts,
                key->part_count,
                key->index.is_unique
            );
        } else if (key->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
            rc = validate_alter_table_modify_fulltext_index(database, key);
        } else if (key->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
            rc = validate_alter_table_modify_spatial_index(database, key);
        }
    }

    return rc;
}

static int refresh_alter_table_modify_index_parts(struct planned_alter_table_modify_column *plan) {
    for (size_t index = 0U; index < plan->index_count; ++index) {
        struct loaded_index_info *key = &plan->indexes[index];

        for (size_t part_index = 0U; part_index < key->part_count; ++part_index) {
            struct loaded_index_part *part = &key->parts[part_index];

            if (part->index_column.column_id == plan->original_column.column_id) {
                make_modify_target_descriptor(plan, &part->column);
            }
        }
    }

    return MYLITE_OK;
}

static int validate_alter_table_modify_auto_increment(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const struct planned_alter_table_modify_column *plan
) {
    size_t auto_increment_count = 0U;
    struct integer_column_range range = {0};
    int rc = MYLITE_OK;

    if (plan == NULL || columns == NULL || plan->column_index >= column_count) {
        set_runtime_error(database, "invalid ALTER TABLE auto-increment plan");
        return MYLITE_ERROR;
    }

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        const bool is_auto_increment =
            column_index == plan->column_index ? plan->column.is_auto_increment
                                               : columns[column_index].is_auto_increment;

        if (is_auto_increment) {
            ++auto_increment_count;
        }
    }
    if (auto_increment_count == 0U) {
        return MYLITE_OK;
    }
    if (auto_increment_count > 1U) {
        set_wrong_auto_key_error(database);
        return MYLITE_ERROR;
    }
    if (!plan->column.is_auto_increment) {
        return MYLITE_OK;
    }

    rc = integer_range_for_logical_type(
        database,
        (struct integer_logical_type_range_request){
            .logical_type = plan->column.logical_type,
            .unsupported_message = "AUTO_INCREMENT supports only baseline integer columns",
        },
        &range
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!alter_table_modify_auto_increment_has_leading_key(plan)) {
        set_wrong_auto_key_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static bool alter_table_modify_auto_increment_has_leading_key(
    const struct planned_alter_table_modify_column *plan
) {
    if (plan == NULL) {
        return false;
    }
    if (plan->adds_inline_primary_key || plan->adds_inline_unique_key) {
        return true;
    }

    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct loaded_index_info *key = &plan->indexes[index];

        if (key->part_count == 0U) {
            continue;
        }
        if (key->parts[0].column.column_id == plan->original_column.column_id) {
            return true;
        }
    }

    return false;
}

static int validate_alter_table_modify_primary_index(
    struct mylite_db *database,
    const struct loaded_index_info *index
) {
    uint64_t total = 0U;

    if (index == NULL || index->part_count == 0U) {
        set_runtime_error(database, "invalid primary-key descriptor");
        return MYLITE_ERROR;
    }
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        const struct loaded_index_part *part = &index->parts[part_index];
        uint64_t part_bytes = 0U;
        int rc = MYLITE_OK;

        if (part->column.is_nullable) {
            set_primary_key_part_null_error(database);
            return MYLITE_ERROR;
        }
        rc = column_descriptor_primary_key_part_key_bytes(database, &part->column, &part_bytes);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (total > UINT64_MAX - part_bytes) {
            set_key_too_long_error(database, innodb_max_key_length_bytes);
            return MYLITE_ERROR;
        }
        total += part_bytes;
        if (total > innodb_max_key_length_bytes) {
            set_key_too_long_error(database, innodb_max_key_length_bytes);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int validate_alter_table_modify_fulltext_index(
    struct mylite_db *database,
    const struct loaded_index_info *index
) {
    if (index == NULL || index->part_count == 0U) {
        set_runtime_error(database, "invalid FULLTEXT index descriptor");
        return MYLITE_ERROR;
    }
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        struct planned_column column = {0};

        planned_column_from_catalog_descriptor(&index->parts[part_index].column, NULL, &column);
        int rc = validate_fulltext_index_column(database, &column);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int validate_alter_table_modify_spatial_index(
    struct mylite_db *database,
    const struct loaded_index_info *index
) {
    if (index == NULL || index->part_count != 1U) {
        set_runtime_error(database, "invalid SPATIAL index descriptor");
        return MYLITE_ERROR;
    }

    return validate_alter_table_add_spatial_column(database, &index->parts[0].column);
}

static int resolve_alter_table_modify_column_position(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_modify_column *out_plan
) {
    size_t after_column_index = 0U;
    int rc = MYLITE_OK;

    out_plan->target_column_index = out_plan->column_index;
    if (!out_plan->has_position) {
        return MYLITE_OK;
    }
    if (out_plan->position_first) {
        out_plan->target_column_index = 0U;
        out_plan->changes_position = out_plan->target_column_index != out_plan->column_index;
        return MYLITE_OK;
    }

    rc = find_column_index(columns, column_count, out_plan->after_column_name, &after_column_index);
    if (rc != MYLITE_OK || after_column_index == out_plan->column_index) {
        set_unknown_column_in_table_error(
            database,
            out_plan->after_column_name,
            out_plan->table.name
        );
        return MYLITE_ERROR;
    }

    if (after_column_index < out_plan->column_index) {
        out_plan->target_column_index = after_column_index + 1U;
    } else {
        out_plan->target_column_index = after_column_index;
    }
    out_plan->changes_position = out_plan->target_column_index != out_plan->column_index;
    return MYLITE_OK;
}

static int complete_alter_table_modify_column_plan(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    struct planned_alter_table_modify_column *out_plan
) {
    bool supports_char_varchar_replacement = modify_column_char_varchar_replacement_supported(
        &out_plan->original_column,
        &out_plan->column
    );
    bool supports_text_family_replacement = modify_column_text_family_replacement_supported(
        &out_plan->original_column,
        &out_plan->column
    );
    bool supports_binary_string_replacement = modify_column_binary_string_replacement_supported(
        &out_plan->original_column,
        &out_plan->column
    );
    bool supports_temporal_replacement =
        modify_column_temporal_replacement_supported(&out_plan->original_column, &out_plan->column);
    bool supports_string_to_integer_replacement = modify_column_string_to_integer_replacement_supported(
        &out_plan->original_column,
        &out_plan->column
    );
    int rc = MYLITE_OK;

    if (!supports_char_varchar_replacement && !supports_text_family_replacement &&
        !supports_binary_string_replacement && !supports_temporal_replacement &&
        !supports_string_to_integer_replacement &&
        (column_descriptor_is_string_family(&out_plan->original_column) ||
         column_descriptor_is_binary_string_family(&out_plan->original_column) ||
         column_descriptor_is_json(&out_plan->original_column) ||
         column_descriptor_is_bit(&out_plan->original_column) ||
         column_descriptor_is_decimal(&out_plan->original_column) ||
         column_descriptor_is_approximate(&out_plan->original_column) ||
         column_descriptor_is_date(&out_plan->original_column) ||
         column_descriptor_is_time(&out_plan->original_column) ||
         column_descriptor_is_datetime(&out_plan->original_column) ||
         column_descriptor_is_timestamp(&out_plan->original_column) ||
         column_descriptor_is_year(&out_plan->original_column) ||
         column_descriptor_is_enum(&out_plan->original_column) ||
         column_descriptor_is_set(&out_plan->original_column) ||
         column_descriptor_is_spatial(&out_plan->original_column) ||
         planned_column_is_string_family(&out_plan->column) ||
         planned_column_is_binary_string_family(&out_plan->column) ||
         planned_column_is_json(&out_plan->column) || planned_column_is_bit(&out_plan->column) ||
         planned_column_is_enum(&out_plan->column) || planned_column_is_set(&out_plan->column) ||
         planned_column_is_decimal(&out_plan->column) ||
         planned_column_is_approximate(&out_plan->column) ||
         planned_column_is_date(&out_plan->column) || planned_column_is_time(&out_plan->column) ||
         planned_column_is_datetime(&out_plan->column) ||
         planned_column_is_timestamp(&out_plan->column) ||
         planned_column_is_year(&out_plan->column) ||
         planned_column_is_spatial(&out_plan->column))) {
        set_unsupported_error(database, out_plan->integer_support_message);
        return MYLITE_ERROR;
    }
    if (modify_column_definition_matches(&out_plan->original_column, &out_plan->column)) {
        if (modify_column_name_matches(&out_plan->original_column, &out_plan->column)) {
            if (out_plan->changes_position) {
                goto rebuild_column;
            }
            if (out_plan->original_column.is_visible) {
                out_plan->is_noop = true;
            } else {
                out_plan->is_metadata_only = true;
            }
            return MYLITE_OK;
        }
        if (out_plan->changes_position) {
            goto rebuild_column;
        }
        out_plan->is_metadata_only = true;
        return MYLITE_OK;
    }
    if (!out_plan->changes_position &&
        modify_column_metadata_only_replacement(&out_plan->original_column, &out_plan->column)) {
        out_plan->is_metadata_only = true;
        return MYLITE_OK;
    }

rebuild_column:
    if (!modify_column_type_matches(&out_plan->original_column, &out_plan->column) ||
        out_plan->original_column.is_auto_increment != out_plan->column.is_auto_increment) {
        out_plan->reports_rebuild_row_count = true;
    }
    rc = choose_sqlite_rowid_alias(
        database,
        columns,
        out_plan->column_count,
        out_plan->rowid_alias_message,
        &out_plan->rowid_alias
    );
    if (rc == MYLITE_OK) {
        rc = collect_modify_column_rebuild_columns(
            database,
            columns,
            out_plan->column_count,
            out_plan
        );
    }

    return rc;
}

static bool modify_column_definition_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    if (strcmp(original_column->logical_type, replacement_column->logical_type) != 0) {
        return false;
    }
    if (strcmp(original_column->physical_type, replacement_column->physical_type) != 0) {
        return false;
    }
    if (original_column->is_nullable != replacement_column->is_nullable) {
        return false;
    }
    if (original_column->is_auto_increment != replacement_column->is_auto_increment) {
        return false;
    }
    if (original_column->on_update_current_timestamp !=
        replacement_column->on_update_current_timestamp) {
        return false;
    }
    if (strcmp(original_column->character_set_name, replacement_column->character_set_name) != 0) {
        return false;
    }
    if (strcmp(original_column->collation_name, replacement_column->collation_name) != 0) {
        return false;
    }
    if (strcmp(original_column->comment, replacement_column->comment) != 0) {
        return false;
    }
    if (original_column->default_kind != replacement_column->default_kind) {
        return false;
    }
    if (original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER &&
        original_column->default_integer != replacement_column->default_integer) {
        return false;
    }
    if (original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION &&
        original_column->default_integer != replacement_column->default_integer) {
        return false;
    }
    if ((original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL ||
         original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT ||
         original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION ||
         original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY ||
         column_default_kind_is_expression(original_column->default_kind)) &&
        strcmp(original_column->default_text, replacement_column->default_text) != 0) {
        return false;
    }

    return true;
}

static bool modify_column_type_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    if (strcmp(original_column->logical_type, replacement_column->logical_type) != 0) {
        return false;
    }
    if (strcmp(original_column->physical_type, replacement_column->physical_type) != 0) {
        return false;
    }

    return true;
}

static bool modify_column_metadata_only_replacement(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    if (strcmp(original_column->physical_type, replacement_column->physical_type) != 0) {
        return false;
    }
    if (original_column->is_nullable != replacement_column->is_nullable) {
        return false;
    }
    if (original_column->is_auto_increment != replacement_column->is_auto_increment) {
        return false;
    }
    if (!modify_column_integer_value_domain_matches(original_column, replacement_column)) {
        return modify_column_char_varchar_widening_metadata_only(
            original_column,
            replacement_column
        );
    }

    return true;
}

static bool modify_column_char_varchar_widening_metadata_only(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    size_t original_length = 0U;
    size_t replacement_length = 0U;
    size_t original_prefix_length = 0U;
    size_t replacement_prefix_length = 0U;
    bool original_is_varchar = column_descriptor_is_varchar(original_column);
    bool replacement_is_varchar = planned_column_is_varchar(replacement_column);
    bool original_is_char = column_descriptor_is_char(original_column);
    bool replacement_is_char = planned_column_is_char(replacement_column);

    if ((!original_is_varchar || !replacement_is_varchar) &&
        (!original_is_char || !replacement_is_char)) {
        return false;
    }
    if (strcmp(original_column->character_set_name, replacement_column->character_set_name) != 0 ||
        strcmp(original_column->collation_name, replacement_column->collation_name) != 0) {
        return false;
    }

    original_prefix_length = original_is_varchar
                                 ? varchar_descriptor_prefix_length(original_column->logical_type)
                                 : char_descriptor_prefix_length(original_column->logical_type);
    replacement_prefix_length = replacement_is_varchar
                                    ? varchar_descriptor_prefix_length(
                                          replacement_column->logical_type)
                                    : char_descriptor_prefix_length(replacement_column->logical_type);

    if (!parse_char_varchar_descriptor_length_literal(
            original_column->logical_type,
            original_prefix_length,
            &original_length
        ) ||
        !parse_char_varchar_descriptor_length_literal(
            replacement_column->logical_type,
            replacement_prefix_length,
            &replacement_length
        )) {
        return false;
    }

    return replacement_length >= original_length;
}

static bool parse_char_varchar_descriptor_length_literal(
    const char *logical_type,
    size_t prefix_length,
    size_t *out_length
) {
    uint64_t value = 0U;
    size_t index = 0U;

    if (logical_type == NULL || out_length == NULL || prefix_length == 0U) {
        return false;
    }
    if (logical_type[prefix_length] == '\0') {
        return false;
    }
    for (index = prefix_length; logical_type[index] != '\0'; ++index) {
        unsigned char byte = (unsigned char)logical_type[index];

        if (byte == ')') {
            *out_length = (size_t)value;
            return logical_type[index + 1U] == '\0';
        }
        if (byte < '0' || byte > '9' ||
            value > (UINT64_MAX - (uint64_t)(byte - '0')) / (uint64_t)decimal_base) {
            return false;
        }
        value = (value * (uint64_t)decimal_base) + (uint64_t)(byte - '0');
    }

    return false;
}

static bool modify_column_name_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    if (strcmp(original_column->name, replacement_column->name) != 0) {
        return false;
    }

    return true;
}

static void planned_alter_table_modify_column_deinit(
    struct planned_alter_table_modify_column *plan
) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    zero_temporal_default_warnings_deinit(&plan->zero_temporal_default_warnings);
    mylite_execution_loaded_index_infos_deinit(&plan->indexes, &plan->index_count);
    *plan = (struct planned_alter_table_modify_column){0};
}
