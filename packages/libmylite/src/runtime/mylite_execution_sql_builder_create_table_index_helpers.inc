int build_physical_table_name(int64_t table_id, char *destination, size_t destination_size) {
    int written = snprintf(destination, destination_size, "_mylite_user_table_%" PRId64, table_id);

    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

int build_physical_view_name(int64_t table_id, char *destination, size_t destination_size) {
    int written = snprintf(destination, destination_size, "_mylite_user_view_%" PRId64, table_id);

    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

int build_physical_index_name(int64_t index_id, char *destination, size_t destination_size) {
    int written = snprintf(destination, destination_size, "_mylite_user_index_%" PRId64, index_id);

    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

int build_physical_check_constraint_name(
    int64_t check_constraint_id,
    char *destination,
    size_t destination_size
) {
    int written =
        snprintf(destination, destination_size, "_mylite_user_check_%" PRId64, check_constraint_id);

    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

int build_create_table_sql(
    const struct planned_create_table *plan,
    const char *physical_name,
    bool temporary,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = append_create_table_definition_sql(&string, plan, physical_name, temporary);
    if (rc == MYLITE_OK && plan->has_primary_key) {
        rc = append_create_table_primary_key_sql(&string, plan, physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = append_create_table_secondary_indexes_sql(&string, plan, physical_name);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);

    return rc;
}

int build_create_table_definition_sql(
    const struct planned_create_table *plan,
    const char *physical_name,
    bool temporary,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = append_create_table_definition_sql(&string, plan, physical_name, temporary);
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_create_table_definition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan,
    const char *physical_name,
    bool temporary
) {
    const char *create_prefix = "CREATE TABLE ";
    int rc = MYLITE_OK;

    if (temporary) {
        create_prefix = "CREATE TEMPORARY TABLE ";
    }
    rc = mylite_dynamic_string_append(string, create_prefix);

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " (");
    }
    if (rc == MYLITE_OK) {
        rc = append_create_table_columns_sql(string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_create_table_generated_column_constraints_sql(string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_create_table_check_constraints_sql(string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

int build_create_table_indexes_sql(
    const struct planned_create_table *plan,
    const char *physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    if (plan->has_primary_key) {
        rc = append_create_table_primary_key_sql(&string, plan, physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = append_create_table_secondary_indexes_sql(&string, plan, physical_name);
    }
    if (rc == MYLITE_OK && string.length != 0U) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_create_table_columns_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const struct planned_column *column = &plan->columns[column_index];

        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ' ');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, column->physical_type);
        }
        if (rc == MYLITE_OK && column->is_generated) {
            rc = mylite_dynamic_string_append(string, " GENERATED ALWAYS AS (");
        }
        if (rc == MYLITE_OK && column->is_generated) {
            rc = mylite_dynamic_string_append(string, column->sqlite_generation_expression);
        }
        if (rc == MYLITE_OK && column->is_generated) {
            rc = mylite_dynamic_string_append(
                string,
                column->generated_kind == MYLITE_CATALOG_GENERATED_COLUMN_STORED ? ") STORED"
                                                                                 : ") VIRTUAL"
            );
        }
        if (rc == MYLITE_OK && !column->is_nullable) {
            rc = mylite_dynamic_string_append(string, " NOT NULL");
        }
    }

    return rc;
}

static int append_create_table_check_constraints_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->check_constraint_count; ++index) {
        const struct planned_check_constraint *check_constraint = &plan->check_constraints[index];

        if (!check_constraint->is_enforced) {
            continue;
        }
        rc = mylite_dynamic_string_append(string, ", CONSTRAINT ");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                check_constraint->physical_name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " CHECK (");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, check_constraint->sqlite_expression);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ')');
        }
    }

    return rc;
}

static int append_create_table_generated_column_constraints_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const struct planned_column *column = &plan->columns[column_index];

        if (!column->is_generated) {
            continue;
        }
        rc = append_generated_column_range_check_constraint_sql(string, column, column_index);
    }

    return rc;
}

static int append_generated_column_range_check_constraint_sql(
    struct mylite_dynamic_string *string,
    const struct planned_column *column,
    size_t column_index
) {
    const uint64_t sqlite_int64_negative_abs_max = 9223372036854775808ULL;
    struct integer_column_range range = {0};
    int rc = integer_range_for_logical_type(
        NULL,
        (struct integer_logical_type_range_request){
            .logical_type = column->logical_type,
            .unsupported_message = "",
        },
        &range
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_dynamic_string_append(string, ", CONSTRAINT ");
    if (rc == MYLITE_OK) {
        rc = append_generated_column_range_check_constraint_name(string, column_index);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " CHECK (");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL OR (typeof(");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") = 'integer' AND ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " >= ");
    }
    if (rc == MYLITE_OK) {
        if (range.negative_abs_max == 0U) {
            rc = mylite_dynamic_string_append_char(string, '0');
        } else if (range.negative_abs_max == sqlite_int64_negative_abs_max) {
            rc = mylite_dynamic_string_append(string, "(-9223372036854775807 - 1)");
        } else {
            rc = mylite_dynamic_string_append_char(string, '-');
            if (rc == MYLITE_OK) {
                rc = append_uint64_literal(string, range.negative_abs_max);
            }
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AND ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " <= ");
    }
    if (rc == MYLITE_OK) {
        rc = append_uint64_literal(string, range.positive_max);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "))");
    }

    return rc;
}

static int append_generated_column_range_check_constraint_name(
    struct mylite_dynamic_string *string,
    size_t column_index
) {
    int rc = mylite_dynamic_string_append_char(string, '"');

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "_mylite_generated_column_range_");
    }
    if (rc == MYLITE_OK) {
        rc = append_size_literal(string, column_index);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '"');
    }

    return rc;
}

static int append_create_table_primary_key_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan,
    const char *physical_name
) {
    int rc =
        append_create_table_index_sql(string, true, plan->primary_key_physical_name, physical_name);

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->primary_key_part_count;
         ++part_index) {
        size_t column_index = plan->primary_key_parts[part_index].column_index;

        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_planned_key_part_sql(string, plan, &plan->columns[column_index]);
        }
        if (rc == MYLITE_OK && plan->primary_key_parts[part_index].sort_direction ==
                                   MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC) {
            rc = mylite_dynamic_string_append(string, " DESC");
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_create_table_index_sql_close(string);
    }

    return rc;
}

static int append_create_table_secondary_indexes_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan,
    const char *physical_name
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->secondary_index_count; ++index) {
        const struct planned_secondary_index *secondary = &plan->secondary_indexes[index];

        if (planned_secondary_index_is_fulltext(secondary) ||
            planned_secondary_index_is_spatial(secondary)) {
            continue;
        }
        rc = append_create_table_index_sql(
            string,
            secondary->is_unique,
            secondary->physical_name,
            physical_name
        );
        for (size_t part_index = 0U; rc == MYLITE_OK && part_index < secondary->part_count;
             ++part_index) {
            if (part_index != 0U) {
                rc = mylite_dynamic_string_append(string, ", ");
            }
            if (rc == MYLITE_OK) {
                rc = append_planned_secondary_key_part_sql(
                    string,
                    plan,
                    &secondary->parts[part_index]
                );
            }
        }
        if (rc == MYLITE_OK) {
            rc = append_create_table_index_sql_close(string);
        }
    }

    return rc;
}

static int append_create_table_index_sql(
    struct mylite_dynamic_string *string,
    bool is_unique,
    const char *index_physical_name,
    const char *table_physical_name
) {
    int rc = mylite_dynamic_string_append(string, "; CREATE ");

    if (rc == MYLITE_OK && is_unique) {
        rc = mylite_dynamic_string_append(string, "UNIQUE ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "INDEX ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, index_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " ON ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, table_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " (");
    }

    return rc;
}

static int append_create_table_index_sql_close(struct mylite_dynamic_string *string) {
    return mylite_dynamic_string_append_char(string, ')');
}

static int append_planned_key_part_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan,
    const struct planned_column *column
) {
    int rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);

    if (rc == MYLITE_OK && planned_column_uses_string_key_collation(plan, column, false)) {
        rc = append_named_string_key_collation_sql(
            string,
            column->collation_name[0] != '\0' ? column->collation_name : plan->default_collation
        );
    }

    return rc;
}

static int append_planned_secondary_key_part_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan,
    const struct planned_secondary_index_part *part
) {
    const struct planned_column *column = &plan->columns[part->column_index];
    char prefix_text[integer_text_capacity];
    int rc = MYLITE_OK;

    if (!part->has_prefix_length) {
        rc = append_planned_key_part_sql(string, plan, column);
    } else {
        rc = mylite_dynamic_string_append(string, "substr(");
    }
    if (rc == MYLITE_OK && part->has_prefix_length) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
    }
    if (rc == MYLITE_OK && part->has_prefix_length) {
        int written = snprintf(prefix_text, sizeof(prefix_text), "%" PRId64, part->prefix_length);

        if (written < 0 || (size_t)written >= sizeof(prefix_text)) {
            return MYLITE_NOMEM;
        }
        rc = mylite_dynamic_string_append(string, ", 1, ");
    }
    if (rc == MYLITE_OK && part->has_prefix_length) {
        rc = mylite_dynamic_string_append(string, prefix_text);
    }
    if (rc == MYLITE_OK && part->has_prefix_length) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    if (rc == MYLITE_OK && part->has_prefix_length &&
        planned_column_uses_string_key_collation(plan, column, true)) {
        rc = append_named_string_key_collation_sql(
            string,
            column->collation_name[0] != '\0' ? column->collation_name : plan->default_collation
        );
    }
    if (rc == MYLITE_OK && part->sort_direction == MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC) {
        rc = mylite_dynamic_string_append(string, " DESC");
    }

    return rc;
}

int append_loaded_key_part_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *part,
    const char *qualifier
) {
    int rc = MYLITE_OK;

    if (part != NULL && part->index_column.has_prefix_length) {
        return append_loaded_prefix_key_part_sql(string, part, qualifier);
    }

    if (qualifier != NULL) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, qualifier);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, '.');
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, part->column.name);
    }
    if (rc == MYLITE_OK && column_descriptor_uses_string_key_collation(&part->column, false)) {
        rc = append_named_string_key_collation_sql(string, part->column.collation_name);
    }

    return rc;
}

static int append_loaded_prefix_key_part_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *part,
    const char *qualifier
) {
    char prefix_text[integer_text_capacity];
    int rc = MYLITE_OK;
    int written = 0;

    written =
        snprintf(prefix_text, sizeof(prefix_text), "%" PRId64, part->index_column.prefix_length);
    if (written < 0 || (size_t)written >= sizeof(prefix_text)) {
        return MYLITE_NOMEM;
    }

    rc = mylite_dynamic_string_append(string, "substr(");
    if (rc == MYLITE_OK && qualifier != NULL) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, qualifier);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, '.');
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, part->column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", 1, ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, prefix_text);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    if (rc == MYLITE_OK && column_descriptor_uses_string_key_collation(&part->column, true)) {
        rc = append_named_string_key_collation_sql(string, part->column.collation_name);
    }

    return rc;
}

int append_loaded_key_part_parameter_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *part,
    size_t parameter_index
) {
    char prefix_text[integer_text_capacity];
    int rc = MYLITE_OK;
    int written = 0;

    if (part == NULL || !part->index_column.has_prefix_length) {
        return append_numbered_parameter(string, parameter_index);
    }

    written =
        snprintf(prefix_text, sizeof(prefix_text), "%" PRId64, part->index_column.prefix_length);
    if (written < 0 || (size_t)written >= sizeof(prefix_text)) {
        return MYLITE_NOMEM;
    }

    rc = mylite_dynamic_string_append(string, "substr(");
    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", 1, ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, prefix_text);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    if (rc == MYLITE_OK && column_descriptor_uses_string_key_collation(&part->column, true)) {
        rc = append_named_string_key_collation_sql(string, part->column.collation_name);
    }

    return rc;
}

static bool planned_column_uses_string_key_collation(
    const struct planned_create_table *plan,
    const struct planned_column *column,
    bool include_text_family
) {
    const char *collation_name = NULL;

    if (column == NULL) {
        return false;
    }
    if (include_text_family) {
        if (!planned_column_is_string_family(column)) {
            return false;
        }
    } else if (!planned_column_is_char_or_varchar(column)) {
        return false;
    }

    if (column->collation_name[0] != '\0') {
        collation_name = column->collation_name;
    } else if (plan != NULL && plan->default_collation[0] != '\0') {
        collation_name = plan->default_collation;
    }
    return !string_key_collation_name_is_binary(collation_name);
}

bool column_descriptor_uses_string_key_collation(
    const struct mylite_catalog_column_descriptor *column,
    bool include_text_family
) {
    if (column == NULL) {
        return false;
    }
    if (include_text_family) {
        if (!column_descriptor_is_string_family(column)) {
            return false;
        }
    } else if (!column_descriptor_is_char_or_varchar(column)) {
        return false;
    }
    return !string_key_collation_name_is_binary(column->collation_name);
}

static bool string_key_collation_name_is_binary(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return false;
    }
    return (text_equals_ascii_case_insensitive(name, "binary") ||
            text_equals_ascii_case_insensitive(name, "ascii_bin") ||
            text_equals_ascii_case_insensitive(name, "utf8mb3_bin") ||
            text_equals_ascii_case_insensitive(name, "utf8mb4_bin") ||
            text_equals_ascii_case_insensitive(name, "utf8mb4_0900_bin")) != 0;
}

int append_string_key_collation_sql(struct mylite_dynamic_string *string) {
    return append_named_string_key_collation_sql(string, string_key_collation_name);
}

static int append_named_string_key_collation_sql(
    struct mylite_dynamic_string *string,
    const char *collation_name
) {
    enum mylite_collation_kind kind = MYLITE_COLLATION_UTF8MB4_0900_AI_CI;
    const char *sqlite_name = NULL;
    int rc = MYLITE_OK;

    if (mylite_collation_kind_from_name(collation_name, &kind) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    sqlite_name = mylite_collation_sqlite_name(kind);
    if (sqlite_name == NULL) {
        return MYLITE_OK;
    }
    rc = mylite_dynamic_string_append(string, " COLLATE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, sqlite_name);
    }

    return rc;
}
