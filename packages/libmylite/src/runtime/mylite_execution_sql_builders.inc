static int build_physical_table_name(int64_t table_id, char *destination, size_t destination_size) {
    int written = snprintf(destination, destination_size, "_mylite_user_table_%" PRId64, table_id);

    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int build_physical_view_name(int64_t table_id, char *destination, size_t destination_size) {
    int written = snprintf(destination, destination_size, "_mylite_user_view_%" PRId64, table_id);

    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int build_physical_index_name(int64_t index_id, char *destination, size_t destination_size) {
    int written = snprintf(destination, destination_size, "_mylite_user_index_%" PRId64, index_id);

    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int build_physical_check_constraint_name(
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

static int build_create_table_sql(
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

static int build_create_table_definition_sql(
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

static int build_create_table_indexes_sql(
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
            rc = append_planned_key_part_sql(string, &plan->columns[column_index]);
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
    const struct planned_column *column
) {
    int rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);

    if (rc == MYLITE_OK && planned_column_is_char_or_varchar(column)) {
        rc = append_string_key_collation_sql(string);
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
        rc = append_planned_key_part_sql(string, column);
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
    if (rc == MYLITE_OK && part->has_prefix_length && planned_column_is_string_family(column)) {
        rc = append_string_key_collation_sql(string);
    }
    if (rc == MYLITE_OK && part->sort_direction == MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC) {
        rc = mylite_dynamic_string_append(string, " DESC");
    }

    return rc;
}

static int append_loaded_key_part_sql(
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
    if (rc == MYLITE_OK && column_descriptor_is_char_or_varchar(&part->column)) {
        rc = append_string_key_collation_sql(string);
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
    if (rc == MYLITE_OK && column_descriptor_is_string_family(&part->column)) {
        rc = append_string_key_collation_sql(string);
    }

    return rc;
}

static int append_loaded_key_part_parameter_sql(
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
    if (rc == MYLITE_OK && column_descriptor_is_string_family(&part->column)) {
        rc = append_string_key_collation_sql(string);
    }

    return rc;
}

static int append_string_key_collation_sql(struct mylite_dynamic_string *string) {
    int rc = mylite_dynamic_string_append(string, " COLLATE ");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, string_key_collation_name);
    }

    return rc;
}

static int build_drop_table_sql(const char *physical_name, char **out_sql) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "DROP TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, physical_name);
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

static int build_drop_index_sql(const char *physical_name, char **out_sql) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "DROP INDEX ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, physical_name);
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

static int build_alter_table_add_column_sql(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "ALTER TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " ADD COLUMN ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, ' ');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, plan->column.physical_type);
    }
    if (rc == MYLITE_OK && !plan->column.is_nullable) {
        rc = mylite_dynamic_string_append(&string, " NOT NULL");
    }
    if (rc == MYLITE_OK) {
        rc = append_alter_table_add_column_default(database, &string, plan);
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

static int build_alter_table_add_primary_key_null_validation_sql(
    const struct planned_alter_table_add_primary_key *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT 1 FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(&string, " OR ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                &string,
                plan->parts[part_index].column.name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, " IS NULL");
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " LIMIT 1");
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

static int build_alter_table_add_primary_key_duplicate_validation_sql(
    const struct planned_alter_table_add_primary_key *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_alter_table_primary_key_expression_list_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " GROUP BY ");
    }
    if (rc == MYLITE_OK) {
        rc = append_alter_table_primary_key_expression_list_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " HAVING COUNT(*) > 1 ORDER BY ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, "MIN(");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, ')');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " LIMIT 1");
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

static int build_alter_table_add_primary_key_string_validation_sql(
    const struct planned_alter_table_add_primary_key *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    bool has_condition = false;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        const struct loaded_index_part *part = &plan->parts[part_index];

        if (!column_descriptor_is_char_or_varchar(&part->column)) {
            continue;
        }
        if (has_condition) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(&string, part->column.name);
        }
        has_condition = true;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    has_condition = false;
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        const struct loaded_index_part *part = &plan->parts[part_index];

        if (!column_descriptor_is_char_or_varchar(&part->column)) {
            continue;
        }
        if (has_condition) {
            rc = mylite_dynamic_string_append(&string, " OR ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(&string, part->column.name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, " IS NOT NULL");
        }
        has_condition = true;
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

static int build_alter_table_add_primary_key_index_sql(
    const struct planned_alter_table_add_primary_key *plan,
    const char *index_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "CREATE UNIQUE INDEX ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, index_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " ON ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(&string, &plan->parts[part_index], NULL);
        }
        if (rc == MYLITE_OK && plan->parts[part_index].index_column.sort_direction ==
                                   MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC) {
            rc = mylite_dynamic_string_append(&string, " DESC");
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, ')');
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

static int build_add_index_sql(
    const struct planned_alter_table_add_index *plan,
    const char *index_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "CREATE ");
    if (rc == MYLITE_OK && plan->is_unique) {
        rc = mylite_dynamic_string_append(&string, "UNIQUE ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, "INDEX ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, index_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " ON ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(&string, &plan->parts[part_index], NULL);
        }
        if (rc == MYLITE_OK && plan->parts[part_index].index_column.sort_direction ==
                                   MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC) {
            rc = mylite_dynamic_string_append(&string, " DESC");
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, ')');
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

static int build_create_unique_index_string_validation_sql(
    const struct planned_alter_table_add_index *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_create_unique_index_string_validation_select_list_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_create_unique_index_string_validation_where_sql(&string, plan);
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

static int append_create_unique_index_string_validation_select_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_index *plan
) {
    bool has_string_part = false;
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        const struct loaded_index_part *part = &plan->parts[part_index];

        if (!loaded_index_part_requires_string_key_validation(part)) {
            continue;
        }
        if (has_string_part) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, part->column.name);
        }
        has_string_part = true;
    }

    if (rc == MYLITE_OK && !has_string_part) {
        return MYLITE_ERROR;
    }
    return rc;
}

static int append_create_unique_index_string_validation_where_sql(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_index *plan
) {
    bool has_string_part = false;
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        const struct loaded_index_part *part = &plan->parts[part_index];

        if (!loaded_index_part_requires_string_key_validation(part)) {
            continue;
        }
        if (has_string_part) {
            rc = mylite_dynamic_string_append(string, " OR ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, part->column.name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NOT NULL");
        }
        has_string_part = true;
    }

    if (rc == MYLITE_OK && !has_string_part) {
        return MYLITE_ERROR;
    }
    return rc;
}

static int build_create_unique_index_duplicate_validation_sql(
    const struct planned_alter_table_add_index *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_loaded_key_part_list_sql(&string, plan->parts, plan->part_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " = (SELECT MIN(");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_loaded_key_part_not_null_filter_sql(&string, plan->parts, plan->part_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " GROUP BY ");
    }
    if (rc == MYLITE_OK) {
        rc = append_loaded_key_part_list_sql(&string, plan->parts, plan->part_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " HAVING COUNT(*) > 1 ORDER BY MIN(");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") LIMIT 1)");
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

static int append_loaded_key_part_list_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *parts,
    size_t part_count
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(string, &parts[part_index], NULL);
        }
    }

    return rc;
}

static int append_loaded_key_part_not_null_filter_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *parts,
    size_t part_count
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(string, &parts[part_index], NULL);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NOT NULL");
        }
    }

    return rc;
}

static int append_alter_table_primary_key_expression_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_primary_key *plan
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(string, &plan->parts[part_index], NULL);
        }
    }

    return rc;
}

static int build_alter_table_auto_increment_max_sql(
    const struct planned_alter_table_auto_increment *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT MAX(");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " IS NOT NULL");
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

static int append_alter_table_add_column_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    if (plan->column.default_kind != MYLITE_CATALOG_COLUMN_DEFAULT_NONE &&
        plan->column.default_kind != MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT) {
        return append_alter_table_add_column_explicit_default(database, string, plan);
    }
    if (plan->column.is_nullable) {
        return MYLITE_OK;
    }
    if (planned_column_is_string_family(&plan->column)) {
        return mylite_dynamic_string_append(string, " DEFAULT ''");
    }
    if (planned_column_is_json(&plan->column)) {
        return mylite_dynamic_string_append(string, " DEFAULT 'null'");
    }
    if (planned_column_is_binary_string_family(&plan->column)) {
        return append_alter_table_add_column_binary_zero(string, plan);
    }
    if (planned_column_is_bit(&plan->column)) {
        return append_alter_table_add_column_bit_zero(string, plan);
    }
    if (planned_column_is_enum(&plan->column)) {
        return append_alter_table_add_column_enum_default(database, string, plan);
    }
    if (planned_column_is_set(&plan->column)) {
        return mylite_dynamic_string_append(string, " DEFAULT ''");
    }
    if (planned_column_is_year(&plan->column)) {
        return mylite_dynamic_string_append(string, " DEFAULT '0000'");
    }
    if (planned_column_is_decimal(&plan->column)) {
        return append_alter_table_add_column_decimal_zero(database, string, plan);
    }
    if (planned_column_is_approximate(&plan->column)) {
        return mylite_dynamic_string_append(string, " DEFAULT 0");
    }
    if (planned_column_is_date(&plan->column)) {
        return mylite_dynamic_string_append(string, " DEFAULT '0000-00-00'");
    }
    if (planned_column_is_time(&plan->column)) {
        return mylite_dynamic_string_append(string, " DEFAULT '00:00:00'");
    }
    if (planned_column_is_datetime(&plan->column)) {
        return mylite_dynamic_string_append(string, " DEFAULT '0000-00-00 00:00:00'");
    }
    if (planned_column_is_timestamp(&plan->column)) {
        return mylite_dynamic_string_append(string, " DEFAULT '0000-00-00 00:00:00'");
    }

    return mylite_dynamic_string_append(string, " DEFAULT 0");
}

static int append_alter_table_add_column_explicit_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    if (planned_column_is_bit(&plan->column) &&
        plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION) {
        return append_alter_table_add_column_bit_expression_default(database, string, plan);
    }
    if (planned_column_is_year(&plan->column) &&
        plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION) {
        return append_alter_table_add_column_year_expression_default(database, string, plan);
    }
    if (column_default_kind_has_integer_value(plan->column.default_kind)) {
        char default_text[integer_text_capacity];
        int written = snprintf(
            default_text,
            sizeof(default_text),
            " DEFAULT %" PRId64,
            plan->column.default_integer
        );

        if (written < 0 || (size_t)written >= sizeof(default_text)) {
            return MYLITE_NOMEM;
        }
        return mylite_dynamic_string_append(string, default_text);
    }
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION) {
        return mylite_dynamic_string_append(string, " DEFAULT NULL");
    }
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP) {
        return append_alter_table_add_column_current_default(database, string, plan);
    }
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE) {
        return append_alter_table_add_column_current_default(database, string, plan);
    }
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME) {
        return append_alter_table_add_column_current_default(database, string, plan);
    }
    if (planned_column_is_bit(&plan->column) &&
        plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return append_alter_table_add_column_bit_default(database, string, plan);
    }
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY) {
        return append_alter_table_add_column_binary_default(database, string, plan);
    }
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION) {
        return append_alter_table_add_column_character_expression_default(database, string, plan);
    }
    if (planned_column_is_approximate(&plan->column) &&
        plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        int rc = mylite_dynamic_string_append(string, " DEFAULT ");

        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, plan->column.default_text);
        }
        return rc;
    }
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL ||
        plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return append_quoted_sql_text(string, plan->column.default_text);
    }

    return MYLITE_OK;
}

static int append_alter_table_add_column_current_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP) {
        char timestamp_text[datetime_text_length + 1U];
        int rc = format_current_timestamp_text(database, timestamp_text, sizeof(timestamp_text));

        if (rc != MYLITE_OK) {
            return rc;
        }
        return append_quoted_sql_text(string, timestamp_text);
    }
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE) {
        char date_text[date_text_length + 1U];
        int rc = format_current_date_text(database, date_text, sizeof(date_text));

        if (rc != MYLITE_OK) {
            return rc;
        }
        return append_quoted_sql_text(string, date_text);
    }
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME) {
        char time_text[time_text_minimum_length + 1U];
        int rc = format_current_time_text(database, time_text, sizeof(time_text));

        if (rc != MYLITE_OK) {
            return rc;
        }
        return append_quoted_sql_text(string, time_text);
    }

    return MYLITE_OK;
}

static int append_alter_table_add_column_character_expression_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_blob = false,
        .is_real = false,
        .integer = 0,
        .real = 0.0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(&plan->column, &descriptor);
    descriptor.default_kind = plan->column.default_kind;
    snprintf(
        descriptor.default_text,
        sizeof(descriptor.default_text),
        "%s",
        plan->column.default_text
    );
    rc = materialize_character_expression_default_value(database, &descriptor, false, &value);
    if (rc == MYLITE_OK) {
        rc = append_quoted_sql_text(string, value.text);
    }
    planned_value_deinit(&value);
    return rc;
}

static int append_alter_table_add_column_bit_expression_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_blob = false,
        .is_real = false,
        .integer = 0,
        .real = 0.0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(&plan->column, &descriptor);
    descriptor.default_kind = plan->column.default_kind;
    descriptor.default_integer = plan->column.default_integer;
    rc = materialize_bit_expression_default_value(database, &descriptor, &value);
    if (rc == MYLITE_OK) {
        rc = append_sqlite_blob_default(string, value.text, value.text_length);
    }
    planned_value_deinit(&value);
    return rc;
}

static int append_alter_table_add_column_year_expression_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_blob = false,
        .is_real = false,
        .integer = 0,
        .real = 0.0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = make_year_value(database, (uint32_t)plan->column.default_integer, &value);

    if (rc == MYLITE_OK) {
        rc = append_quoted_sql_text(string, value.text);
    }
    planned_value_deinit(&value);
    return rc;
}

static int append_alter_table_add_column_enum_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    struct enum_type_info info = {0};
    int rc = enum_type_info_for_logical_type(
        database,
        plan->column.logical_type,
        "invalid ENUM descriptor",
        &info
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (info.label_count == 0U) {
        set_unsupported_error(database, "ENUM requires at least one label");
        return MYLITE_ERROR;
    }

    return append_quoted_sql_text(string, info.labels[0].text);
}

static int append_alter_table_add_column_bit_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_blob = false,
        .is_real = false,
        .integer = 0,
        .real = 0.0,
        .text = NULL,
        .text_length = 0U,
    };
    struct mylite_sql_ast_node literal = {
        .kind = MYLITE_SQL_AST_LITERAL,
        .span =
            {
                .text = plan->column.default_text,
                .length = strlen(plan->column.default_text),
            },
        .payload.literal = {.kind = MYLITE_SQL_AST_LITERAL_BIT},
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(&plan->column, &descriptor);
    rc = convert_bit_literal(database, &literal, &descriptor, 1U, false, &value);
    if (rc == MYLITE_OK) {
        rc = append_sqlite_blob_default(string, value.text, value.text_length);
    }
    planned_value_deinit(&value);

    return rc;
}

static int append_alter_table_add_column_binary_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = decode_binary_default_text(database, plan->column.default_text, &bytes, &byte_count);

    if (rc == MYLITE_OK) {
        rc = append_sqlite_blob_default(string, bytes, byte_count);
    }
    free(bytes);
    return rc;
}

static int append_alter_table_add_column_decimal_zero(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    struct decimal_type_info info = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = decimal_type_info_for_logical_type(plan->column.logical_type, &info);

    if (rc == MYLITE_OK) {
        rc = build_decimal_zero_value(database, &info, &value);
    }
    if (rc == MYLITE_OK) {
        rc = append_quoted_sql_text(string, value.text);
    }
    planned_value_deinit(&value);

    return rc;
}

static int append_alter_table_add_column_binary_zero(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info =
        binary_string_type_info_for_logical_type(plan->column.logical_type, &storage);
    size_t byte_count = 0U;
    int rc = MYLITE_OK;

    if (info == NULL) {
        return MYLITE_ERROR;
    }
    if (info->fixed_length) {
        byte_count = (size_t)info->maximum_length;
    }

    rc = mylite_dynamic_string_append(string, " DEFAULT X'");
    for (size_t byte_index = 0U; rc == MYLITE_OK && byte_index < byte_count; ++byte_index) {
        rc = mylite_dynamic_string_append(string, "00");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }

    return rc;
}

static int append_alter_table_add_column_bit_zero(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    uint64_t width = 0U;
    size_t byte_count = 0U;
    char bytes[sizeof(uint64_t)] = {0};
    int rc = bit_width_for_logical_type(plan->column.logical_type, &width);

    if (rc != MYLITE_OK) {
        return rc;
    }
    byte_count = bit_byte_width(width);
    return append_sqlite_blob_default(string, bytes, byte_count);
}

static int append_sqlite_blob_default(
    struct mylite_dynamic_string *string,
    const char *bytes,
    size_t byte_count
) {
    static const char hex_digits[] = "0123456789ABCDEF";

    int rc = mylite_dynamic_string_append(string, " DEFAULT X'");

    for (size_t byte_index = 0U; rc == MYLITE_OK && byte_index < byte_count; ++byte_index) {
        unsigned char byte = (unsigned char)bytes[byte_index];

        rc = mylite_dynamic_string_append_char(string, hex_digits[byte >> byte_high_nibble_shift]);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, hex_digits[byte & byte_low_nibble_mask]);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }

    return rc;
}

static int append_quoted_sql_text(struct mylite_dynamic_string *string, const char *text) {
    int rc = mylite_dynamic_string_append(string, " DEFAULT '");

    for (size_t index = 0U; rc == MYLITE_OK && text[index] != '\0'; ++index) {
        if (text[index] == '\'') {
            rc = mylite_dynamic_string_append(string, "''");
        } else {
            rc = mylite_dynamic_string_append_char(string, text[index]);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }

    return rc;
}

static int append_mysql_quoted_default_text(
    struct mylite_dynamic_string *string,
    const char *text
) {
    int rc = mylite_dynamic_string_append(string, " DEFAULT ");

    if (rc == MYLITE_OK) {
        rc = append_mysql_quoted_text(string, text);
    }

    return rc;
}

static int append_mysql_quoted_text(struct mylite_dynamic_string *string, const char *text) {
    int rc = mylite_dynamic_string_append_char(string, '\'');

    for (size_t index = 0U; rc == MYLITE_OK && text[index] != '\0'; ++index) {
        switch (text[index]) {
        case '\'':
            rc = mylite_dynamic_string_append(string, "''");
            break;
        case '\\':
            rc = mylite_dynamic_string_append(string, "\\\\");
            break;
        case '\n':
            rc = mylite_dynamic_string_append(string, "\\n");
            break;
        case '\r':
            rc = mylite_dynamic_string_append(string, "\\r");
            break;
        case '\t':
            rc = mylite_dynamic_string_append(string, "\\t");
            break;
        default:
            rc = mylite_dynamic_string_append_char(string, text[index]);
            break;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }

    return rc;
}

static int build_alter_table_drop_column_sql(
    const struct planned_alter_table_drop_column *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "ALTER TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " DROP COLUMN ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->column.name);
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

static int build_alter_table_rename_column_sql(
    const struct planned_alter_table_rename_column *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "ALTER TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " RENAME COLUMN ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " TO ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->new_column_name);
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

static int build_modify_temporary_physical_name(
    const struct planned_alter_table_modify_column *plan,
    const struct mylite_catalog_mutation *mutation,
    char *destination,
    size_t destination_size
) {
    int written = 0;

    if (plan == NULL || mutation == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_MISUSE;
    }

    written = snprintf(
        destination,
        destination_size,
        "_mylite_user_table_%" PRId64 "_modify_%" PRIu64,
        plan->table.table_id,
        mylite_catalog_mutation_generation(mutation)
    );
    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_NOMEM;
    }

    return MYLITE_OK;
}

static int build_alter_table_modify_validation_sql(
    const struct planned_alter_table_modify_column *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->original_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " ORDER BY ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, plan->rowid_alias);
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

static int build_alter_table_modify_create_sql(
    const struct planned_alter_table_modify_column *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "CREATE TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, temporary_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const struct mylite_catalog_column_descriptor *column = &plan->columns[column_index];

        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(&string, column->name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(&string, ' ');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, column->physical_type);
        }
        if (rc == MYLITE_OK && !column->is_nullable) {
            rc = mylite_dynamic_string_append(&string, " NOT NULL");
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, ')');
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

static int build_alter_table_modify_copy_sql(
    const struct planned_alter_table_modify_column *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "INSERT INTO ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, temporary_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                &string,
                plan->columns[column_index].name
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") SELECT ");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const char *source_name = plan->columns[column_index].name;

        if (plan->columns[column_index].column_id == plan->original_column.column_id) {
            source_name = plan->original_column.name;
        }
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(&string, source_name);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " ORDER BY ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, plan->rowid_alias);
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

static bool alter_table_modify_needs_binary_copy_materialization(
    const struct planned_alter_table_modify_column *plan
) {
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        const struct mylite_catalog_column_descriptor *column = &plan->columns[column_index];

        if (column->column_id == plan->original_column.column_id &&
            column_descriptor_is_binary_string_family(column)) {
            return true;
        }
    }

    return false;
}

static int execute_physical_alter_table_modify_copy_rows(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan,
    const char *temporary_physical_name
) {
    struct alter_table_modify_copy_statements statements = {0};
    struct planned_value *values = NULL;
    char *select_sql = NULL;
    char *insert_sql = NULL;
    size_t row_number = 0U;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    if (plan->column_count > (size_t)INT_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = calloc(plan->column_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = build_alter_table_modify_select_sql(plan, &select_sql);
    if (rc == MYLITE_OK) {
        rc = build_alter_table_modify_insert_sql(plan, temporary_physical_name, &insert_sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, select_sql, &statements.select_statement);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, insert_sql, &statements.insert_statement);
    }
    while (rc == MYLITE_OK &&
           (sqlite_rc = sqlite3_step(statements.select_statement)) == SQLITE_ROW) {
        if (row_number >= (size_t)INT64_MAX) {
            set_runtime_error(database, plan->row_count_overflow_message);
            rc = MYLITE_ERROR;
            break;
        }
        ++row_number;
        rc = execute_alter_table_modify_copy_row(database, &statements, plan, row_number, values);
        deinit_alter_table_modify_copy_row(values, plan->column_count);
    }
    if (rc == MYLITE_OK && sqlite_rc != SQLITE_DONE) {
        rc = mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    deinit_alter_table_modify_copy_row(values, plan->column_count);
    rc = finalize_sqlite_statement(statements.insert_statement, rc);
    statements.insert_statement = NULL;
    rc = finalize_sqlite_statement(statements.select_statement, rc);
    free(values);
    free(select_sql);
    free(insert_sql);
    return rc;
}

static int build_alter_table_modify_select_sql(
    const struct planned_alter_table_modify_column *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const char *source_name = plan->columns[column_index].name;

        if (plan->columns[column_index].column_id == plan->original_column.column_id) {
            source_name = plan->original_column.name;
        }
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(&string, source_name);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " ORDER BY ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, plan->rowid_alias);
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

static int build_alter_table_modify_insert_sql(
    const struct planned_alter_table_modify_column *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "INSERT INTO ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, temporary_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                &string,
                plan->columns[column_index].name
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") VALUES (");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_parameters(&string, plan->column_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, ')');
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

static int execute_alter_table_modify_copy_row(
    struct mylite_db *database,
    struct alter_table_modify_copy_statements *statements,
    const struct planned_alter_table_modify_column *plan,
    size_t row_number,
    struct planned_value *values
) {
    int sqlite_rc = SQLITE_OK;
    int rc = materialize_alter_table_modify_copy_row(
        database,
        statements->select_statement,
        plan,
        row_number,
        values
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = bind_alter_table_modify_copy_row(statements->insert_statement, plan, values);
    if (rc != MYLITE_OK) {
        return rc;
    }
    sqlite_rc = sqlite3_step(statements->insert_statement);
    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    sqlite_rc = sqlite3_reset(statements->insert_statement);
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    sqlite_rc = sqlite3_clear_bindings(statements->insert_statement);
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int materialize_alter_table_modify_copy_row(
    struct mylite_db *database,
    sqlite3_stmt *select_statement,
    const struct planned_alter_table_modify_column *plan,
    size_t row_number,
    struct planned_value *values
) {
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        const struct mylite_catalog_column_descriptor *column = &plan->columns[column_index];
        int rc = MYLITE_OK;

        if (column_index > (size_t)INT_MAX) {
            return MYLITE_ERROR;
        }
        if (column->column_id == plan->original_column.column_id &&
            column_descriptor_is_binary_string_family(column)) {
            if (sqlite3_column_type(select_statement, (int)column_index) == SQLITE_NULL) {
                values[column_index] = (struct planned_value){.is_null = true};
            } else {
                rc = materialize_sqlite_binary_string_value(
                    database,
                    select_statement,
                    (int)column_index,
                    column,
                    row_number,
                    &values[column_index]
                );
            }
        } else {
            rc = materialize_insert_select_raw_value(
                database,
                select_statement,
                (int)column_index,
                &values[column_index]
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int bind_alter_table_modify_copy_row(
    sqlite3_stmt *insert_statement,
    const struct planned_alter_table_modify_column *plan,
    const struct planned_value *values
) {
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        int rc = MYLITE_OK;

        if (column_index >= (size_t)INT_MAX) {
            return MYLITE_ERROR;
        }
        rc = bind_planned_value_parameter(
            insert_statement,
            (int)column_index + 1,
            &values[column_index]
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static void deinit_alter_table_modify_copy_row(struct planned_value *values, size_t column_count) {
    if (values == NULL) {
        return;
    }
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        planned_value_deinit(&values[column_index]);
    }
}

static int build_alter_table_modify_indexes_sql(
    const struct planned_alter_table_modify_column *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_count; ++index) {
        rc = append_alter_table_modify_index_sql(&string, plan, &plan->indexes[index]);
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

static int append_alter_table_modify_index_sql(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_modify_column *plan,
    const struct loaded_index_info *index
) {
    bool is_unique = index->index.is_unique;
    int rc = MYLITE_OK;

    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        return MYLITE_OK;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
        is_unique = true;
    }
    if (string->length != 0U) {
        rc = mylite_dynamic_string_append(string, "; ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "CREATE ");
    }
    if (rc == MYLITE_OK && is_unique) {
        rc = mylite_dynamic_string_append(string, "UNIQUE ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "INDEX ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, index->index.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " ON ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " (");
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        const struct loaded_index_part *part = &index->parts[part_index];

        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(string, part, NULL);
        }
        if (rc == MYLITE_OK &&
            part->index_column.sort_direction == MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC) {
            rc = mylite_dynamic_string_append(string, " DESC");
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int build_alter_table_order_temporary_physical_name(
    const struct planned_alter_table_order_by *plan,
    uint64_t sqlite_schema_generation,
    char *destination,
    size_t destination_size
) {
    int written = 0;

    if (plan == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_MISUSE;
    }

    written = snprintf(
        destination,
        destination_size,
        "_mylite_user_table_%" PRId64 "_order_%" PRIu64,
        plan->table.table_id,
        sqlite_schema_generation
    );
    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_NOMEM;
    }

    return MYLITE_OK;
}

static int build_alter_table_order_create_sql(
    const struct planned_alter_table_order_by *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "CREATE TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, temporary_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const struct mylite_catalog_column_descriptor *column = &plan->columns[column_index];

        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(&string, column->name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(&string, ' ');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, column->physical_type);
        }
        if (rc == MYLITE_OK && !column->is_nullable) {
            rc = mylite_dynamic_string_append(&string, " NOT NULL");
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, ')');
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

static int build_alter_table_order_copy_sql(
    const struct planned_alter_table_order_by *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "INSERT INTO ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, temporary_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    if (rc == MYLITE_OK) {
        rc = append_alter_table_order_column_list(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_alter_table_order_column_list(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " ORDER BY ");
    }
    if (rc == MYLITE_OK) {
        rc = append_alter_table_order_order_list(&string, plan);
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

static int append_alter_table_order_column_list(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_order_by *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->columns[column_index].name
            );
        }
    }

    return rc;
}

static int append_alter_table_order_order_list(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_order_by *plan
) {
    int rc = MYLITE_OK;

    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->item_count; ++item_index) {
        const struct planned_alter_table_order_by_item *item = &plan->items[item_index];

        if (item_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, item->column.name);
        }
        if (rc == MYLITE_OK) {
            if (item->direction == PLANNED_SELECT_ORDER_DESC) {
                rc = mylite_dynamic_string_append(string, " DESC");
            } else {
                rc = mylite_dynamic_string_append(string, " ASC");
            }
        }
    }

    return rc;
}

static int build_alter_table_force_temporary_physical_name(
    const struct planned_alter_table_force *plan,
    uint64_t sqlite_schema_generation,
    char *destination,
    size_t destination_size
) {
    int written = 0;

    if (plan == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_MISUSE;
    }

    written = snprintf(
        destination,
        destination_size,
        "_mylite_user_table_%" PRId64 "_force_%" PRIu64,
        plan->table.table_id,
        sqlite_schema_generation
    );
    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_NOMEM;
    }

    return MYLITE_OK;
}

static int build_alter_table_force_create_sql(
    const struct planned_alter_table_force *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "CREATE TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, temporary_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const struct mylite_catalog_column_descriptor *column = &plan->columns[column_index];

        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(&string, column->name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(&string, ' ');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, column->physical_type);
        }
        if (rc == MYLITE_OK && !column->is_nullable) {
            rc = mylite_dynamic_string_append(&string, " NOT NULL");
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, ')');
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

static int build_alter_table_force_copy_sql(
    const struct planned_alter_table_force *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "INSERT INTO ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, temporary_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    if (rc == MYLITE_OK) {
        rc = append_alter_table_force_column_list(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_alter_table_force_column_list(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
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

static int append_alter_table_force_column_list(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_force *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->columns[column_index].name
            );
        }
    }

    return rc;
}

static int build_alter_table_rename_physical_table_sql(
    const char *source_physical_name,
    const char *target_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "ALTER TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, source_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " RENAME TO ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, target_physical_name);
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

static int build_truncate_table_sql(const struct planned_truncate_table *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "DELETE FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
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

static int build_insert_sql(const struct planned_insert *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    size_t physical_column_count = count_physical_insert_columns(plan);
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "INSERT INTO ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_column_names(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") VALUES (");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_parameters(&string, physical_column_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, ')');
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

static int append_insert_column_names(
    struct mylite_dynamic_string *string,
    const struct planned_insert *plan
) {
    int rc = MYLITE_OK;
    bool has_previous = false;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (plan->columns[column_index].is_generated) {
            continue;
        }
        if (has_previous) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->columns[column_index].name
            );
        }
        has_previous = true;
    }

    return rc;
}

static size_t count_physical_insert_columns(const struct planned_insert *plan) {
    size_t count = 0U;

    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (!plan->columns[column_index].is_generated) {
            ++count;
        }
    }
    return count;
}

static int append_insert_parameters(struct mylite_dynamic_string *string, size_t column_count) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, column_index + 1U);
        }
    }

    return rc;
}

static int append_numbered_parameter(struct mylite_dynamic_string *string, size_t parameter_index) {
    char parameter[integer_text_capacity];
    int written = snprintf(parameter, sizeof(parameter), "?%zu", parameter_index);

    if (written < 0 || (size_t)written >= sizeof(parameter)) {
        return MYLITE_NOMEM;
    }

    return mylite_dynamic_string_append(string, parameter);
}

static int append_size_literal(struct mylite_dynamic_string *string, size_t value) {
    char text[integer_text_capacity];
    int written = snprintf(text, sizeof(text), "%zu", value);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        return MYLITE_NOMEM;
    }

    return mylite_dynamic_string_append(string, text);
}

static int append_uint64_literal(struct mylite_dynamic_string *string, uint64_t value) {
    char text[integer_text_capacity];
    int written = snprintf(text, sizeof(text), "%" PRIu64, value);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        return MYLITE_NOMEM;
    }

    return mylite_dynamic_string_append(string, text);
}

static int build_insert_select_temp_table_name(
    const struct mylite_db *database,
    char *destination,
    size_t destination_size
) {
    int written = snprintf(
        destination,
        destination_size,
        "_mylite_insert_select_%" PRIu64,
        database->session.connection_id
    );

    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int build_insert_select_materialize_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
) {
    if (plan->source_kind == PLANNED_INSERT_SELECT_SOURCE_COMPOUND) {
        return build_insert_select_compound_materialize_sql(plan, temporary_table_name, out_sql);
    }

    return build_insert_select_table_materialize_sql(plan, temporary_table_name, out_sql);
}

static int build_insert_select_table_materialize_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "CREATE TEMP TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, temporary_table_name);
    }
    if (rc == MYLITE_OK) {
        if (plan->source.is_distinct) {
            rc = mylite_dynamic_string_append(&string, " AS SELECT DISTINCT ");
        } else {
            rc = mylite_dynamic_string_append(&string, " AS SELECT ");
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_source_projection(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            plan->source.table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->source.predicate, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_order_sql(&string, &plan->source.order, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_limit_sql(&string, &plan->source.limit, &next_parameter);
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

static int build_insert_select_compound_materialize_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "CREATE TEMP TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, temporary_table_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AS ");
    }
    for (size_t branch_index = 0U;
         rc == MYLITE_OK && branch_index < plan->compound_source.branch_count;
         ++branch_index) {
        const struct planned_insert_select_compound_branch *branch =
            &plan->compound_source.branches[branch_index];

        if (branch_index != 0U) {
            rc = mylite_dynamic_string_append(&string, " UNION ALL ");
        }
        if (rc == MYLITE_OK) {
            rc = append_insert_select_compound_branch_sql(
                &string,
                branch,
                branch_index,
                branch_index == 0U,
                &next_parameter
            );
        }
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

static int append_insert_select_compound_branch_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select_compound_branch *branch,
    size_t branch_index,
    bool alias_columns,
    size_t *next_parameter
) {
    if (branch->kind == PLANNED_INSERT_SELECT_COMPOUND_BRANCH_ROW_SCALAR) {
        return append_insert_select_compound_row_scalar_branch_sql(
            string,
            branch,
            branch_index,
            alias_columns,
            next_parameter
        );
    }

    return append_insert_select_compound_table_branch_sql(
        string,
        branch,
        branch_index,
        alias_columns,
        next_parameter
    );
}

static int append_insert_select_compound_table_branch_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select_compound_branch *branch,
    size_t branch_index,
    bool alias_columns,
    size_t *next_parameter
) {
    const struct planned_select *source = &branch->source;
    int rc = MYLITE_OK;

    if (source->is_distinct) {
        rc = mylite_dynamic_string_append(string, "SELECT DISTINCT ");
    } else {
        rc = mylite_dynamic_string_append(string, "SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_compound_branch_marker_sql(string, branch_index, alias_columns);
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < source->column_count;
         ++column_index) {
        rc = mylite_dynamic_string_append(string, ", ");
        if (rc == MYLITE_OK) {
            rc = append_insert_select_compound_column_projection_sql(string, source, column_index);
        }
        if (rc == MYLITE_OK && alias_columns) {
            rc = mylite_dynamic_string_append(string, " AS ");
        }
        if (rc == MYLITE_OK && alias_columns) {
            rc = append_insert_select_temp_column_name(string, column_index);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_from_sql(string, source);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(string, &source->predicate, next_parameter);
    }

    return rc;
}

static int append_insert_select_compound_branch_marker_sql(
    struct mylite_dynamic_string *string,
    size_t branch_index,
    bool alias_columns
) {
    int rc = append_size_literal(string, branch_index);

    if (rc == MYLITE_OK && alias_columns) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK && alias_columns) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            string,
            insert_select_union_branch_column_name
        );
    }

    return rc;
}

static int append_insert_select_compound_column_projection_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select *source,
    size_t column_index
) {
    const struct mylite_catalog_column_descriptor *column = &source->columns[column_index];
    int rc = append_descriptor_column_name_sql_for_source(
        string,
        column,
        source->column_source_indexes[column_index],
        planned_select_qualifies_source_references(source)
    );

    if (rc == MYLITE_OK && column_descriptor_is_string_family(column)) {
        rc = append_string_key_collation_sql(string);
    }

    return rc;
}

static int append_insert_select_compound_row_scalar_branch_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select_compound_branch *branch,
    size_t branch_index,
    bool alias_columns,
    size_t *next_parameter
) {
    const struct planned_row_scalar_select *source = &branch->row_source;
    int rc = mylite_dynamic_string_append(string, "SELECT ");

    if (rc == MYLITE_OK) {
        rc = append_insert_select_compound_branch_marker_sql(string, branch_index, alias_columns);
    }
    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < source->item_count; ++item_index) {
        rc = mylite_dynamic_string_append(string, ", ");
        if (rc == MYLITE_OK) {
            rc = append_insert_select_compound_row_scalar_projection_sql(
                string,
                &source->items[item_index].expression,
                next_parameter
            );
        }
        if (rc == MYLITE_OK && alias_columns) {
            rc = mylite_dynamic_string_append(string, " AS ");
        }
        if (rc == MYLITE_OK && alias_columns) {
            rc = append_insert_select_temp_column_name(string, item_index);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_tableless_filter_sql(string, source, next_parameter);
    }

    return rc;
}

static int append_insert_select_compound_row_scalar_projection_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = append_row_scalar_expression_sql(string, expression, next_parameter);

    if (rc == MYLITE_OK && row_scalar_expression_uses_string_collation(expression)) {
        rc = append_string_key_collation_sql(string);
    }

    return rc;
}

static bool row_scalar_expression_uses_string_collation(
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL) {
        return false;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        return expression->value.is_text;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return column_descriptor_is_string_family(&expression->column);
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
        return true;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
        return (expression->conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_CHAR ||
                expression->conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_USING_CHARSET) != 0;
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
        return expression->field_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
        break;
    }

    return false;
}

static int build_insert_select_validation_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
) {
    if (plan->source_kind == PLANNED_INSERT_SELECT_SOURCE_COMPOUND) {
        return build_insert_select_compound_validation_sql(plan, temporary_table_name, out_sql);
    }

    return build_insert_select_table_validation_sql(plan, temporary_table_name, out_sql);
}

static int build_insert_select_table_validation_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->target_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_insert_select_temp_column_name(&string, column_index);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_temp_table_name(&string, temporary_table_name);
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

static int build_insert_select_compound_validation_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t last_distinct_branch = 0U;
    bool has_distinct_branch =
        insert_select_compound_last_distinct_branch(plan, &last_distinct_branch);
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->target_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_insert_select_compound_qualified_temp_column_name(
                &string,
                insert_select_union_current_alias,
                column_index
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_compound_qualified_branch_column_name(
            &string,
            insert_select_union_current_alias
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_temp_table_name(&string, temporary_table_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            insert_select_union_current_alias
        );
    }
    if (rc == MYLITE_OK && has_distinct_branch) {
        rc = append_insert_select_compound_distinct_filter_sql(
            &string,
            plan,
            temporary_table_name,
            last_distinct_branch
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " ORDER BY ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            insert_select_union_current_alias
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ".rowid");
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

static bool insert_select_compound_last_distinct_branch(
    const struct planned_insert_select *plan,
    size_t *out_branch_index
) {
    bool found = false;

    *out_branch_index = 0U;
    for (size_t branch_index = 1U; branch_index < plan->compound_source.branch_count;
         ++branch_index) {
        if (plan->compound_source.branches[branch_index].modifier !=
            MYLITE_SQL_AST_UNION_MODIFIER_ALL) {
            *out_branch_index = branch_index;
            found = true;
        }
    }

    return found;
}

static int append_insert_select_compound_distinct_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    size_t last_distinct_branch
) {
    int rc = mylite_dynamic_string_append(string, " WHERE (");

    if (rc == MYLITE_OK) {
        rc = append_insert_select_compound_qualified_branch_column_name(
            string,
            insert_select_union_current_alias
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " > ");
    }
    if (rc == MYLITE_OK) {
        rc = append_size_literal(string, last_distinct_branch);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " OR NOT EXISTS (SELECT 1 FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_temp_table_name(string, temporary_table_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc =
            mylite_dynamic_string_append_quoted_identifier(string, insert_select_union_prior_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc =
            mylite_dynamic_string_append_quoted_identifier(string, insert_select_union_prior_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ".rowid < ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            string,
            insert_select_union_current_alias
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ".rowid");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->target_count;
         ++column_index) {
        rc = mylite_dynamic_string_append(string, " AND ");
        if (rc == MYLITE_OK) {
            rc = append_insert_select_compound_distinct_equality_sql(string, plan, column_index);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "))");
    }

    return rc;
}

static int append_insert_select_compound_distinct_equality_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select *plan,
    size_t column_index
) {
    bool use_string_collation =
        insert_select_compound_output_uses_string_collation(plan, column_index);
    int rc = mylite_dynamic_string_append(string, "((");

    if (rc == MYLITE_OK) {
        rc = append_insert_select_compound_qualified_temp_column_name(
            string,
            insert_select_union_prior_alias,
            column_index
        );
    }
    if (rc == MYLITE_OK && use_string_collation) {
        rc = append_string_key_collation_sql(string);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " = ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_compound_qualified_temp_column_name(
            string,
            insert_select_union_current_alias,
            column_index
        );
    }
    if (rc == MYLITE_OK && use_string_collation) {
        rc = append_string_key_collation_sql(string);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") OR (");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_compound_qualified_temp_column_name(
            string,
            insert_select_union_prior_alias,
            column_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL AND ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_compound_qualified_temp_column_name(
            string,
            insert_select_union_current_alias,
            column_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL))");
    }

    return rc;
}

static int append_insert_select_compound_qualified_temp_column_name(
    struct mylite_dynamic_string *string,
    const char *alias,
    size_t column_index
) {
    int rc = mylite_dynamic_string_append_quoted_identifier(string, alias);

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '.');
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_temp_column_name(string, column_index);
    }

    return rc;
}

static int append_insert_select_compound_qualified_branch_column_name(
    struct mylite_dynamic_string *string,
    const char *alias
) {
    int rc = mylite_dynamic_string_append_quoted_identifier(string, alias);

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '.');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            string,
            insert_select_union_branch_column_name
        );
    }

    return rc;
}

static bool insert_select_compound_output_uses_string_collation(
    const struct planned_insert_select *plan,
    size_t column_index
) {
    for (size_t branch_index = 0U; branch_index < plan->compound_source.branch_count;
         ++branch_index) {
        const struct planned_insert_select_compound_branch *branch =
            &plan->compound_source.branches[branch_index];

        if (branch->kind == PLANNED_INSERT_SELECT_COMPOUND_BRANCH_TABLE) {
            if (column_index < branch->source.column_count &&
                column_descriptor_is_string_family(&branch->source.columns[column_index])) {
                return true;
            }
        } else if (
            column_index < branch->row_source.item_count &&
            row_scalar_expression_uses_string_collation(
                &branch->row_source.items[column_index].expression
            )
        ) {
            return true;
        }
    }

    return false;
}

static int build_drop_temp_table_sql(const char *temporary_table_name, char **out_sql) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "DROP TABLE IF EXISTS ");
    if (rc == MYLITE_OK) {
        rc = append_insert_select_temp_table_name(&string, temporary_table_name);
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

static int append_insert_select_source_projection(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->source.column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->source.columns[column_index].name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " AS ");
        }
        if (rc == MYLITE_OK) {
            rc = append_insert_select_temp_column_name(string, column_index);
        }
    }

    return rc;
}

static int append_insert_select_temp_column_name(
    struct mylite_dynamic_string *string,
    size_t column_index
) {
    char column_name[integer_text_capacity + sizeof("_mylite_value_")];
    int written = snprintf(column_name, sizeof(column_name), "_mylite_value_%zu", column_index);

    if (written < 0 || (size_t)written >= sizeof(column_name)) {
        return MYLITE_NOMEM;
    }

    return mylite_dynamic_string_append_quoted_identifier(string, column_name);
}

static int append_insert_select_temp_table_name(
    struct mylite_dynamic_string *string,
    const char *temporary_table_name
) {
    int rc = mylite_dynamic_string_append(string, "temp.");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, temporary_table_name);
    }

    return rc;
}

static bool find_insert_select_target_position(
    const struct planned_insert_select *plan,
    size_t column_index,
    size_t *out_target_position
) {
    for (size_t target_position = 0U; target_position < plan->target_count; ++target_position) {
        if (plan->target_indexes[target_position] == column_index) {
            *out_target_position = target_position;
            return true;
        }
    }

    return false;
}

static int build_create_table_select_insert_sql(
    const struct planned_create_table_select *plan,
    const char *physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "INSERT INTO ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    if (rc == MYLITE_OK) {
        rc = append_create_table_select_target_column_names(&string, plan);
    }
    if (rc == MYLITE_OK) {
        if (plan->source.is_distinct) {
            rc = mylite_dynamic_string_append(&string, ") SELECT DISTINCT ");
        } else {
            rc = mylite_dynamic_string_append(&string, ") SELECT ");
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_create_table_select_source_projection(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            plan->source.table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->source.predicate, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_order_sql(&string, &plan->source.order, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_limit_sql(&string, &plan->source.limit, &next_parameter);
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

static int append_create_table_select_target_column_names(
    struct mylite_dynamic_string *string,
    const struct planned_create_table_select *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < plan->create_table.column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->create_table.columns[column_index].name
            );
        }
    }

    return rc;
}

static int append_create_table_select_source_projection(
    struct mylite_dynamic_string *string,
    const struct planned_create_table_select *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->source.column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->source.columns[column_index].name
            );
        }
    }

    return rc;
}

static int build_select_sql(const struct planned_select *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    if (plan->is_distinct) {
        rc = mylite_dynamic_string_append(&string, "SELECT DISTINCT ");
    } else {
        rc = mylite_dynamic_string_append(&string, "SELECT ");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_select_projection_column_sql(&string, plan, column_index);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_from_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_order_sql(&string, &plan->order, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_limit_sql(&string, &plan->limit, &next_parameter);
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

static int append_select_projection_column_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select *plan,
    size_t column_index
) {
    const struct mylite_catalog_column_descriptor *column = &plan->columns[column_index];
    int rc = append_descriptor_column_name_sql_for_source(
        string,
        column,
        plan->column_source_indexes[column_index],
        planned_select_qualifies_source_references(plan)
    );

    if (rc == MYLITE_OK && plan->is_distinct && column_descriptor_is_string_family(column)) {
        rc = append_string_key_collation_sql(string);
    }

    return rc;
}

static int append_select_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select *plan
) {
    int rc = MYLITE_OK;

    if (plan->source_count == 0U) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->table.physical_name);
        if (rc == MYLITE_OK && plan->requires_source_alias) {
            rc = mylite_dynamic_string_append(string, " AS ");
        }
        if (rc == MYLITE_OK && plan->requires_source_alias) {
            rc = append_select_source_alias(string, 0U);
        }
        return rc;
    }
    if (plan->join_count != plan->source_count - 1U || plan->join_kinds == NULL ||
        plan->join_conditions == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append_quoted_identifier(
        string,
        plan->sources[0].table.physical_name
    );
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_source_alias(string, 0U);
    }

    for (size_t join_index = 0U; rc == MYLITE_OK && join_index < plan->join_count; ++join_index) {
        size_t right_source_index = join_index + 1U;

        if (plan->join_kinds[join_index] == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER) {
            rc = mylite_dynamic_string_append(string, " LEFT JOIN ");
        } else if (plan->join_kinds[join_index] == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER) {
            rc = mylite_dynamic_string_append(string, " RIGHT JOIN ");
        } else {
            rc = mylite_dynamic_string_append(string, " JOIN ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->sources[right_source_index].table.physical_name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " AS ");
        }
        if (rc == MYLITE_OK) {
            rc = append_select_source_alias(string, right_source_index);
        }
        if (rc == MYLITE_OK) {
            rc = append_select_join_condition_sql(string, &plan->join_conditions[join_index]);
        }
    }

    return rc;
}

static bool planned_select_qualifies_source_references(const struct planned_select *plan) {
    if (plan == NULL) {
        return false;
    }
    return (plan->source_count > 0U || plan->requires_source_alias) != 0;
}

static int append_select_join_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_join_condition *condition
) {
    int rc = MYLITE_OK;

    if (!condition->has_condition) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " ON ");
    if (rc == MYLITE_OK) {
        rc = append_descriptor_value_sql_for_source(
            string,
            &condition->left_column,
            condition->left_source_index,
            true
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " = ");
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_value_sql_for_source(
            string,
            &condition->right_column,
            condition->right_source_index,
            true
        );
    }

    return rc;
}

static int append_select_source_alias(struct mylite_dynamic_string *string, size_t source_index) {
    char alias[select_source_alias_capacity];
    int written = snprintf(alias, sizeof(alias), "_mylite_s%zu", source_index);

    if (written < 0 || (size_t)written >= sizeof(alias)) {
        return MYLITE_NOMEM;
    }
    return mylite_dynamic_string_append_quoted_identifier(string, alias);
}

static int build_row_scalar_select_sql(
    const struct planned_row_scalar_select *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->item_count; ++item_index) {
        if (item_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_expression_sql(
                &string,
                &plan->items[item_index].expression,
                &next_parameter
            );
        }
    }
    if (rc == MYLITE_OK && plan->has_source) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK && plan->has_source) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK && plan->has_source) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
    }
    if (rc == MYLITE_OK && plan->has_source) {
        rc = append_select_order_sql(&string, &plan->order, &next_parameter);
    }
    if (rc == MYLITE_OK && plan->has_source) {
        rc = append_select_limit_sql(&string, &plan->limit, &next_parameter);
    }
    if (rc == MYLITE_OK && !plan->has_source) {
        rc = append_row_scalar_tableless_filter_sql(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK && !plan->has_source) {
        rc = append_select_limit_sql(&string, &plan->limit, &next_parameter);
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

static int append_row_scalar_tableless_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_select *plan,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (!plan->tableless_filter.has_filter) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " WHERE ");
    if (rc == MYLITE_OK && plan->tableless_filter.has_scalar_predicate) {
        return append_select_predicate_expression_sql_without_exists(
            string,
            &plan->tableless_filter.predicate,
            next_parameter
        );
    }
    if (rc == MYLITE_OK && plan->tableless_filter.negate) {
        rc = mylite_dynamic_string_append(string, "NOT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_exists_predicate_sql(
            string,
            &plan->tableless_filter.subquery,
            next_parameter
        );
    }

    return rc;
}

static int append_row_scalar_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = append_numbered_parameter(string, *next_parameter);
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return mylite_dynamic_string_append_quoted_identifier(string, expression->column.name);
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
        return append_row_scalar_window_function_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
        return append_row_scalar_integer_arithmetic_expression_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
        return append_row_scalar_concat_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
        return append_row_scalar_concat_ws_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
        return append_row_scalar_field_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
        return append_row_scalar_greatest_least_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
        return append_row_scalar_interval_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
        return append_row_scalar_date_format_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
        return append_row_scalar_date_format_numeric_equal_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
        return append_row_scalar_date_interval_second_expression_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
        return append_row_scalar_datediff_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
        return append_row_scalar_timediff_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
        return append_row_scalar_timestampdiff_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
        return append_row_scalar_timestamp_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
        return append_row_scalar_string_length_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
        return append_row_scalar_string_codepoint_expression_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
        return append_row_scalar_string_case_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
        return append_row_scalar_string_trim_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
        return append_row_scalar_unix_timestamp_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
        return append_row_scalar_temporal_extract_expression_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
        return append_row_scalar_sec_to_time_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
        return append_row_scalar_from_unixtime_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
        return append_row_scalar_temporal_constructor_expression_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_period_add",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_period_diff",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_convert_tz",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
        return append_row_scalar_string_slice_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
        return append_row_scalar_string_padding_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
        return append_row_scalar_string_bitmask_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
        return append_row_scalar_string_search_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
        return append_row_scalar_string_replace_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
        return append_row_scalar_string_insert_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
        return append_row_scalar_string_reverse_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
        return append_row_scalar_soundex_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
        return append_row_scalar_string_quote_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
        return append_row_scalar_substring_index_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
        return append_row_scalar_find_in_set_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
        return append_row_scalar_strcmp_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
        return append_row_scalar_regexp_like_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
        return append_row_scalar_regexp_string_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
        return append_row_scalar_json_valid_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
        return append_row_scalar_json_search_expression_sql(
            string,
            "_mylite_json_contains",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
        return append_row_scalar_json_search_expression_sql(
            string,
            "_mylite_json_contains_path",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
        return append_row_scalar_json_extract_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
        return append_row_scalar_json_value_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
        return append_row_scalar_json_unquote_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
        return append_row_scalar_json_quote_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
        return append_row_scalar_json_unquote_extract_expression_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
        return append_row_scalar_json_length_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
        return append_row_scalar_json_keys_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
        return append_row_scalar_json_type_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
        return append_row_scalar_json_set_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
        return append_row_scalar_json_constructor_expression_sql(
            string,
            "_mylite_json_array",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
        return append_row_scalar_json_constructor_expression_sql(
            string,
            "_mylite_json_object",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
        return append_row_scalar_hex_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
        return append_row_scalar_base64_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
        return append_row_scalar_unhex_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_weight_string",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_weight_string_binary",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
        return append_row_scalar_uuid_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
        return append_row_scalar_char_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        return append_row_scalar_control_flow_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        return append_row_scalar_rand_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
        return append_row_scalar_conversion_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
        break;
    }

    return MYLITE_ERROR;
}

static int append_row_scalar_window_function_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION ||
        next_parameter == NULL) {
        return MYLITE_ERROR;
    }
    function_name = window_function_sql_name(expression->window_function_kind);
    if (function_name == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_window_function_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") OVER (");
    }
    if (rc == MYLITE_OK && expression->window_has_partition) {
        rc = mylite_dynamic_string_append(string, "PARTITION BY ");
    }
    if (rc == MYLITE_OK && expression->window_has_partition) {
        rc = append_descriptor_value_sql_for_source(
            string,
            &expression->window_partition_column,
            0U,
            false
        );
    }
    if (rc == MYLITE_OK && expression->window_has_partition && expression->window_has_order) {
        rc = mylite_dynamic_string_append_char(string, ' ');
    }
    if (rc == MYLITE_OK && expression->window_has_order) {
        rc = mylite_dynamic_string_append(string, "ORDER BY ");
    }
    if (rc == MYLITE_OK && expression->window_has_order) {
        rc = append_descriptor_value_sql_for_source(
            string,
            &expression->window_order_column,
            0U,
            false
        );
    }
    if (rc == MYLITE_OK && expression->window_has_order) {
        rc = append_select_order_direction_sql(string, expression->window_order_direction);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_window_function_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *argument,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (argument == NULL || next_parameter == NULL) {
        return MYLITE_ERROR;
    }
    switch (argument->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = append_numbered_parameter(string, *next_parameter);
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_descriptor_value_sql_for_source(string, &argument->column, 0U, false);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    }
    return MYLITE_ERROR;
}

static const char *window_function_sql_name(enum planned_window_function_kind kind) {
    switch (kind) {
    case PLANNED_WINDOW_FUNCTION_ROW_NUMBER:
        return "row_number";
    case PLANNED_WINDOW_FUNCTION_RANK:
        return "rank";
    case PLANNED_WINDOW_FUNCTION_DENSE_RANK:
        return "dense_rank";
    case PLANNED_WINDOW_FUNCTION_PERCENT_RANK:
        return "percent_rank";
    case PLANNED_WINDOW_FUNCTION_CUME_DIST:
        return "cume_dist";
    case PLANNED_WINDOW_FUNCTION_NTILE:
        return "ntile";
    case PLANNED_WINDOW_FUNCTION_LAG:
        return "lag";
    case PLANNED_WINDOW_FUNCTION_LEAD:
        return "lead";
    case PLANNED_WINDOW_FUNCTION_FIRST_VALUE:
        return "first_value";
    case PLANNED_WINDOW_FUNCTION_LAST_VALUE:
        return "last_value";
    case PLANNED_WINDOW_FUNCTION_NTH_VALUE:
        return "nth_value";
    case PLANNED_WINDOW_FUNCTION_NONE:
        break;
    }
    return NULL;
}

static int append_row_scalar_rand_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_RAND ||
        next_parameter == NULL) {
        return MYLITE_ERROR;
    }
    if (!expression->has_rand_seed) {
        return mylite_dynamic_string_append(string, "_mylite_rand()");
    }

    rc = mylite_dynamic_string_append(string, "_mylite_rand_seeded(");
    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_conversion_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U ||
        expression->conversion_step_count == 0U) {
        return MYLITE_ERROR;
    }

    for (size_t step_index = 0U; step_index < expression->conversion_step_count; ++step_index) {
        const char *function_name =
            row_scalar_conversion_sql_function_name(&expression->conversion_steps[step_index]);

        if (function_name == NULL) {
            return MYLITE_ERROR;
        }
        rc = mylite_dynamic_string_append(string, function_name);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, '(');
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    rc = append_row_scalar_conversion_base_expression_sql(
        string,
        &expression->arguments[0],
        next_parameter
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    for (size_t step_index = 0U; step_index < expression->conversion_step_count; ++step_index) {
        rc = mylite_dynamic_string_append_char(string, ')');
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int append_row_scalar_conversion_base_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = append_numbered_parameter(string, *next_parameter);
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return mylite_dynamic_string_append_quoted_identifier(string, expression->column.name);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        break;
    }

    return MYLITE_ERROR;
}

static const char *row_scalar_conversion_sql_function_name(
    const struct planned_row_scalar_conversion_step *step
) {
    if (step == NULL) {
        return NULL;
    }

    switch (step->kind) {
    case PLANNED_ROW_SCALAR_CONVERSION_SIGNED:
        return "_mylite_cast_signed";
    case PLANNED_ROW_SCALAR_CONVERSION_UNSIGNED:
        return "_mylite_cast_unsigned";
    case PLANNED_ROW_SCALAR_CONVERSION_USING_CHARSET:
        if (step->ascii_only) {
            return "_mylite_cast_convert_text_ascii";
        }
        return "_mylite_cast_convert_text";
    case PLANNED_ROW_SCALAR_CONVERSION_BINARY:
    case PLANNED_ROW_SCALAR_CONVERSION_CHAR:
    case PLANNED_ROW_SCALAR_CONVERSION_USING_BINARY:
        return "_mylite_cast_convert_text";
    case PLANNED_ROW_SCALAR_CONVERSION_NONE:
        break;
    }
    return NULL;
}

// Row-scalar SQL generation walks expression trees recursively through helper-specific nodes.
// NOLINTNEXTLINE(misc-no-recursion)
static int append_row_scalar_non_concat_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = append_numbered_parameter(string, *next_parameter);
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return mylite_dynamic_string_append_quoted_identifier(string, expression->column.name);
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
        return append_row_scalar_conversion_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
        return append_row_scalar_uuid_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_period_add",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_period_diff",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_convert_tz",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_weight_string",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_weight_string_binary",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
        break;
    }

    return MYLITE_ERROR;
}

static int append_row_scalar_integer_arithmetic_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    struct row_scalar_integer_arithmetic_append_stack stack = {0};
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = row_scalar_integer_arithmetic_append_stack_push(
        &stack,
        (struct row_scalar_integer_arithmetic_append_frame){
            .expression = expression,
            .phase = ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_ENTER,
        }
    );

    while (rc == MYLITE_OK && stack.count != 0U) {
        struct row_scalar_integer_arithmetic_append_frame frame = stack.items[--stack.count];

        rc = append_row_scalar_integer_arithmetic_frame_sql(string, &stack, &frame, next_parameter);
    }
    row_scalar_integer_arithmetic_append_stack_deinit(&stack);
    return rc;
}

static int append_row_scalar_integer_arithmetic_frame_sql(
    struct mylite_dynamic_string *string,
    struct row_scalar_integer_arithmetic_append_stack *stack,
    const struct row_scalar_integer_arithmetic_append_frame *frame,
    size_t *next_parameter
) {
    if (frame == NULL) {
        return MYLITE_ERROR;
    }

    switch (frame->phase) {
    case ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_COMMA:
        return mylite_dynamic_string_append(string, ", ");
    case ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_CLOSE:
        return mylite_dynamic_string_append_char(string, ')');
    case ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_ENTER:
        return append_row_scalar_integer_arithmetic_enter_sql(
            string,
            stack,
            frame->expression,
            next_parameter
        );
    }
    return MYLITE_ERROR;
}

static int append_row_scalar_integer_arithmetic_enter_sql(
    struct mylite_dynamic_string *string,
    struct row_scalar_integer_arithmetic_append_stack *stack,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = append_numbered_parameter(string, *next_parameter);
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return mylite_dynamic_string_append_quoted_identifier(string, expression->column.name);
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
        return append_row_scalar_integer_arithmetic_function_call_sql(string, stack, expression);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        return MYLITE_ERROR;
    }
    return MYLITE_ERROR;
}

static int append_row_scalar_integer_arithmetic_function_call_sql(
    struct mylite_dynamic_string *string,
    struct row_scalar_integer_arithmetic_append_stack *stack,
    const struct planned_row_scalar_expression *expression
) {
    const char *function_name = row_scalar_integer_arithmetic_function_name(expression);
    int rc = MYLITE_OK;

    if (function_name == NULL || expression->arguments == NULL ||
        expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = row_scalar_integer_arithmetic_append_stack_push(
        stack,
        (struct row_scalar_integer_arithmetic_append_frame){
            .expression = NULL,
            .phase = ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_CLOSE,
        }
    );
    if (rc == MYLITE_OK) {
        rc = row_scalar_integer_arithmetic_append_stack_push(
            stack,
            (struct row_scalar_integer_arithmetic_append_frame){
                .expression = &expression->arguments[1],
                .phase = ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_ENTER,
            }
        );
    }
    if (rc == MYLITE_OK) {
        rc = row_scalar_integer_arithmetic_append_stack_push(
            stack,
            (struct row_scalar_integer_arithmetic_append_frame){
                .expression = NULL,
                .phase = ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_COMMA,
            }
        );
    }
    if (rc == MYLITE_OK) {
        rc = row_scalar_integer_arithmetic_append_stack_push(
            stack,
            (struct row_scalar_integer_arithmetic_append_frame){
                .expression = &expression->arguments[0],
                .phase = ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_ENTER,
            }
        );
    }
    return rc;
}

static const char *row_scalar_integer_arithmetic_function_name(
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL) {
        return NULL;
    }
    switch (expression->arithmetic_operator) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
        return "_mylite_i64_add";
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        return "_mylite_i64_sub";
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return "_mylite_i64_mul";
    default:
        return NULL;
    }
}

static void row_scalar_integer_arithmetic_append_stack_deinit(
    struct row_scalar_integer_arithmetic_append_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct row_scalar_integer_arithmetic_append_stack){0};
}

static int row_scalar_integer_arithmetic_append_stack_push(
    struct row_scalar_integer_arithmetic_append_stack *stack,
    struct row_scalar_integer_arithmetic_append_frame frame
) {
    struct row_scalar_integer_arithmetic_append_frame *items = NULL;
    size_t capacity = 0U;

    if (stack == NULL) {
        return MYLITE_MISUSE;
    }
    if (stack->count == stack->capacity) {
        capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;
        if (capacity < stack->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            return MYLITE_NOMEM;
        }
        items = realloc(stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }
    stack->items[stack->count] = frame;
    ++stack->count;
    return MYLITE_OK;
}

static int append_row_scalar_concat_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression->argument_count == 0U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append_char(string, '(');
    if (rc == MYLITE_OK && expression->argument_count == 1U) {
        rc = mylite_dynamic_string_append(string, "'' || ");
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, " || ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_row_scalar_concat_ws_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression->argument_count < 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_concat_ws(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_row_scalar_field_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    bool use_string_collation = expression->field_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
    int rc = MYLITE_OK;

    if (expression->argument_count < 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "(CASE WHEN ");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_field_operand_sql(
            string,
            &expression->arguments[0],
            next_parameter,
            false
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL THEN 0");
    }
    for (size_t argument_index = 1U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        char position_text[integer_text_capacity];
        int written = snprintf(position_text, sizeof(position_text), "%zu", argument_index);

        if (written < 0 || (size_t)written >= sizeof(position_text)) {
            return MYLITE_NOMEM;
        }
        rc = mylite_dynamic_string_append(string, " WHEN ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_field_operand_sql(
                string,
                &expression->arguments[0],
                next_parameter,
                use_string_collation
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_field_operand_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter,
                use_string_collation
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " THEN ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, position_text);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " ELSE 0 END)");
    }

    return rc;
}

static int append_row_scalar_greatest_least_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *sqlite_function = NULL;
    bool use_string_collation = false;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count < 2U) {
        return MYLITE_ERROR;
    }
    if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_GREATEST) {
        sqlite_function = "max";
    } else if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_LEAST) {
        sqlite_function = "min";
    } else {
        return MYLITE_ERROR;
    }

    use_string_collation = expression->field_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
    rc = mylite_dynamic_string_append(string, sqlite_function);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t offset = 0U; rc == MYLITE_OK && offset < expression->argument_count; ++offset) {
        size_t argument_index = expression->argument_count - 1U - offset;

        if (offset != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_field_operand_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter,
                use_string_collation
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_interval_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL ||
        expression->argument_count < 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "(CASE WHEN ");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL THEN -1");
    }
    for (size_t argument_index = 1U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = mylite_dynamic_string_append(string, " WHEN ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[0],
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " < ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " THEN ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_interval_result_sql(string, (int64_t)argument_index - 1);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " ELSE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_interval_result_sql(string, (int64_t)expression->argument_count - 1);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " END)");
    }
    return rc;
}

static int append_row_scalar_interval_result_sql(
    struct mylite_dynamic_string *string,
    int64_t value
) {
    char text[integer_text_capacity];
    int written = snprintf(text, sizeof(text), "%" PRId64, value);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        return MYLITE_NOMEM;
    }
    return mylite_dynamic_string_append(string, text);
}

static int append_row_scalar_date_format_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL ||
        (expression->argument_count != 2U && expression->argument_count != 3U)) {
        return MYLITE_ERROR;
    }
    if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE) {
        rc = mylite_dynamic_string_append(string, "_mylite_str_to_date(");
    } else if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT) {
        rc = mylite_dynamic_string_append(string, "_mylite_time_format(");
    } else {
        rc = mylite_dynamic_string_append(string, "_mylite_date_format(");
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_unhex_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_unhex(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_base64_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64) {
        rc = mylite_dynamic_string_append(string, "_mylite_to_base64(");
    } else if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64) {
        rc = mylite_dynamic_string_append(string, "_mylite_from_base64(");
    } else {
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

// Registered scalar functions delegate argument rendering back to the general row-scalar walker.
// NOLINTNEXTLINE(misc-no-recursion)
static int append_row_scalar_registered_function_expression_sql(
    struct mylite_dynamic_string *string,
    const char *function_name,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (function_name == NULL || expression == NULL ||
        (expression->argument_count != 0U && expression->arguments == NULL)) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_uuid_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name =
        expression == NULL ? NULL : row_scalar_uuid_sql_function_name(expression->kind);
    int rc = MYLITE_OK;

    if (function_name == NULL || expression->argument_count > 2U ||
        (expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_UUID &&
         (expression->argument_count == 0U || expression->arguments == NULL))) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_uuid_argument_sql(
                string,
                &expression->arguments[argument_index],
                argument_index,
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_uuid_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *argument,
    size_t argument_index,
    size_t *next_parameter
) {
    if (argument == NULL) {
        return MYLITE_ERROR;
    }

    switch (argument->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_uuid_leaf_argument_sql(string, argument, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
        if (argument_index == 0U) {
            return append_row_scalar_uuid_inner_expression_sql(string, argument, next_parameter);
        }
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
        break;
    }
    return MYLITE_ERROR;
}

static int append_row_scalar_uuid_inner_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name =
        expression == NULL ? NULL : row_scalar_uuid_sql_function_name(expression->kind);
    int rc = MYLITE_OK;

    if (function_name == NULL || expression->argument_count > 2U ||
        (expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_UUID &&
         (expression->argument_count == 0U || expression->arguments == NULL))) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_uuid_leaf_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_uuid_leaf_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *argument,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (argument == NULL) {
        return MYLITE_ERROR;
    }

    switch (argument->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = append_numbered_parameter(string, *next_parameter);
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return mylite_dynamic_string_append_quoted_identifier(string, argument->column.name);
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
        break;
    }
    return MYLITE_ERROR;
}

static const char *row_scalar_uuid_sql_function_name(enum planned_row_scalar_expression_kind kind) {
    switch (kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
        return "_mylite_uuid";
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
        return "_mylite_is_uuid";
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
        return "_mylite_uuid_to_bin";
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
        return "_mylite_bin_to_uuid";
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
        return NULL;
    }
    return NULL;
}

static int append_row_scalar_char_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count == 0U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_char(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_date_format_numeric_equal_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    rc = append_row_scalar_date_format_numeric_operand_sql(
        string,
        &expression->arguments[0],
        next_parameter
    );
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " = ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_date_format_numeric_operand_sql(
            string,
            &expression->arguments[1],
            next_parameter
        );
    }
    return rc;
}

static int append_row_scalar_date_format_numeric_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, "CAST(");

    if (rc == MYLITE_OK) {
        if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT) {
            rc = append_row_scalar_date_format_expression_sql(string, expression, next_parameter);
        } else if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_VALUE) {
            rc = append_row_scalar_non_concat_expression_sql(string, expression, next_parameter);
        } else {
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS REAL)");
    }
    return rc;
}

static int append_row_scalar_date_interval_second_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != date_interval_planned_argument_count ||
        expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, "_mylite_date_interval_second(");
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < date_interval_planned_argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_datediff_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 4U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_datediff(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 4U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_timediff_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 4U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_timediff(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 4U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_timestampdiff_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL ||
        expression->argument_count != planned_row_scalar_timestampdiff_argument_count ||
        expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_timestampdiff(");
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < planned_row_scalar_timestampdiff_argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_field_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter,
    bool use_string_collation
) {
    int rc = append_row_scalar_non_concat_expression_sql(string, expression, next_parameter);

    if (rc == MYLITE_OK && use_string_collation) {
        rc = append_string_key_collation_sql(string);
    }
    return rc;
}

static int append_row_scalar_string_length_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const struct planned_row_scalar_expression *argument = NULL;
    bool argument_is_binary = false;
    bool force_blob = false;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    argument = &expression->arguments[0];
    argument_is_binary = row_scalar_string_length_argument_is_binary(argument);
    if (!argument_is_binary &&
        expression->string_length_kind != PLANNED_STRING_LENGTH_FUNCTION_CHARACTER) {
        force_blob = true;
    }

    if (expression->string_length_kind == PLANNED_STRING_LENGTH_FUNCTION_BIT) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "length(");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_length_operand_sql(
            string,
            argument,
            next_parameter,
            force_blob
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    if (rc == MYLITE_OK && expression->string_length_kind == PLANNED_STRING_LENGTH_FUNCTION_BIT) {
        rc = mylite_dynamic_string_append(string, " * 8)");
    }
    return rc;
}

static int append_row_scalar_string_length_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter,
    bool force_blob
) {
    int rc = MYLITE_OK;

    if (force_blob) {
        rc = mylite_dynamic_string_append(string, "CAST(");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(string, expression, next_parameter);
    }
    if (rc == MYLITE_OK && force_blob) {
        rc = mylite_dynamic_string_append(string, " AS BLOB)");
    }
    return rc;
}

static bool row_scalar_string_length_argument_is_binary(
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        return false;
    }
    return (column_descriptor_is_binary_string_family(&expression->column) ||
            column_descriptor_is_bit(&expression->column)) != 0;
}

static int append_row_scalar_string_codepoint_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    function_name = row_scalar_string_codepoint_sql_function(expression->string_codepoint_kind);
    if (function_name == NULL) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static const char *row_scalar_string_codepoint_sql_function(
    enum planned_string_codepoint_function_kind function_kind
) {
    switch (function_kind) {
    case PLANNED_STRING_CODEPOINT_FUNCTION_ASCII:
        return "_mylite_ascii";
    case PLANNED_STRING_CODEPOINT_FUNCTION_ORD:
        return "_mylite_ord";
    case PLANNED_STRING_CODEPOINT_FUNCTION_NONE:
        break;
    }
    return NULL;
}

static int append_row_scalar_string_case_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    function_name = row_scalar_string_case_sql_function(expression->string_case_kind);
    if (function_name == NULL) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static const char *row_scalar_string_case_sql_function(
    enum planned_string_case_function_kind function_kind
) {
    switch (function_kind) {
    case PLANNED_STRING_CASE_FUNCTION_LOWER:
        return "_mylite_lower_ascii";
    case PLANNED_STRING_CASE_FUNCTION_UPPER:
        return "_mylite_upper_ascii";
    case PLANNED_STRING_CASE_FUNCTION_NONE:
        break;
    }
    return NULL;
}

static int append_row_scalar_string_trim_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 2U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    function_name = row_scalar_string_trim_sql_function(expression->string_trim_kind);
    if (function_name == NULL) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static const char *row_scalar_string_trim_sql_function(
    enum planned_string_trim_function_kind function_kind
) {
    switch (function_kind) {
    case PLANNED_STRING_TRIM_FUNCTION_BOTH:
        return "_mylite_trim";
    case PLANNED_STRING_TRIM_FUNCTION_LEADING:
        return "_mylite_ltrim";
    case PLANNED_STRING_TRIM_FUNCTION_TRAILING:
        return "_mylite_rtrim";
    case PLANNED_STRING_TRIM_FUNCTION_NONE:
        break;
    }
    return NULL;
}

static int append_row_scalar_unix_timestamp_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 2U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_unix_timestamp(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_temporal_extract_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count == 0U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_temporal_extract(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_sec_to_time_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_sec_to_time(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_timestamp_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 4U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, "_mylite_timestamp(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 4U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_from_unixtime_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_from_unixtime(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_temporal_constructor_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
        function_name = "_mylite_from_days";
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
        function_name = "_mylite_makedate";
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
        function_name = "_mylite_maketime";
        break;
    default:
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_string_slice_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    if (expression->string_slice_kind == PLANNED_STRING_SLICE_FUNCTION_SUBSTRING) {
        return append_row_scalar_substring_expression_sql(string, expression, next_parameter);
    }
    return append_row_scalar_left_right_expression_sql(string, expression, next_parameter);
}

static int append_row_scalar_string_padding_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    if (expression->string_padding_kind == PLANNED_STRING_PADDING_FUNCTION_SPACE) {
        if (expression->argument_count != 1U) {
            return MYLITE_ERROR;
        }
    } else if (expression->string_padding_kind == PLANNED_STRING_PADDING_FUNCTION_REPEAT) {
        if (expression->argument_count != 2U) {
            return MYLITE_ERROR;
        }
    } else if (
        expression->string_padding_kind == PLANNED_STRING_PADDING_FUNCTION_LPAD ||
        expression->string_padding_kind == PLANNED_STRING_PADDING_FUNCTION_RPAD
    ) {
        if (expression->argument_count != 3U) {
            return MYLITE_ERROR;
        }
    } else {
        return MYLITE_ERROR;
    }

    function_name = row_scalar_string_padding_sql_function(expression->string_padding_kind);
    if (function_name == NULL) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static const char *row_scalar_string_padding_sql_function(
    enum planned_string_padding_function_kind function_kind
) {
    switch (function_kind) {
    case PLANNED_STRING_PADDING_FUNCTION_LPAD:
        return "_mylite_lpad";
    case PLANNED_STRING_PADDING_FUNCTION_RPAD:
        return "_mylite_rpad";
    case PLANNED_STRING_PADDING_FUNCTION_REPEAT:
        return "_mylite_repeat";
    case PLANNED_STRING_PADDING_FUNCTION_SPACE:
        return "_mylite_space";
    case PLANNED_STRING_PADDING_FUNCTION_NONE:
        break;
    }
    return NULL;
}

static int append_row_scalar_string_bitmask_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    if ((expression->string_bitmask_kind == PLANNED_STRING_BITMASK_FUNCTION_EXPORT_SET &&
         (expression->argument_count < string_bitmask_export_set_min_argument_count ||
          expression->argument_count > string_bitmask_export_set_max_argument_count)) ||
        (expression->string_bitmask_kind == PLANNED_STRING_BITMASK_FUNCTION_MAKE_SET &&
         expression->argument_count < string_bitmask_make_set_min_argument_count)) {
        return MYLITE_ERROR;
    }

    function_name = row_scalar_string_bitmask_sql_function(expression->string_bitmask_kind);
    if (function_name == NULL) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static const char *row_scalar_string_bitmask_sql_function(
    enum planned_string_bitmask_function_kind function_kind
) {
    switch (function_kind) {
    case PLANNED_STRING_BITMASK_FUNCTION_EXPORT_SET:
        return "_mylite_export_set";
    case PLANNED_STRING_BITMASK_FUNCTION_MAKE_SET:
        return "_mylite_make_set";
    case PLANNED_STRING_BITMASK_FUNCTION_NONE:
        break;
    }
    return NULL;
}

static int append_row_scalar_string_search_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U ||
        expression->string_search_kind == PLANNED_STRING_SEARCH_FUNCTION_NONE) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_locate_ascii_ci(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 3U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_string_replace_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_replace(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 3U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_string_insert_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 4U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_insert_string(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 4U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_string_reverse_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_reverse_utf8(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_soundex_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_soundex(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_string_quote_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_quote_sql(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_substring_index_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_substring_index(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 3U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_find_in_set_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_find_in_set_ascii_ci(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_strcmp_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_strcmp_ascii_ci(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_regexp_like_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }

    if (expression->regexp_case_sensitive) {
        rc = mylite_dynamic_string_append(string, "_mylite_regexp_cs_ascii(");
    } else {
        rc = mylite_dynamic_string_append(string, "_mylite_regexp_ci_ascii(");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[1],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_regexp_string_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        (expression->argument_count != 2U && expression->argument_count != 3U)) {
        return MYLITE_ERROR;
    }

    if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR) {
        rc = mylite_dynamic_string_append(string, "_mylite_regexp_instr_ci_ascii(");
    } else if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR) {
        rc = mylite_dynamic_string_append(string, "_mylite_regexp_substr_ci_ascii(");
    } else if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE) {
        rc = mylite_dynamic_string_append(string, "_mylite_regexp_replace_ci_ascii(");
    } else {
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[1],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK && expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK && expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[2],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_json_valid_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_json_valid(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_json_search_expression_sql(
    struct mylite_dynamic_string *string,
    const char *function_name,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (function_name == NULL || expression == NULL ||
        (expression->argument_count != 0U && expression->arguments == NULL)) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_json_introspection_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_json_extract_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_json_extract(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_json_extract_document_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[1],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_json_value_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_json_value(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_json_extract_document_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[1],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_json_unquote_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_json_unquote(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_json_quote_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_json_quote(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_json_unquote_extract_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_json_unquote(_mylite_json_extract(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_json_extract_document_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[1],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "))");
    }
    return rc;
}

static int append_row_scalar_json_extract_document_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_non_concat_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
        return append_row_scalar_json_set_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        break;
    }

    return MYLITE_ERROR;
}

static int append_row_scalar_json_introspection_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_non_concat_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
        return append_row_scalar_json_set_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
        return append_row_scalar_json_extract_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        break;
    }

    return MYLITE_ERROR;
}

static int append_row_scalar_json_length_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        (expression->argument_count != 1U && expression->argument_count != 2U)) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_json_length(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_json_introspection_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_json_keys_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        (expression->argument_count != 1U && expression->argument_count != 2U)) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_json_keys(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_json_introspection_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_json_type_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_json_type(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_json_introspection_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_json_set_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *sqlite_function_name = NULL;
    int rc = MYLITE_OK;

    rc = json_mutation_sqlite_function_name(expression, &sqlite_function_name);
    if (rc != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, sqlite_function_name);
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_json_set_document_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        if (expression->json_mutation_kind == PLANNED_JSON_MUTATION_REMOVE) {
            rc = append_row_scalar_json_remove_arguments_sql(string, expression, next_parameter);
        } else {
            rc = append_row_scalar_json_set_pair_arguments_sql(string, expression, next_parameter);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int json_mutation_sqlite_function_name(
    const struct planned_row_scalar_expression *expression,
    const char **out_function_name
) {
    if (expression == NULL || expression->arguments == NULL || out_function_name == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->json_mutation_kind) {
    case PLANNED_JSON_MUTATION_SET:
        if (expression->argument_count < 3U || (expression->argument_count % 2U) == 0U) {
            return MYLITE_ERROR;
        }
        *out_function_name = "_mylite_json_set(";
        return MYLITE_OK;
    case PLANNED_JSON_MUTATION_INSERT:
        if (expression->argument_count < 3U || (expression->argument_count % 2U) == 0U) {
            return MYLITE_ERROR;
        }
        *out_function_name = "_mylite_json_insert(";
        return MYLITE_OK;
    case PLANNED_JSON_MUTATION_REPLACE:
        if (expression->argument_count < 3U || (expression->argument_count % 2U) == 0U) {
            return MYLITE_ERROR;
        }
        *out_function_name = "_mylite_json_replace(";
        return MYLITE_OK;
    case PLANNED_JSON_MUTATION_REMOVE:
        if (expression->argument_count < 2U) {
            return MYLITE_ERROR;
        }
        *out_function_name = "_mylite_json_remove(";
        return MYLITE_OK;
    }
    return MYLITE_ERROR;
}

static int append_row_scalar_json_remove_arguments_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    for (size_t argument_index = 1U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = mylite_dynamic_string_append(string, ", ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    return rc;
}

static int append_row_scalar_json_set_pair_arguments_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    for (size_t argument_index = 1U; rc == MYLITE_OK && argument_index < expression->argument_count;
         argument_index += 2U) {
        rc = mylite_dynamic_string_append(string, ", ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_json_set_value_argument_sql(
                string,
                &expression->arguments[argument_index + 1U],
                next_parameter
            );
        }
    }
    return rc;
}

static int append_row_scalar_json_set_document_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_non_concat_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        break;
    }

    return MYLITE_ERROR;
}

static int append_row_scalar_json_set_value_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *argument,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (argument == NULL) {
        return MYLITE_ERROR;
    }
    switch (argument->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
        rc = append_row_scalar_json_value_kind_sql(string, MYLITE_JSON_SQL_VALUE_JSON);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_json_constructor_expression_sql(
                string,
                "_mylite_json_array",
                argument,
                next_parameter
            );
        }
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
        rc = append_row_scalar_json_value_kind_sql(string, MYLITE_JSON_SQL_VALUE_JSON);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_json_constructor_expression_sql(
                string,
                "_mylite_json_object",
                argument,
                next_parameter
            );
        }
        break;
    default:
        rc = append_row_scalar_json_constructor_argument_sql(string, argument, next_parameter);
        break;
    }
    return rc;
}

static int append_row_scalar_json_value_kind_sql(
    struct mylite_dynamic_string *string,
    enum mylite_json_sql_value_kind value_kind
) {
    char tag[integer_text_capacity];
    int written = snprintf(tag, sizeof(tag), "%d", (int)value_kind);

    if (written < 0 || (size_t)written >= sizeof(tag)) {
        return MYLITE_ERROR;
    }

    return mylite_dynamic_string_append(string, tag);
}

static int append_row_scalar_json_constructor_expression_sql(
    struct mylite_dynamic_string *string,
    const char *function_name,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (function_name == NULL || expression == NULL ||
        (expression->argument_count != 0U && expression->arguments == NULL)) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_json_constructor_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_json_constructor_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *argument,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (argument == NULL) {
        return MYLITE_ERROR;
    }

    rc = append_row_scalar_json_value_kind_sql(string, argument->json_value_kind);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(string, argument, next_parameter);
    }
    return rc;
}

static int append_row_scalar_control_flow_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
        return append_row_scalar_if_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
        return append_row_scalar_ifnull_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
        return append_row_scalar_coalesce_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
        return append_row_scalar_nullif_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        return append_row_scalar_isnull_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    }
    return MYLITE_ERROR;
}

static int append_row_scalar_nested_control_flow_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
        return append_row_scalar_nested_if_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
        return append_row_scalar_nested_ifnull_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
        return append_row_scalar_nested_coalesce_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
        return append_row_scalar_nested_nullif_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        return append_row_scalar_nested_isnull_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    }
    return MYLITE_ERROR;
}

static int append_row_scalar_if_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "(CASE WHEN COALESCE(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", 0) <> 0 THEN ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_argument_sql(
            string,
            &expression->arguments[1],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " ELSE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_argument_sql(
            string,
            &expression->arguments[2],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " END)");
    }
    return rc;
}

static int append_row_scalar_ifnull_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, "ifnull(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_control_flow_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_coalesce_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count == 0U) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, "coalesce(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_control_flow_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_nullif_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    bool use_string_collation =
        (expression != NULL &&
         expression->field_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING) != 0;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, "nullif(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK && use_string_collation) {
            rc = append_row_scalar_control_flow_collated_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        } else if (rc == MYLITE_OK) {
            rc = append_row_scalar_control_flow_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_isnull_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append_char(string, '(');
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL)");
    }
    return rc;
}

static int append_row_scalar_nested_if_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "(CASE WHEN COALESCE(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_leaf_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", 0) <> 0 THEN ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_leaf_argument_sql(
            string,
            &expression->arguments[1],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " ELSE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_leaf_argument_sql(
            string,
            &expression->arguments[2],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " END)");
    }
    return rc;
}

static int append_row_scalar_nested_ifnull_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, "ifnull(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_control_flow_leaf_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_nested_coalesce_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count == 0U) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, "coalesce(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_control_flow_leaf_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_nested_nullif_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    bool use_string_collation =
        (expression != NULL &&
         expression->field_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING) != 0;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, "nullif(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK && use_string_collation) {
            rc = append_row_scalar_control_flow_leaf_collated_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        } else if (rc == MYLITE_OK) {
            rc = append_row_scalar_control_flow_leaf_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_nested_isnull_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append_char(string, '(');
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_leaf_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL)");
    }
    return rc;
}

static int append_row_scalar_control_flow_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_non_concat_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        return append_row_scalar_nested_control_flow_expression_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    }
    return MYLITE_ERROR;
}

static int append_row_scalar_control_flow_collated_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = append_row_scalar_control_flow_argument_sql(string, expression, next_parameter);

    if (rc == MYLITE_OK) {
        rc = append_string_key_collation_sql(string);
    }
    return rc;
}

static int append_row_scalar_control_flow_leaf_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_non_concat_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        break;
    }
    return MYLITE_ERROR;
}

static int append_row_scalar_control_flow_leaf_collated_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = append_row_scalar_control_flow_leaf_argument_sql(string, expression, next_parameter);

    if (rc == MYLITE_OK) {
        rc = append_string_key_collation_sql(string);
    }
    return rc;
}

static int append_row_scalar_left_right_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const struct planned_row_scalar_expression *value = NULL;
    const struct planned_row_scalar_expression *length = NULL;
    bool use_right = false;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 2U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    if (expression->string_slice_kind != PLANNED_STRING_SLICE_FUNCTION_LEFT &&
        expression->string_slice_kind != PLANNED_STRING_SLICE_FUNCTION_RIGHT) {
        return MYLITE_ERROR;
    }

    value = &expression->arguments[0];
    length = &expression->arguments[1];
    use_right = expression->string_slice_kind == PLANNED_STRING_SLICE_FUNCTION_RIGHT;

    rc = mylite_dynamic_string_append(string, "(CASE WHEN ");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, value, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL OR ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, length, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL THEN NULL WHEN ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, length, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " <= 0 THEN '' ELSE substr(");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, value, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK && use_right) {
        rc = append_row_scalar_string_slice_operand_sql(string, length, next_parameter, true);
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "1, ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_string_slice_operand_sql(string, length, next_parameter, false);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") END)");
    }
    return rc;
}

static int append_row_scalar_substring_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const struct planned_row_scalar_expression *value = NULL;
    const struct planned_row_scalar_expression *position = NULL;
    const struct planned_row_scalar_expression *length = NULL;
    bool has_length = false;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        (expression->argument_count != 2U && expression->argument_count != 3U)) {
        return MYLITE_ERROR;
    }

    value = &expression->arguments[0];
    position = &expression->arguments[1];
    has_length = expression->argument_count == 3U;
    if (has_length) {
        length = &expression->arguments[2];
    }

    rc = mylite_dynamic_string_append(string, "(CASE WHEN ");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_substring_null_guard_sql(
            string,
            value,
            position,
            length,
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " THEN NULL WHEN ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, position, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " = 0 THEN ''");
    }
    if (rc == MYLITE_OK && has_length) {
        rc = append_row_scalar_substring_nonpositive_length_guard_sql(
            string,
            length,
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_substring_negative_start_guard_sql(
            string,
            value,
            position,
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_substring_call_sql(string, value, position, length, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") END)");
    }
    return rc;
}

static int append_row_scalar_substring_null_guard_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *value,
    const struct planned_row_scalar_expression *position,
    const struct planned_row_scalar_expression *length,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    rc = append_row_scalar_string_slice_operand_sql(string, value, next_parameter, false);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL OR ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, position, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL");
    }
    if (rc == MYLITE_OK && length != NULL) {
        rc = mylite_dynamic_string_append(string, " OR ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_string_slice_operand_sql(string, length, next_parameter, false);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NULL");
        }
    }
    return rc;
}

static int append_row_scalar_substring_nonpositive_length_guard_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *length,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    rc = mylite_dynamic_string_append(string, " WHEN ");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, length, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " <= 0 THEN ''");
    }
    return rc;
}

static int append_row_scalar_substring_negative_start_guard_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *value,
    const struct planned_row_scalar_expression *position,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    rc = mylite_dynamic_string_append(string, " WHEN ");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, position, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " < 0 AND length(");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, value, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") + ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, position, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " < 0 THEN '' ELSE ");
    }
    return rc;
}

static int append_row_scalar_substring_call_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *value,
    const struct planned_row_scalar_expression *position,
    const struct planned_row_scalar_expression *length,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    rc = mylite_dynamic_string_append(string, "substr(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, value, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, position, next_parameter, false);
    }
    if (rc == MYLITE_OK && length != NULL) {
        rc = mylite_dynamic_string_append(string, ", ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_string_slice_operand_sql(string, length, next_parameter, false);
        }
    }
    return rc;
}

static int append_row_scalar_string_slice_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter,
    bool negate
) {
    int rc = MYLITE_OK;

    if (negate) {
        rc = mylite_dynamic_string_append(string, "(-");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(string, expression, next_parameter);
    }
    if (rc == MYLITE_OK && negate) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_hex_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const struct planned_row_scalar_expression *argument = NULL;
    bool argument_is_integer = false;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    argument = &expression->arguments[0];
    if (argument->kind == PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        if (strcmp(argument->column.physical_type, "INTEGER") == 0) {
            argument_is_integer = true;
        }
    }

    if (!argument_is_integer) {
        rc = mylite_dynamic_string_append(string, "(CASE WHEN ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(string, argument, next_parameter);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NULL THEN NULL ELSE hex(");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(string, argument, next_parameter);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, ") END)");
        }
        return rc;
    }

    rc = mylite_dynamic_string_append(string, "(CASE WHEN ");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(string, argument, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL THEN NULL ELSE printf('%llX', ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(string, argument, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") END)");
    }
    return rc;
}

static int build_select_found_rows_sql(const struct planned_select *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT COUNT(*) FROM ");
    if (rc == MYLITE_OK) {
        rc = append_select_from_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
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

static int build_count_sql(const struct planned_count *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    size_t next_parameter = plan->function == PLANNED_COUNT_LITERAL ? 2U : 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT COUNT(");
    if (rc == MYLITE_OK && plan->function == PLANNED_COUNT_STAR) {
        rc = mylite_dynamic_string_append(&string, "*");
    }
    if (rc == MYLITE_OK && plan->function == PLANNED_COUNT_COLUMN) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->count_column.name);
    }
    if (rc == MYLITE_OK && plan->function == PLANNED_COUNT_DISTINCT_COLUMN) {
        rc = mylite_dynamic_string_append(&string, "DISTINCT ");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->count_column.name);
        }
    }
    if (rc == MYLITE_OK && plan->function == PLANNED_COUNT_LITERAL) {
        rc = append_numbered_parameter(&string, 1U);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
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

static int build_column_aggregate_sql(const struct planned_column_aggregate *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_column_aggregate_select_list_sql(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
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

static int append_column_aggregate_select_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_column_aggregate *plan,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (plan->function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
        return append_group_concat_call_sql(
            string,
            &plan->aggregate_column,
            0U,
            &plan->aggregate_order,
            false,
            plan->has_separator,
            next_parameter
        );
    }
    if (plan->function == PLANNED_COLUMN_AGGREGATE_AVG) {
        rc = mylite_dynamic_string_append(string, "SUM(");
        if (rc == MYLITE_OK) {
            rc =
                mylite_dynamic_string_append_quoted_identifier(string, plan->aggregate_column.name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, "), COUNT(");
        }
        if (rc == MYLITE_OK) {
            rc =
                mylite_dynamic_string_append_quoted_identifier(string, plan->aggregate_column.name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ')');
        }
        return rc;
    }

    rc = mylite_dynamic_string_append(string, column_aggregate_sql_function(plan->function));
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->aggregate_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_group_concat_call_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *value_column,
    size_t value_source_index,
    const struct planned_select_order *order,
    bool qualify_column_references,
    bool has_separator,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, "_mylite_group_concat(");

    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(
            string,
            value_column,
            value_source_index,
            qualify_column_references
        );
    }
    if (rc == MYLITE_OK && has_separator) {
        rc = mylite_dynamic_string_append(string, ", ");
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, *next_parameter);
        }
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
    }
    if (rc == MYLITE_OK && order != NULL && order->has_order) {
        rc = mylite_dynamic_string_append(string, " ORDER BY ");
        if (rc == MYLITE_OK) {
            rc = append_descriptor_value_sql_for_source(
                string,
                &order->column,
                order->column_source_index,
                order->qualify_column_reference
            );
        }
        if (rc == MYLITE_OK) {
            if (order->direction == PLANNED_SELECT_ORDER_DESC) {
                rc = mylite_dynamic_string_append(string, " DESC");
            } else {
                rc = mylite_dynamic_string_append(string, " ASC");
            }
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static const char *column_aggregate_sql_function(enum planned_column_aggregate_function function) {
    switch (function) {
    case PLANNED_COLUMN_AGGREGATE_NONE:
        return "";
    case PLANNED_COLUMN_AGGREGATE_MIN:
        return "MIN";
    case PLANNED_COLUMN_AGGREGATE_MAX:
        return "MAX";
    case PLANNED_COLUMN_AGGREGATE_SUM:
        return "SUM";
    case PLANNED_COLUMN_AGGREGATE_AVG:
        return "";
    case PLANNED_COLUMN_AGGREGATE_BIT_AND:
        return "_mylite_bit_and";
    case PLANNED_COLUMN_AGGREGATE_BIT_OR:
        return "_mylite_bit_or";
    case PLANNED_COLUMN_AGGREGATE_BIT_XOR:
        return "_mylite_bit_xor";
    case PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT:
        return "group_concat";
    }

    return "";
}

static int build_grouped_aggregate_sql(
    const struct planned_grouped_aggregate *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_grouped_aggregate_select_list_sql(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_grouped_aggregate_from_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " GROUP BY ");
    }
    for (size_t group_index = 0U; rc == MYLITE_OK && group_index < plan->group_count;
         ++group_index) {
        if (group_index > 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_descriptor_value_sql_for_source(
                &string,
                &plan->groups[group_index].column,
                plan->groups[group_index].column_source_index,
                plan->source_count > 0U
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_grouped_having_sql(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_grouped_order_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_limit_sql(&string, &plan->limit, &next_parameter);
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

static int append_grouped_aggregate_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan
) {
    int rc = MYLITE_OK;

    if (plan->source_count == 0U) {
        return mylite_dynamic_string_append_quoted_identifier(string, plan->table.physical_name);
    }
    if (plan->source_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append_quoted_identifier(
        string,
        plan->sources[0].table.physical_name
    );
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_source_alias(string, 0U);
    }
    if (rc == MYLITE_OK && plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER) {
        rc = mylite_dynamic_string_append(string, " LEFT JOIN ");
    } else if (rc == MYLITE_OK && plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER) {
        rc = mylite_dynamic_string_append(string, " RIGHT JOIN ");
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " JOIN ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            string,
            plan->sources[1].table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_source_alias(string, 1U);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_join_condition_sql(string, &plan->join_condition);
    }

    return rc;
}

static int append_grouped_aggregate_select_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter
) {
    bool qualify = plan->source_count > 0U;
    int rc = MYLITE_OK;

    for (size_t projection_index = 0U; rc == MYLITE_OK && projection_index < plan->projection_count;
         ++projection_index) {
        if (projection_index > 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_descriptor_column_name_sql_for_source(
                string,
                &plan->projections[projection_index].column,
                plan->projections[projection_index].column_source_index,
                qualify
            );
        }
    }

    for (size_t aggregate_index = 0U; rc == MYLITE_OK && aggregate_index < plan->aggregate_count;
         ++aggregate_index) {
        if (aggregate_index > 0U || plan->projection_count > 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_grouped_aggregate_select_item_sql(
                string,
                plan,
                &plan->aggregates[aggregate_index],
                next_parameter
            );
        }
    }

    return rc;
}

static int append_grouped_aggregate_select_item_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    const struct planned_grouped_aggregate_item *item,
    size_t *next_parameter
) {
    bool qualify = plan->source_count > 0U;
    int rc = MYLITE_OK;

    if (item->function == PLANNED_GROUPED_AGGREGATE_AVG) {
        rc = mylite_dynamic_string_append(string, "SUM(");
        if (rc == MYLITE_OK) {
            rc = append_descriptor_column_name_sql_for_source(
                string,
                &item->aggregate_column,
                item->aggregate_column_source_index,
                qualify
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, "), COUNT(");
        }
        if (rc == MYLITE_OK) {
            rc = append_descriptor_column_name_sql_for_source(
                string,
                &item->aggregate_column,
                item->aggregate_column_source_index,
                qualify
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ')');
        }
        return rc;
    }
    if (item->function == PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
        return append_group_concat_call_sql(
            string,
            &item->aggregate_column,
            item->aggregate_column_source_index,
            &item->aggregate_order,
            qualify,
            item->has_separator,
            next_parameter
        );
    }

    return append_grouped_aggregate_expression_sql(string, plan, item);
}

static int append_grouped_aggregate_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    const struct planned_grouped_aggregate_item *item
) {
    bool qualify = plan->source_count > 0U;
    int rc = MYLITE_OK;

    if (item->function == PLANNED_GROUPED_AGGREGATE_COUNT_STAR) {
        return mylite_dynamic_string_append(string, "COUNT(*)");
    }
    if (item->function == PLANNED_GROUPED_AGGREGATE_ANY_VALUE) {
        return append_descriptor_column_name_sql_for_source(
            string,
            &item->aggregate_column,
            item->aggregate_column_source_index,
            qualify
        );
    }
    if (item->function == PLANNED_GROUPED_AGGREGATE_AVG) {
        rc = mylite_dynamic_string_append(string, "AVG(");
    } else {
        rc = mylite_dynamic_string_append(string, grouped_aggregate_sql_function(item->function));
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, '(');
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(
            string,
            &item->aggregate_column,
            item->aggregate_column_source_index,
            qualify
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static const char *grouped_aggregate_sql_function(
    enum planned_grouped_aggregate_function function
) {
    switch (function) {
    case PLANNED_GROUPED_AGGREGATE_COUNT_COLUMN:
        return "COUNT";
    case PLANNED_GROUPED_AGGREGATE_MIN:
        return "MIN";
    case PLANNED_GROUPED_AGGREGATE_MAX:
        return "MAX";
    case PLANNED_GROUPED_AGGREGATE_SUM:
        return "SUM";
    case PLANNED_GROUPED_AGGREGATE_BIT_AND:
        return "_mylite_bit_and";
    case PLANNED_GROUPED_AGGREGATE_BIT_OR:
        return "_mylite_bit_or";
    case PLANNED_GROUPED_AGGREGATE_BIT_XOR:
        return "_mylite_bit_xor";
    case PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT:
        return "group_concat";
    case PLANNED_GROUPED_AGGREGATE_NONE:
    case PLANNED_GROUPED_AGGREGATE_COUNT_STAR:
    case PLANNED_GROUPED_AGGREGATE_AVG:
    case PLANNED_GROUPED_AGGREGATE_ANY_VALUE:
        return "";
    }

    return "";
}

static int append_grouped_having_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (plan->having.kind == PLANNED_GROUPED_HAVING_NONE) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " HAVING ");
    if (rc == MYLITE_OK) {
        rc = append_grouped_having_operand_sql(string, plan);
    }
    if (plan->having.kind == PLANNED_GROUPED_HAVING_COMPARISON) {
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ' ');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(
                string,
                comparison_operator_sql(plan->having.operator_kind)
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ' ');
        }
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, *next_parameter);
        }
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
    } else if (plan->having.kind == PLANNED_GROUPED_HAVING_IS_NULL) {
        if (plan->having.operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL) {
            if (rc == MYLITE_OK) {
                rc = mylite_dynamic_string_append(string, " IS NOT NULL");
            }
        } else if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NULL");
        }
    }

    return rc;
}

static int append_grouped_having_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan
) {
    if (plan->having.operand == PLANNED_GROUPED_HAVING_OPERAND_GROUP_COLUMN) {
        if (plan->having.group_index >= plan->group_count) {
            return MYLITE_ERROR;
        }
        return append_descriptor_value_sql_for_source(
            string,
            &plan->groups[plan->having.group_index].column,
            plan->groups[plan->having.group_index].column_source_index,
            plan->source_count > 0U
        );
    }
    if (plan->having.operand == PLANNED_GROUPED_HAVING_OPERAND_AGGREGATE) {
        return append_grouped_having_aggregate_sql(string, plan);
    }

    return MYLITE_OK;
}

static int append_grouped_having_aggregate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan
) {
    if (plan->having.aggregate_index >= plan->aggregate_count) {
        return MYLITE_ERROR;
    }
    return append_grouped_aggregate_expression_sql(
        string,
        plan,
        &plan->aggregates[plan->having.aggregate_index]
    );
}

static int append_grouped_order_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan
) {
    int rc = MYLITE_OK;

    if (!plan->order.has_order) {
        return MYLITE_OK;
    }
    if (!plan->order_uses_aggregate) {
        return append_select_order_sql(string, &plan->order, NULL);
    }
    if (plan->order_aggregate_index >= plan->aggregate_count) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, " ORDER BY ");
    if (rc == MYLITE_OK) {
        rc = append_grouped_aggregate_expression_sql(
            string,
            plan,
            &plan->aggregates[plan->order_aggregate_index]
        );
    }
    if (rc == MYLITE_OK) {
        if (plan->order.direction == PLANNED_SELECT_ORDER_DESC) {
            rc = mylite_dynamic_string_append(string, " DESC");
        } else {
            rc = mylite_dynamic_string_append(string, " ASC");
        }
    }

    return rc;
}

static int append_select_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (!planned_select_predicate_has_expression(predicate)) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " WHERE ");
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_expression_sql(string, predicate, next_parameter);
    }

    return rc;
}

static int append_select_predicate_sql_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (!planned_select_predicate_has_expression(predicate)) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " WHERE ");
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_expression_sql_without_exists(
            string,
            predicate,
            next_parameter
        );
    }

    return rc;
}

static int append_select_predicate_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
) {
    struct predicate_sql_work_item *items = NULL;
    size_t item_count = 0U;
    int rc = append_predicate_sql_work_node(&items, &item_count, predicate->root_index);

    while (rc == MYLITE_OK && item_count > 0U) {
        struct predicate_sql_work_item item = items[--item_count];
        rc = append_select_predicate_expression_work_item(
            string,
            predicate,
            item,
            &items,
            &item_count,
            next_parameter
        );
    }

    free(items);
    return rc;
}

static int append_select_predicate_expression_sql_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
) {
    struct predicate_sql_work_item *items = NULL;
    size_t item_count = 0U;
    int rc = append_predicate_sql_work_node(&items, &item_count, predicate->root_index);

    while (rc == MYLITE_OK && item_count > 0U) {
        struct predicate_sql_work_item item = items[--item_count];
        rc = append_select_predicate_expression_work_item_without_exists(
            string,
            predicate,
            item,
            &items,
            &item_count,
            next_parameter
        );
    }

    free(items);
    return rc;
}

static int append_select_predicate_expression_work_item(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    struct predicate_sql_work_item item,
    struct predicate_sql_work_item **items,
    size_t *item_count,
    size_t *next_parameter
) {
    switch (item.kind) {
    case PREDICATE_SQL_WORK_CLOSE:
        return mylite_dynamic_string_append_char(string, ')');
    case PREDICATE_SQL_WORK_OPERATOR:
        return append_select_predicate_logical_operator_sql(string, item.operator_kind);
    case PREDICATE_SQL_WORK_TEXT:
        return mylite_dynamic_string_append(string, item.text);
    case PREDICATE_SQL_WORK_NODE:
        return append_select_predicate_expression_node_sql(
            string,
            predicate,
            item.node_index,
            next_parameter,
            items,
            item_count
        );
    }

    return MYLITE_ERROR;
}

static int append_select_predicate_expression_work_item_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    struct predicate_sql_work_item item,
    struct predicate_sql_work_item **items,
    size_t *item_count,
    size_t *next_parameter
) {
    switch (item.kind) {
    case PREDICATE_SQL_WORK_CLOSE:
        return mylite_dynamic_string_append_char(string, ')');
    case PREDICATE_SQL_WORK_OPERATOR:
        return append_select_predicate_logical_operator_sql(string, item.operator_kind);
    case PREDICATE_SQL_WORK_TEXT:
        return mylite_dynamic_string_append(string, item.text);
    case PREDICATE_SQL_WORK_NODE:
        return append_select_predicate_expression_node_sql_without_exists(
            string,
            predicate,
            item.node_index,
            next_parameter,
            items,
            item_count
        );
    }

    return MYLITE_ERROR;
}

static int append_select_predicate_logical_operator_sql(
    struct mylite_dynamic_string *string,
    enum mylite_sql_ast_operator operator_kind
) {
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_OR ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR) {
        return mylite_dynamic_string_append(string, " OR ");
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_AND ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND) {
        return mylite_dynamic_string_append(string, " AND ");
    }
    return MYLITE_ERROR;
}

static int append_select_predicate_expression_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t node_index,
    size_t *next_parameter,
    struct predicate_sql_work_item **items,
    size_t *item_count
) {
    const struct planned_select_predicate_node *node = NULL;

    if (node_index >= predicate->node_count) {
        return MYLITE_ERROR;
    }

    node = &predicate->nodes[node_index];
    if (node->kind == PLANNED_SELECT_PREDICATE_AND || node->kind == PLANNED_SELECT_PREDICATE_OR ||
        node->kind == PLANNED_SELECT_PREDICATE_XOR) {
        return append_select_predicate_logical_node_sql(string, node, items, item_count);
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_NOT) {
        return append_select_predicate_not_node_sql(string, node, items, item_count);
    }

    return append_select_predicate_node_sql(string, predicate, node, next_parameter);
}

static int append_select_predicate_expression_node_sql_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t node_index,
    size_t *next_parameter,
    struct predicate_sql_work_item **items,
    size_t *item_count
) {
    const struct planned_select_predicate_node *node = NULL;

    if (node_index >= predicate->node_count) {
        return MYLITE_ERROR;
    }

    node = &predicate->nodes[node_index];
    if (node->kind == PLANNED_SELECT_PREDICATE_AND || node->kind == PLANNED_SELECT_PREDICATE_OR ||
        node->kind == PLANNED_SELECT_PREDICATE_XOR) {
        return append_select_predicate_logical_node_sql(string, node, items, item_count);
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_NOT) {
        return append_select_predicate_not_node_sql(string, node, items, item_count);
    }

    return append_select_predicate_node_sql_without_exists(string, predicate, node, next_parameter);
}

static int append_select_predicate_logical_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    struct predicate_sql_work_item **items,
    size_t *item_count
) {
    int rc = MYLITE_OK;

    if (node->kind == PLANNED_SELECT_PREDICATE_XOR) {
        rc = mylite_dynamic_string_append(string, "((NOT NOT ");
        if (rc == MYLITE_OK) {
            rc = append_predicate_sql_work_close(items, item_count);
        }
        if (rc == MYLITE_OK) {
            rc = append_predicate_sql_work_close(items, item_count);
        }
        if (rc == MYLITE_OK) {
            rc = append_predicate_sql_work_node(items, item_count, node->right_index);
        }
        if (rc == MYLITE_OK) {
            rc = append_predicate_sql_work_text(items, item_count, ") <> (NOT NOT ");
        }
        if (rc == MYLITE_OK) {
            rc = append_predicate_sql_work_node(items, item_count, node->left_index);
        }
        return rc;
    }

    rc = mylite_dynamic_string_append_char(string, '(');

    if (rc == MYLITE_OK) {
        rc = append_predicate_sql_work_close(items, item_count);
    }
    if (rc == MYLITE_OK) {
        rc = append_predicate_sql_work_node(items, item_count, node->right_index);
    }
    if (rc == MYLITE_OK) {
        rc = append_predicate_sql_work_operator(items, item_count, node->operator_kind);
    }
    if (rc == MYLITE_OK) {
        rc = append_predicate_sql_work_node(items, item_count, node->left_index);
    }

    return rc;
}

static int append_select_predicate_not_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    struct predicate_sql_work_item **items,
    size_t *item_count
) {
    int rc = mylite_dynamic_string_append(string, "(NOT ");

    if (rc == MYLITE_OK) {
        rc = append_predicate_sql_work_close(items, item_count);
    }
    if (rc == MYLITE_OK) {
        rc = append_predicate_sql_work_node(items, item_count, node->left_index);
    }

    return rc;
}

static int append_select_predicate_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append_char(string, '(');

    if (node->kind == PLANNED_SELECT_PREDICATE_EXISTS) {
        if (rc == MYLITE_OK) {
            rc = append_select_exists_predicate_sql(string, node->exists_subquery, next_parameter);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ')');
        }
        return rc;
    }

    if (rc == MYLITE_OK) {
        rc = append_select_predicate_non_exists_node_sql(string, predicate, node, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_select_predicate_node_sql_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append_char(string, '(');

    if (node->kind == PLANNED_SELECT_PREDICATE_EXISTS) {
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_non_subquery_node_sql(string, predicate, node, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_select_predicate_non_exists_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON &&
        comparison_operator_is_regexp(node->operator_kind)) {
        return append_select_regexp_predicate_sql(
            string,
            predicate->qualify_column_references,
            node,
            next_parameter
        );
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH ||
        node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON ||
        node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL) {
        return append_select_row_scalar_predicate_sql(string, node, next_parameter);
    }

    return append_select_predicate_column_term_sql(string, predicate, node, next_parameter);
}

static int append_select_predicate_non_subquery_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON &&
        comparison_operator_is_regexp(node->operator_kind)) {
        return append_select_regexp_predicate_sql(
            string,
            predicate->qualify_column_references,
            node,
            next_parameter
        );
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH ||
        node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON ||
        node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL) {
        return append_select_row_scalar_predicate_sql(string, node, next_parameter);
    }

    return append_select_predicate_column_term_sql_without_subqueries(
        string,
        predicate,
        node,
        next_parameter
    );
}

static int append_select_predicate_column_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc =
        append_select_predicate_subject_sql(string, node, predicate->qualify_column_references);

    if (rc == MYLITE_OK) {
        if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON) {
            rc = append_select_comparison_predicate_term_sql(
                string,
                predicate->qualify_column_references,
                node,
                next_parameter
            );
        } else if (node->kind == PLANNED_SELECT_PREDICATE_IS_NULL) {
            rc = append_select_is_null_predicate_term_sql(string, node);
        } else if (node->kind == PLANNED_SELECT_PREDICATE_IS_BOOLEAN) {
            rc = append_select_is_boolean_predicate_term_sql(
                string,
                predicate->qualify_column_references,
                node
            );
        } else if (node->kind == PLANNED_SELECT_PREDICATE_BETWEEN) {
            rc = append_select_between_predicate_term_sql(string, next_parameter);
        } else if (node->kind == PLANNED_SELECT_PREDICATE_IN) {
            rc = append_select_in_predicate_term_sql(string, node, next_parameter);
        } else {
            rc = MYLITE_ERROR;
        }
    }

    return rc;
}

static int append_select_predicate_column_term_sql_without_subqueries(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc =
        append_select_predicate_subject_sql(string, node, predicate->qualify_column_references);

    if (rc == MYLITE_OK) {
        if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON) {
            rc = append_select_comparison_predicate_term_sql(
                string,
                predicate->qualify_column_references,
                node,
                next_parameter
            );
        } else if (node->kind == PLANNED_SELECT_PREDICATE_IS_NULL) {
            rc = append_select_is_null_predicate_term_sql(string, node);
        } else if (node->kind == PLANNED_SELECT_PREDICATE_IS_BOOLEAN) {
            rc = append_select_is_boolean_predicate_term_sql(
                string,
                predicate->qualify_column_references,
                node
            );
        } else if (node->kind == PLANNED_SELECT_PREDICATE_BETWEEN) {
            rc = append_select_between_predicate_term_sql(string, next_parameter);
        } else if (node->kind == PLANNED_SELECT_PREDICATE_IN) {
            rc = append_select_in_literal_predicate_term_sql(string, node, next_parameter);
        } else {
            rc = MYLITE_ERROR;
        }
    }

    return rc;
}

static int append_select_row_scalar_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc = append_row_scalar_expression_sql(string, node->row_scalar_expression, next_parameter);

    if (rc == MYLITE_OK && node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK && node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON) {
        if (row_scalar_expression_uses_string_collation(node->row_scalar_expression)) {
            rc = append_string_key_collation_sql(string);
        }
    }
    if (rc == MYLITE_OK && node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON) {
        rc = mylite_dynamic_string_append_char(string, ' ');
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, comparison_operator_sql(node->operator_kind));
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ' ');
        }
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, *next_parameter);
        }
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
        return rc;
    }
    if (rc == MYLITE_OK && node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL) {
        if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NULL) {
            return mylite_dynamic_string_append(string, " IS NULL");
        }
        if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL) {
            return mylite_dynamic_string_append(string, " IS NOT NULL");
        }
    }

    return MYLITE_ERROR;
}

static int append_select_exists_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_exists_subquery *subquery,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (subquery == NULL) {
        return MYLITE_ERROR;
    }
    if (subquery->limit.has_limit && subquery->limit.row_count == 0) {
        return mylite_dynamic_string_append(string, "0");
    }

    rc = mylite_dynamic_string_append(string, "EXISTS (SELECT 1");
    if (rc == MYLITE_OK && subquery->has_table_source) {
        rc = mylite_dynamic_string_append(string, " FROM ");
    }
    if (rc == MYLITE_OK && subquery->has_table_source) {
        rc = append_exists_subquery_from_sql(string, subquery);
    }
    if (rc == MYLITE_OK && subquery->has_table_source) {
        rc = append_select_predicate_sql_without_exists(
            string,
            &subquery->predicate,
            next_parameter
        );
    }
    if (rc == MYLITE_OK && subquery->limit.has_limit) {
        rc = append_select_limit_sql(string, &subquery->limit, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_exists_subquery_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_exists_subquery *subquery
) {
    int rc = mylite_dynamic_string_append_quoted_identifier(
        string,
        subquery->source.table.physical_name
    );

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_source_alias(string, 1U);
    }

    return rc;
}

static int append_descriptor_value_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
) {
    if (column_descriptor_is_time(column)) {
        return append_time_seconds_value_sql_for_source(string, column, source_index, qualify);
    }
    if (column_descriptor_is_string_family(column)) {
        int rc =
            append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);

        if (rc == MYLITE_OK) {
            rc = append_string_key_collation_sql(string);
        }
        return rc;
    }

    return append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
}

static int append_select_predicate_subject_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    bool qualify
) {
    if (node->compare_date_as_datetime) {
        return append_date_midnight_value_sql_for_source(
            string,
            &node->column,
            node->column_source_index,
            qualify
        );
    }

    return append_descriptor_value_sql_for_source(
        string,
        &node->column,
        node->column_source_index,
        qualify
    );
}

static int append_date_midnight_value_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
) {
    int rc = mylite_dynamic_string_append_char(string, '(');

    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " || ' 00:00:00')");
    }

    return rc;
}

static int append_descriptor_column_name_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
) {
    int rc = MYLITE_OK;

    if (qualify) {
        rc = append_select_source_alias(string, source_index);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, '.');
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
    }

    return rc;
}

static int append_time_seconds_value_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
) {
    int rc = mylite_dynamic_string_append(string, "((CASE WHEN substr(");

    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(
            string,
            ", 1, 1) = '-' THEN -1 ELSE 1 END) * ((CAST(substr("
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", CASE WHEN substr(");
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(
            string,
            ", 1, 1) = '-' THEN 2 ELSE 1 END, CASE WHEN substr("
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", 1, 1) = '-' THEN length(");
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") - 7 ELSE length(");
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") - 6 END) AS INTEGER) * 3600) + (CAST(substr(");
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", length(");
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") - 4, 2) AS INTEGER) * 60) + CAST(substr(");
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", length(");
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(string, column, source_index, qualify);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") - 1, 2) AS INTEGER)))");
    }

    return rc;
}

static int append_select_comparison_predicate_term_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append_char(string, ' ');

    if (comparison_operator_is_like(node->operator_kind)) {
        if (rc == MYLITE_OK) {
            rc = append_select_like_predicate_term_sql(string, node, next_parameter);
        }
        return rc;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, comparison_operator_sql(node->operator_kind));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ' ');
    }
    if (rc == MYLITE_OK) {
        if (node->value_is_column_reference) {
            rc = append_descriptor_value_sql_for_source(
                string,
                &node->value_column,
                node->value_column_source_index,
                qualify_column
            );
        } else {
            rc = append_numbered_parameter(string, *next_parameter);
        }
    }
    if (rc == MYLITE_OK && !node->value_is_column_reference) {
        ++(*next_parameter);
    }

    return rc;
}

static int append_select_like_predicate_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, "LIKE ");

    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK && node->like_uses_escape) {
        rc = mylite_dynamic_string_append(string, " ESCAPE '\\'");
    }

    return rc;
}

static int append_select_regexp_predicate_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, "_mylite_regexp_ci_ascii(");

    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_descriptor_value_sql_for_source(
            string,
            &node->column,
            node->column_source_index,
            qualify_column
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_select_is_null_predicate_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node
) {
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL) {
        return mylite_dynamic_string_append(string, " IS NOT NULL");
    }

    return mylite_dynamic_string_append(string, " IS NULL");
}

static int append_select_is_boolean_predicate_term_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node
) {
    switch (node->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
        return append_is_boolean_rhs_term_sql(string, qualify_column, node, " IS NOT NULL AND ");
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
        return append_is_boolean_rhs_term_sql(string, qualify_column, node, " IS NULL OR ");
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
        return append_is_boolean_rhs_term_sql(string, qualify_column, node, " IS NOT NULL AND ");
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
        return append_is_boolean_rhs_term_sql(string, qualify_column, node, " IS NULL OR ");
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
        return mylite_dynamic_string_append(string, " IS NULL");
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
        return mylite_dynamic_string_append(string, " IS NOT NULL");
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_LEFT_SHIFT:
    case MYLITE_SQL_AST_OPERATOR_RIGHT_SHIFT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_RLIKE:
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_CONCAT:
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
        break;
    }

    return MYLITE_ERROR;
}

static int append_is_boolean_rhs_term_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    const char *suffix
) {
    int rc = mylite_dynamic_string_append(string, suffix);

    if (rc == MYLITE_OK) {
        rc = append_descriptor_column_name_sql_for_source(
            string,
            &node->column,
            node->column_source_index,
            qualify_column
        );
    }
    if (rc == MYLITE_OK && (node->operator_kind == MYLITE_SQL_AST_OPERATOR_IS_FALSE ||
                            node->operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE)) {
        rc = mylite_dynamic_string_append(string, " = 0");
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " <> 0");
    }

    return rc;
}

static int append_select_between_predicate_term_sql(
    struct mylite_dynamic_string *string,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, " BETWEEN ");

    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AND ");
    }
    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }

    return rc;
}

static int append_select_in_predicate_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (node->in_subquery != NULL) {
        rc = mylite_dynamic_string_append(string, " IN (");
        if (rc == MYLITE_OK) {
            rc = append_select_in_subquery_sql(string, node->in_subquery, next_parameter);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ')');
        }
        return rc;
    }

    return append_select_in_literal_predicate_term_sql(string, node, next_parameter);
}

static int append_select_in_literal_predicate_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (node->in_subquery != NULL) {
        return MYLITE_ERROR;
    }
    if (node->value_count == 0U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, " IN (");
    for (size_t value_index = 0U; rc == MYLITE_OK && value_index < node->value_count;
         ++value_index) {
        if (value_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, *next_parameter);
        }
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_select_in_subquery_sql(
    struct mylite_dynamic_string *string,
    const struct planned_in_subquery *subquery,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (subquery == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_descriptor_value_sql_for_source(string, &subquery->column, 1U, true);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_in_subquery_from_sql(string, subquery);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql_without_exists(
            string,
            &subquery->predicate,
            next_parameter
        );
    }

    return rc;
}

static int append_in_subquery_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_in_subquery *subquery
) {
    int rc = mylite_dynamic_string_append_quoted_identifier(
        string,
        subquery->source.table.physical_name
    );

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_source_alias(string, 1U);
    }

    return rc;
}

static int append_predicate_sql_work_node(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    size_t node_index
) {
    return append_predicate_sql_work_item(
        items,
        item_count,
        (struct predicate_sql_work_item){
            .kind = PREDICATE_SQL_WORK_NODE,
            .node_index = node_index,
        }
    );
}

static int append_predicate_sql_work_operator(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    enum mylite_sql_ast_operator operator_kind
) {
    return append_predicate_sql_work_item(
        items,
        item_count,
        (struct predicate_sql_work_item){
            .kind = PREDICATE_SQL_WORK_OPERATOR,
            .operator_kind = operator_kind,
        }
    );
}

static int append_predicate_sql_work_close(
    struct predicate_sql_work_item **items,
    size_t *item_count
) {
    return append_predicate_sql_work_item(
        items,
        item_count,
        (struct predicate_sql_work_item){.kind = PREDICATE_SQL_WORK_CLOSE}
    );
}

static int append_predicate_sql_work_text(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    const char *text
) {
    return append_predicate_sql_work_item(
        items,
        item_count,
        (struct predicate_sql_work_item){
            .kind = PREDICATE_SQL_WORK_TEXT,
            .text = text,
        }
    );
}

static int append_predicate_sql_work_item(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    struct predicate_sql_work_item item
) {
    struct predicate_sql_work_item *grown_items = NULL;
    size_t required_count = *item_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*grown_items)) {
        return MYLITE_NOMEM;
    }

    grown_items = realloc(*items, required_count * sizeof(*grown_items));
    if (grown_items == NULL) {
        return MYLITE_NOMEM;
    }

    *items = grown_items;
    (*items)[*item_count] = item;
    *item_count = required_count;

    return MYLITE_OK;
}

static int append_select_order_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_order *order,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (!order->has_order) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " ORDER BY ");
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (order->item_count == 0U) {
        return append_select_order_legacy_column_sql(string, order);
    }

    for (size_t item_index = 0U; item_index < order->item_count; ++item_index) {
        rc = append_select_order_item_sql(string, order, item_index, next_parameter);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int append_select_order_legacy_column_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_order *order
) {
    int rc = append_descriptor_value_sql_for_source(
        string,
        &order->column,
        order->column_source_index,
        order->qualify_column_reference
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    return append_select_order_direction_sql(string, order->direction);
}

static int append_select_order_item_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_order *order,
    size_t item_index,
    size_t *next_parameter
) {
    const struct planned_select_order_item *item = &order->items[item_index];
    int rc = MYLITE_OK;

    if (item_index != 0U) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (item->kind == PLANNED_SELECT_ORDER_ITEM_FIELD ||
        item->kind == PLANNED_SELECT_ORDER_ITEM_RAND) {
        if (next_parameter == NULL || item->expression == NULL) {
            return MYLITE_ERROR;
        }
        rc = append_row_scalar_expression_sql(string, item->expression, next_parameter);
    } else {
        rc = append_descriptor_value_sql_for_source(
            string,
            &item->column,
            item->column_source_index,
            order->qualify_column_reference
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    return append_select_order_direction_sql(string, item->direction);
}

static int append_select_order_direction_sql(
    struct mylite_dynamic_string *string,
    enum planned_select_order_direction direction
) {
    if (direction == PLANNED_SELECT_ORDER_DESC) {
        return mylite_dynamic_string_append(string, " DESC");
    }

    return mylite_dynamic_string_append(string, " ASC");
}

static int append_select_limit_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_limit *limit,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (!limit->has_limit) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " LIMIT ");
    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK && limit->has_offset) {
        rc = mylite_dynamic_string_append(string, " OFFSET ");
    }
    if (rc == MYLITE_OK && limit->has_offset) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK && limit->has_offset) {
        ++(*next_parameter);
    }

    return rc;
}

static int build_delete_sql(const struct planned_delete *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "DELETE FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK && plan->is_joined) {
        rc = append_joined_delete_rowid_filter_sql(&string, plan, &next_parameter);
    } else if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = append_delete_rowid_limited_sql(&string, plan, &next_parameter);
    } else if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
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

static int append_delete_rowid_limited_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, " WHERE ");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IN (SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(string, &plan->predicate, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_order_sql(string, &plan->order, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_limit_sql(string, &plan->limit, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_joined_delete_rowid_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, " WHERE ");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IN (");
    }
    if (rc == MYLITE_OK) {
        rc = append_joined_delete_rowid_subquery_sql(string, plan, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_joined_delete_rowid_subquery_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, "SELECT ");

    if (rc == MYLITE_OK) {
        rc = append_joined_delete_target_rowid_sql(string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_joined_delete_from_sql(string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(string, &plan->predicate, next_parameter);
    }

    return rc;
}

static int append_joined_delete_target_rowid_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan
) {
    int rc = append_select_source_alias(string, plan->target_source_index);

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '.');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, plan->rowid_alias);
    }

    return rc;
}

static int append_joined_delete_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan
) {
    int rc = MYLITE_OK;

    if (plan->source_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append_quoted_identifier(
        string,
        plan->sources[0].table.physical_name
    );
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_source_alias(string, 0U);
    }
    if (rc == MYLITE_OK && plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER) {
        rc = mylite_dynamic_string_append(string, " LEFT JOIN ");
    } else if (rc == MYLITE_OK && plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER) {
        rc = mylite_dynamic_string_append(string, " RIGHT JOIN ");
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " JOIN ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            string,
            plan->sources[1].table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_source_alias(string, 1U);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_join_condition_sql(string, &plan->join_condition);
    }

    return rc;
}

static int build_update_sql(const struct planned_update *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    size_t next_parameter = update_assignment_parameter_count(plan) + 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "UPDATE ");
    if (rc == MYLITE_OK) {
        rc = append_single_update_target_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " SET ");
    }
    if (rc == MYLITE_OK) {
        rc = append_update_assignment_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_auto_update_columns_sql(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_row_filter_sql(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK && planned_update_has_row_filter(plan)) {
        rc = mylite_dynamic_string_append(&string, " AND ");
    }
    if (rc == MYLITE_OK) {
        rc = append_update_changed_condition_sql(&string, plan, &next_parameter);
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

static int append_single_update_target_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    int rc = mylite_dynamic_string_append_quoted_identifier(string, plan->table.physical_name);

    if (rc == MYLITE_OK && planned_update_needs_target_alias(plan)) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK && planned_update_needs_target_alias(plan)) {
        rc = append_select_source_alias(string, 0U);
    }

    return rc;
}

static bool planned_update_needs_target_alias(const struct planned_update *plan) {
    if (plan == NULL) {
        return false;
    }
    if (plan->is_joined) {
        return false;
    }
    if (!plan->predicate.qualify_column_references) {
        return false;
    }
    return true;
}

static size_t update_assignment_parameter_count(const struct planned_update *plan) {
    if (planned_update_has_multiple_assignments(plan)) {
        return planned_update_executable_assignment_count(plan);
    }
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL) {
        return update_date_interval_parameter_count;
    }
    return 1U;
}

static int append_update_assignment_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    int rc = MYLITE_OK;

    if (planned_update_has_multiple_assignments(plan)) {
        return append_update_multiple_assignments_sql(string, plan);
    }

    rc = mylite_dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " = ");
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL) {
        return append_update_date_interval_assignment_sql(string, plan);
    }
    if (plan->assignment_value_kind != PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC) {
        return mylite_dynamic_string_append(string, "?1");
    }

    rc = mylite_dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
    if (rc == MYLITE_OK && plan->arithmetic_operator == MYLITE_SQL_AST_OPERATOR_ADD) {
        rc = mylite_dynamic_string_append(string, " + ?1");
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " - ?1");
    }

    return rc;
}

static int append_update_date_interval_assignment_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    return append_update_date_interval_function_sql(string, &plan->assignment_column, 1U);
}

static int append_update_multiple_assignments_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    size_t emitted_count = 0U;
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->assignment_count; ++index) {
        if (planned_update_assignment_is_noop(&plan->assignments[index])) {
            continue;
        }
        if (emitted_count > 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->assignments[index].column.name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, emitted_count + 1U);
        }
        if (rc == MYLITE_OK) {
            ++emitted_count;
        }
    }

    return rc;
}

static int append_update_auto_update_columns_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const struct mylite_catalog_column_descriptor *column = &plan->columns[column_index];

        if (!planned_update_column_has_auto_update(plan, column)) {
            continue;
        }
        rc = mylite_dynamic_string_append(string, ", ");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, *next_parameter);
        }
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
    }

    return rc;
}

static bool planned_update_column_has_auto_update(
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *column
) {
    if (plan == NULL || column == NULL || !column->on_update_current_timestamp) {
        return false;
    }
    if (planned_update_has_multiple_assignments(plan)) {
        for (size_t index = 0U; index < plan->assignment_count; ++index) {
            if (planned_update_assignment_is_noop(&plan->assignments[index])) {
                continue;
            }
            if (plan->assignments[index].column.column_id == column->column_id) {
                return false;
            }
        }
        return true;
    }
    return column->column_id != plan->assignment_column.column_id;
}

static size_t planned_update_auto_update_column_count(const struct planned_update *plan) {
    size_t count = 0U;

    if (plan == NULL) {
        return 0U;
    }
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (planned_update_column_has_auto_update(plan, &plan->columns[column_index])) {
            ++count;
        }
    }
    return count;
}

static int build_update_matched_sql(const struct planned_update *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT 1 FROM ");
    if (rc == MYLITE_OK && plan->is_joined) {
        rc = append_joined_update_from_sql(&string, plan);
    } else if (rc == MYLITE_OK) {
        rc = append_single_update_matched_from_sql(&string, plan);
    }
    if (rc == MYLITE_OK && plan->is_joined) {
        rc = append_joined_update_matched_target_filter_sql(&string, plan, &next_parameter);
    } else if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " LIMIT 1");
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

static int append_single_update_matched_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    return append_single_update_target_sql(string, plan);
}

static int append_joined_update_matched_target_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = append_select_predicate_sql(string, &plan->predicate, next_parameter);

    if (rc == MYLITE_OK && planned_select_predicate_has_expression(&plan->predicate)) {
        rc = mylite_dynamic_string_append(string, " AND ");
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_joined_update_target_rowid_sql(string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NOT NULL");
    }

    return rc;
}

static bool planned_update_has_row_filter(const struct planned_update *plan) {
    if (plan->is_joined || plan->limit.has_limit) {
        return true;
    }
    if (planned_select_predicate_has_expression(&plan->predicate)) {
        return true;
    }
    return false;
}

static int append_update_row_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    if (plan->is_joined) {
        return append_joined_update_rowid_filter_sql(string, plan, next_parameter);
    }
    if (plan->limit.has_limit) {
        return append_update_rowid_limited_sql(string, plan, next_parameter);
    }
    if (planned_select_predicate_has_expression(&plan->predicate)) {
        return append_select_predicate_sql(string, &plan->predicate, next_parameter);
    }
    return mylite_dynamic_string_append(string, " WHERE ");
}

static int append_update_rowid_limited_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, " WHERE ");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IN (SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_single_update_rowid_source_sql(string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(string, &plan->predicate, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_order_sql(string, &plan->order, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_limit_sql(string, &plan->limit, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_single_update_rowid_source_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    return append_single_update_target_sql(string, plan);
}

static int append_joined_update_rowid_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, " WHERE ");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IN (");
    }
    if (rc == MYLITE_OK) {
        rc = append_joined_update_rowid_subquery_sql(string, plan, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_joined_update_rowid_subquery_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, "SELECT ");

    if (rc == MYLITE_OK) {
        rc = append_joined_update_target_rowid_sql(string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_joined_update_from_sql(string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(string, &plan->predicate, next_parameter);
    }

    return rc;
}

static int append_joined_update_target_rowid_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    int rc = append_select_source_alias(string, plan->target_source_index);

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '.');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, plan->rowid_alias);
    }

    return rc;
}

static int append_joined_update_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    int rc = MYLITE_OK;

    if (plan->source_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append_quoted_identifier(
        string,
        plan->sources[0].table.physical_name
    );
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_source_alias(string, 0U);
    }
    if (rc == MYLITE_OK && plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER) {
        rc = mylite_dynamic_string_append(string, " LEFT JOIN ");
    } else if (rc == MYLITE_OK && plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER) {
        rc = mylite_dynamic_string_append(string, " RIGHT JOIN ");
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " JOIN ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            string,
            plan->sources[1].table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_source_alias(string, 1U);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_join_condition_sql(string, &plan->join_condition);
    }

    return rc;
}

static int append_update_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (planned_update_has_multiple_assignments(plan)) {
        return append_update_multiple_changed_condition_sql(string, plan, next_parameter);
    }
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC) {
        return append_update_arithmetic_changed_condition_sql(string, plan, next_parameter);
    }
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL) {
        return append_update_date_interval_changed_condition_sql(string, plan, next_parameter);
    }
    if (plan->assignment_value.is_null) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NOT NULL");
        }
        return rc;
    }

    rc = mylite_dynamic_string_append_char(string, '(');
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL OR ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " <> ");
    }
    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_update_multiple_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    size_t emitted_count = 0U;
    int rc = mylite_dynamic_string_append_char(string, '(');

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->assignment_count; ++index) {
        if (planned_update_assignment_is_noop(&plan->assignments[index])) {
            continue;
        }
        if (emitted_count > 0U) {
            rc = mylite_dynamic_string_append(string, " OR ");
        }
        if (rc == MYLITE_OK) {
            rc = append_update_assignment_changed_condition_sql(
                string,
                &plan->assignments[index].column,
                &plan->assignments[index].value,
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            ++emitted_count;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_update_assignment_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    const struct planned_value *value,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (value->is_null) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NOT NULL");
        }
        return rc;
    }

    rc = mylite_dynamic_string_append_char(string, '(');
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL OR ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " <> ");
    }
    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_update_date_interval_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append_char(string, '(');

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NOT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_update_date_interval_function_sql(
            string,
            &plan->assignment_column,
            *next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        *next_parameter += update_date_interval_parameter_count;
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_update_date_interval_function_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t first_parameter
) {
    int rc = mylite_dynamic_string_append(string, "_mylite_date_interval_update(");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
    }
    for (size_t offset = 0U; rc == MYLITE_OK && offset < update_date_interval_parameter_count;
         ++offset) {
        rc = mylite_dynamic_string_append(string, ", ");
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, first_parameter + offset);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_update_arithmetic_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append_char(string, '(');

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NOT NULL AND ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " <> (");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
    }
    if (rc == MYLITE_OK && plan->arithmetic_operator == MYLITE_SQL_AST_OPERATOR_ADD) {
        rc = mylite_dynamic_string_append(string, " + ");
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " - ");
    }
    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "))");
    }

    return rc;
}

static const char *comparison_operator_sql(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return "=";
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        return "IS";
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return "<>";
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return "<";
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return "<=";
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return ">";
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return ">=";
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_LEFT_SHIFT:
    case MYLITE_SQL_AST_OPERATOR_RIGHT_SHIFT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_RLIKE:
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_CONCAT:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
        break;
    }

    return "=";
}

static int execute_sqlite_schema_sql(struct mylite_db *database, const char *sql) {
    int sqlite_rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    int rc = mylite_sqlite_status_to_mylite(sqlite_rc);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        set_physical_sqlite_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int execute_sqlite_control_sql(const struct mylite_db *database, const char *sql) {
    int sqlite_rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int prepare_sqlite_statement(
    const struct mylite_db *database,
    const char *sql,
    sqlite3_stmt **out_statement
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;

    *out_statement = NULL;
    sqlite_rc = sqlite3_prepare_v2(
        database->sqlite,
        sql,
        sqlite_use_nul_terminated_string,
        &statement,
        NULL
    );
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    *out_statement = statement;

    return MYLITE_OK;
}

static int finalize_sqlite_statement(sqlite3_stmt *statement, int rc) {
    int sqlite_rc = SQLITE_OK;

    if (statement == NULL) {
        return rc;
    }

    sqlite_rc = sqlite3_finalize(statement);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int bind_insert_row(sqlite3_stmt *statement, const struct planned_insert *plan, size_t row) {
    int sqlite_rc = sqlite3_reset(statement);

    if (sqlite_rc == SQLITE_OK) {
        sqlite_rc = sqlite3_clear_bindings(statement);
    }
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    int bind_index = 1;

    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        const struct planned_value *value = &plan->rows[row].values[column_index];
        int rc = MYLITE_OK;

        if (plan->columns[column_index].is_generated) {
            continue;
        }
        rc = bind_planned_value_parameter(statement, bind_index, value);
        if (rc != MYLITE_OK) {
            return rc;
        }
        ++bind_index;
    }

    return MYLITE_OK;
}

static int step_insert_row(sqlite3_stmt *statement, int *out_sqlite_rc) {
    int sqlite_rc = sqlite3_step(statement);

    if (out_sqlite_rc != NULL) {
        *out_sqlite_rc = sqlite_rc;
    }
    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static bool sqlite_status_is_constraint(int sqlite_rc) {
    enum { sqlite_primary_result_code_mask = 0xff };

    return (sqlite_rc == SQLITE_CONSTRAINT ||
            (sqlite_rc & sqlite_primary_result_code_mask) == SQLITE_CONSTRAINT) != 0;
}

static int reset_insert_statement_after_constraint(sqlite3_stmt *statement) {
    int reset_rc = sqlite3_reset(statement);

    if (reset_rc != SQLITE_OK && !sqlite_status_is_constraint(reset_rc)) {
        return mylite_sqlite_status_to_mylite(reset_rc);
    }

    return MYLITE_OK;
}

static int handle_check_constraint_violation(
    struct mylite_db *database,
    int64_t table_id,
    bool ignore_errors,
    bool *out_was_check_violation
) {
    const char *physical_name = NULL;
    struct mylite_catalog_check_constraint_descriptor check_constraint = {0};
    bool found = false;
    int rc = MYLITE_OK;

    *out_was_check_violation = false;
    if (!sqlite_error_message_has_check_constraint(
            sqlite3_errmsg(database->sqlite),
            &physical_name
        )) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_try_read_check_constraint_by_physical_name(
        database,
        table_id,
        physical_name,
        &check_constraint,
        &found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read CHECK constraint descriptor");
        return rc;
    }
    if (!found) {
        return MYLITE_OK;
    }

    *out_was_check_violation = true;
    return set_or_warn_check_constraint_violation(database, &check_constraint, ignore_errors);
}

static int handle_generated_not_null_violation(
    struct mylite_db *database,
    int sqlite_rc,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    bool *out_was_generated_not_null_violation
) {
    const char *column_name = NULL;
    size_t column_name_length = 0U;
    int extended_rc = SQLITE_OK;

    *out_was_generated_not_null_violation = false;
    extended_rc = database == NULL ? sqlite_rc : sqlite3_extended_errcode(database->sqlite);
    if (sqlite_rc != SQLITE_CONSTRAINT_NOTNULL && extended_rc != SQLITE_CONSTRAINT_NOTNULL) {
        return MYLITE_OK;
    }

    if (sqlite_error_message_has_not_null_column(
            sqlite3_errmsg(database->sqlite),
            &column_name,
            &column_name_length
        )) {
        for (size_t column_index = 0U; column_index < column_count; ++column_index) {
            const char *descriptor_name = columns[column_index].name;
            size_t descriptor_name_length = 0U;

            if (descriptor_name == NULL) {
                continue;
            }
            descriptor_name_length = strlen(descriptor_name);
            if (columns[column_index].is_generated && !columns[column_index].is_nullable &&
                descriptor_name_length == column_name_length &&
                strncmp(descriptor_name, column_name, column_name_length) == 0) {
                set_bad_null_error(database, descriptor_name);
                *out_was_generated_not_null_violation = true;
                return MYLITE_ERROR;
            }
        }
        return MYLITE_OK;
    }

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        const char *descriptor_name = columns[column_index].name;

        if (descriptor_name != NULL && columns[column_index].is_generated &&
            !columns[column_index].is_nullable) {
            set_bad_null_error(database, descriptor_name);
            *out_was_generated_not_null_violation = true;
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int handle_generated_range_constraint_violation(
    struct mylite_db *database,
    struct generated_constraint_columns columns,
    size_t row_number,
    bool *out_was_generated_range_violation
) {
    const char *physical_name = NULL;
    size_t column_index = 0U;

    *out_was_generated_range_violation = false;
    if (!sqlite_error_message_has_check_constraint(
            sqlite3_errmsg(database->sqlite),
            &physical_name
        )) {
        return MYLITE_OK;
    }
    if (!generated_range_constraint_column_index(physical_name, &column_index) ||
        columns.columns == NULL || column_index >= columns.column_count ||
        !columns.columns[column_index].is_generated) {
        return MYLITE_OK;
    }

    set_out_of_range_error(database, columns.columns[column_index].name, row_number);
    *out_was_generated_range_violation = true;
    return MYLITE_ERROR;
}

static bool sqlite_error_message_has_check_constraint(
    const char *message,
    const char **out_physical_name
) {
    static const char prefix[] = "CHECK constraint failed: ";
    size_t prefix_length = sizeof(prefix) - 1U;

    if (out_physical_name != NULL) {
        *out_physical_name = NULL;
    }
    if (message == NULL || strncmp(message, prefix, prefix_length) != 0) {
        return false;
    }
    if (out_physical_name != NULL) {
        *out_physical_name = message + prefix_length;
    }
    return true;
}

static bool sqlite_error_message_has_not_null_column(
    const char *message,
    const char **out_column_name,
    size_t *out_column_name_length
) {
    static const char prefix[] = "NOT NULL constraint failed: ";
    const char *column_name = NULL;
    size_t prefix_length = sizeof(prefix) - 1U;
    size_t length = 0U;

    if (out_column_name != NULL) {
        *out_column_name = NULL;
    }
    if (out_column_name_length != NULL) {
        *out_column_name_length = 0U;
    }
    if (message == NULL || strncmp(message, prefix, prefix_length) != 0) {
        return false;
    }

    column_name = message + prefix_length;
    for (const char *cursor = column_name; *cursor != '\0'; ++cursor) {
        if (*cursor == '.') {
            column_name = cursor + 1;
        }
    }
    length = strlen(column_name);
    if (length == 0U) {
        return false;
    }
    if (out_column_name != NULL) {
        *out_column_name = column_name;
    }
    if (out_column_name_length != NULL) {
        *out_column_name_length = length;
    }
    return true;
}

static bool generated_range_constraint_column_index(
    const char *physical_name,
    size_t *out_column_index
) {
    enum { decimal_radix = 10U };

    static const char prefix[] = "_mylite_generated_column_range_";
    size_t prefix_length = sizeof(prefix) - 1U;
    size_t value = 0U;

    if (out_column_index != NULL) {
        *out_column_index = 0U;
    }
    if (physical_name == NULL || strncmp(physical_name, prefix, prefix_length) != 0 ||
        physical_name[prefix_length] == '\0') {
        return false;
    }
    for (size_t index = prefix_length; physical_name[index] != '\0'; ++index) {
        unsigned char byte = (unsigned char)physical_name[index];

        if (byte < '0' || byte > '9') {
            return false;
        }
        if (value > (SIZE_MAX - (size_t)(byte - '0')) / decimal_radix) {
            return false;
        }
        value = (value * decimal_radix) + (size_t)(byte - '0');
    }
    if (out_column_index != NULL) {
        *out_column_index = value;
    }
    return true;
}

static int set_or_warn_check_constraint_violation(
    struct mylite_db *database,
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    bool ignore_errors
) {
    if (!ignore_errors) {
        set_check_constraint_violated_error(database, check_constraint->name);
        return MYLITE_ERROR;
    }

    return append_check_constraint_warning(database, check_constraint->name);
}

static int find_insert_unique_key_conflict(
    struct mylite_db *database,
    int sqlite_step_rc,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info **out_conflicting_index,
    char *value_text,
    size_t value_text_size
) {
    int rc = MYLITE_OK;

    *out_conflicting_index = NULL;
    if (value_text != NULL && value_text_size != 0U) {
        value_text[0] = '\0';
    }

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_count; ++index) {
        const struct loaded_index_info *candidate = &plan->indexes[index];
        bool exists = false;
        bool has_null_part = false;

        if (!candidate->index.is_unique || candidate->part_count == 0U) {
            continue;
        }
        rc = insert_unique_key_row_has_null_part(
            database,
            plan,
            row_index,
            candidate,
            &has_null_part
        );
        if (rc != MYLITE_OK) {
            break;
        }
        if (has_null_part) {
            continue;
        }
        rc = unique_key_tuple_exists(
            database,
            plan->table.physical_name,
            candidate,
            plan->rows[row_index].values,
            plan->column_count,
            &exists
        );
        if (rc == MYLITE_OK && exists) {
            *out_conflicting_index = candidate;
            if (value_text != NULL && value_text_size != 0U) {
                rc = format_key_tuple(
                    candidate,
                    plan->rows[row_index].values,
                    plan->column_count,
                    value_text,
                    value_text_size
                );
            }
            break;
        }
    }

    if (rc == MYLITE_OK && *out_conflicting_index == NULL) {
        return mylite_sqlite_status_to_mylite(sqlite_step_rc);
    }

    return rc;
}

static int insert_unique_key_row_has_null_part(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *index,
    bool *out_has_null_part
) {
    *out_has_null_part = false;

    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        size_t column_index = index->parts[part_index].column_index;

        if (column_index >= plan->column_count) {
            set_runtime_error(database, "invalid unique-index descriptor");
            return MYLITE_ERROR;
        }
        if (plan->rows[row_index].values[column_index].is_null) {
            *out_has_null_part = true;
            return MYLITE_OK;
        }
    }

    return MYLITE_OK;
}

static int apply_insert_duplicate_key_update(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    struct insert_execution_counters *counters
) {
    sqlite3_stmt *statement = NULL;
    struct planned_value *assignment_values = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = convert_insert_duplicate_update_values(
        database,
        plan,
        row_index,
        conflicting_index,
        &assignment_values
    );

    if (rc == MYLITE_OK) {
        rc = validate_insert_duplicate_key_assignment_conflicts(
            database,
            plan,
            row_index,
            conflicting_index,
            assignment_values
        );
    }
    if (rc == MYLITE_OK && count_executable_insert_duplicate_assignments(plan) == 0U) {
        insert_duplicate_update_values_deinit(
            &assignment_values,
            plan->duplicate_update.assignment_count
        );
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = build_insert_duplicate_update_sql(plan, conflicting_index, assignment_values, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_insert_duplicate_update_parameters(
            statement,
            plan,
            row_index,
            conflicting_index,
            assignment_values
        );
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            if (sqlite3_changes64(database->sqlite) > 0) {
                counters->affected_rows += 2;
            }
        } else if (sqlite_status_is_constraint(sqlite_rc)) {
            rc = handle_insert_duplicate_update_constraint(database, sqlite_rc, plan, row_index);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    insert_duplicate_update_values_deinit(
        &assignment_values,
        plan->duplicate_update.assignment_count
    );
    free(sql);

    return rc;
}

static int handle_insert_duplicate_update_constraint(
    struct mylite_db *database,
    int sqlite_rc,
    const struct planned_insert *plan,
    size_t row_index
) {
    bool was_generated_range_violation = false;
    bool was_check_violation = false;
    bool was_generated_not_null_violation = false;
    int rc = handle_generated_range_constraint_violation(
        database,
        (struct generated_constraint_columns){
            .columns = plan->columns,
            .column_count = plan->column_count,
        },
        row_index + 1U,
        &was_generated_range_violation
    );

    if (rc == MYLITE_OK && !was_generated_range_violation) {
        rc = handle_check_constraint_violation(
            database,
            plan->table.table_id,
            false,
            &was_check_violation
        );
    }
    if (rc == MYLITE_OK && !was_check_violation && !was_generated_range_violation) {
        rc = handle_generated_not_null_violation(
            database,
            sqlite_rc,
            plan->columns,
            plan->column_count,
            &was_generated_not_null_violation
        );
    }
    if (rc == MYLITE_OK && !was_check_violation && !was_generated_range_violation &&
        !was_generated_not_null_violation) {
        rc = mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return rc;
}

static int validate_insert_duplicate_key_assignment_conflicts(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    const struct planned_value *assignment_values
) {
    struct planned_value *current_values = NULL;
    struct planned_value *projected_values = NULL;
    int rc = MYLITE_OK;

    if (!insert_duplicate_assignment_targets_key(plan)) {
        return MYLITE_OK;
    }

    rc = fetch_insert_duplicate_current_row_values(
        database,
        plan,
        row_index,
        conflicting_index,
        &current_values
    );
    if (rc == MYLITE_OK) {
        rc = project_insert_duplicate_updated_row_values(
            database,
            plan,
            (struct insert_duplicate_projected_row_inputs){
                .current_values = current_values,
                .assignment_values = assignment_values,
            },
            &projected_values
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_count; ++index) {
        const struct loaded_index_info *candidate = &plan->indexes[index];
        bool has_null_part = false;
        bool exists = false;
        char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY] = "";

        if (!candidate->index.is_unique || candidate->part_count == 0U ||
            !insert_duplicate_index_has_assigned_part(plan, candidate)) {
            continue;
        }
        rc = insert_duplicate_key_tuple_has_null_part(
            plan,
            candidate,
            projected_values,
            &has_null_part
        );
        if (rc != MYLITE_OK || has_null_part) {
            continue;
        }
        rc = unique_key_tuple_exists_excluding_insert_duplicate_row(
            database,
            plan,
            row_index,
            candidate,
            conflicting_index,
            projected_values,
            &exists
        );
        if (rc == MYLITE_OK && exists) {
            rc = format_key_tuple(
                candidate,
                projected_values,
                plan->column_count,
                value_text,
                sizeof(value_text)
            );
            if (rc == MYLITE_OK) {
                set_duplicate_key_error(
                    database,
                    plan->target.table_name,
                    candidate->index.name,
                    value_text
                );
                rc = MYLITE_ERROR;
            }
        }
    }

    insert_duplicate_update_values_deinit(&projected_values, plan->column_count);
    insert_duplicate_update_values_deinit(&current_values, plan->column_count);
    return rc;
}

static int fetch_insert_duplicate_current_row_values(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    struct planned_value **out_values
) {
    sqlite3_stmt *statement = NULL;
    struct planned_value *values = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_values = NULL;
    if (plan->column_count > SIZE_MAX / sizeof(*values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = calloc(plan->column_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = build_insert_duplicate_current_row_select_sql(plan, conflicting_index, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        int parameter_index = 1;

        rc = bind_insert_duplicate_key_tuple_parameters(
            statement,
            &parameter_index,
            conflicting_index,
            plan->rows[row_index].values,
            plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
                 ++column_index) {
                rc = materialize_insert_select_selected_value(
                    database,
                    statement,
                    (int)column_index,
                    &plan->columns[column_index],
                    &plan->columns[column_index],
                    row_index + 1U,
                    false,
                    &values[column_index]
                );
            }
        } else if (sqlite_rc == SQLITE_DONE) {
            set_physical_sqlite_row_error(database);
            rc = MYLITE_ERROR;
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    if (rc != MYLITE_OK) {
        insert_duplicate_update_values_deinit(&values, plan->column_count);
        return rc;
    }

    *out_values = values;
    return MYLITE_OK;
}

static int build_insert_duplicate_current_row_select_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                &string,
                plan->columns[column_index].name
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_key_predicate_sql_from_parameters(
            &string,
            conflicting_index,
            1U
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " LIMIT 1");
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

static int project_insert_duplicate_updated_row_values(
    struct mylite_db *database,
    const struct planned_insert *plan,
    struct insert_duplicate_projected_row_inputs inputs,
    struct planned_value **out_values
) {
    struct planned_value *values = NULL;
    int rc = MYLITE_OK;

    *out_values = NULL;
    if (plan->column_count > SIZE_MAX / sizeof(*values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = calloc(plan->column_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const struct planned_value *source = &inputs.current_values[column_index];

        for (size_t assignment_index = 0U;
             assignment_index < plan->duplicate_update.assignment_count;
             ++assignment_index) {
            if (insert_duplicate_assignment_is_noop(
                    &plan->duplicate_update.assignments[assignment_index]
                )) {
                continue;
            }
            if (plan->duplicate_update.assignments[assignment_index].assignment_column_index ==
                column_index) {
                source = &inputs.assignment_values[assignment_index];
                break;
            }
        }
        rc = copy_planned_value(database, source, &values[column_index]);
    }
    if (rc != MYLITE_OK) {
        insert_duplicate_update_values_deinit(&values, plan->column_count);
        return rc;
    }

    *out_values = values;
    return MYLITE_OK;
}

static bool insert_duplicate_index_has_assigned_part(
    const struct planned_insert *plan,
    const struct loaded_index_info *index
) {
    if (plan == NULL || index == NULL) {
        return false;
    }

    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        for (size_t assignment_index = 0U;
             assignment_index < plan->duplicate_update.assignment_count;
             ++assignment_index) {
            if (insert_duplicate_assignment_is_noop(
                    &plan->duplicate_update.assignments[assignment_index]
                )) {
                continue;
            }
            if (index->parts[part_index].column_index ==
                plan->duplicate_update.assignments[assignment_index].assignment_column_index) {
                return true;
            }
        }
    }

    return false;
}

static int insert_duplicate_key_tuple_has_null_part(
    const struct planned_insert *plan,
    const struct loaded_index_info *index,
    const struct planned_value *values,
    bool *out_has_null_part
) {
    *out_has_null_part = false;

    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        size_t column_index = index->parts[part_index].column_index;

        if (column_index >= plan->column_count) {
            return MYLITE_ERROR;
        }
        if (values[column_index].is_null) {
            *out_has_null_part = true;
            return MYLITE_OK;
        }
    }

    return MYLITE_OK;
}

static int unique_key_tuple_exists_excluding_insert_duplicate_row(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *candidate,
    const struct loaded_index_info *conflicting_index,
    const struct planned_value *projected_values,
    bool *out_exists
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int parameter_index = 1;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_exists = false;
    rc = build_unique_key_lookup_excluding_insert_duplicate_row_sql(
        plan,
        candidate,
        conflicting_index,
        &sql
    );
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_insert_duplicate_key_tuple_parameters(
            statement,
            &parameter_index,
            candidate,
            projected_values,
            plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_insert_duplicate_key_tuple_parameters(
            statement,
            &parameter_index,
            conflicting_index,
            plan->rows[row_index].values,
            plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            *out_exists = true;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int build_unique_key_lookup_excluding_insert_duplicate_row_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *candidate,
    const struct loaded_index_info *conflicting_index,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT 1 FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_key_predicate_sql_from_parameters(&string, candidate, 1U);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AND NOT (");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_key_predicate_sql_from_parameters(
            &string,
            conflicting_index,
            candidate->part_count + 1U
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") LIMIT 1");
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

static int convert_insert_duplicate_update_value(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct planned_value *current_values,
    const struct planned_insert_duplicate_update_assignment *duplicate_assignment,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *value_node = NULL;
    const struct mylite_catalog_column_descriptor *column = NULL;

    if (plan == NULL || duplicate_assignment == NULL || out_value == NULL) {
        set_runtime_error(database, "invalid insert duplicate assignment plan");
        return MYLITE_ERROR;
    }
    if (duplicate_assignment->value_kind ==
        PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_VALUES_REFERENCE) {
        return copy_planned_value(
            database,
            &plan->rows[row_index].values[duplicate_assignment->values_column_index],
            out_value
        );
    }
    if (duplicate_assignment->value_kind ==
        PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_SAME_COLUMN_ARITHMETIC) {
        return convert_insert_duplicate_arithmetic_value(
            database,
            plan,
            row_index,
            current_values,
            duplicate_assignment,
            out_value
        );
    }
    if (insert_duplicate_assignment_is_noop(duplicate_assignment)) {
        planned_value_deinit(out_value);
        *out_value = (struct planned_value){.is_null = true};
        return MYLITE_OK;
    }

    value_node = unwrap_parenthesized_expression(duplicate_assignment->assignment_value_node);
    column = &plan->columns[duplicate_assignment->assignment_column_index];
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        return convert_null_insert_value(
            database,
            column,
            insert_duplicate_update_allows_null_adjustment(database, plan, column),
            out_value
        );
    }

    return convert_insert_value_for_plan(
        database,
        plan,
        value_node,
        column,
        row_index + 1U,
        false,
        dml_allows_string_truncation_adjustment(database, false),
        out_value
    );
}

static int convert_insert_duplicate_arithmetic_value(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct planned_value *current_values,
    const struct planned_insert_duplicate_update_assignment *duplicate_assignment,
    struct planned_value *out_value
) {
    const size_t column_index = duplicate_assignment->assignment_column_index;
    const struct planned_value *current_value = NULL;
    uint64_t delta = 0U;
    int rc = MYLITE_OK;

    if (current_values == NULL || column_index >= plan->column_count) {
        set_runtime_error(database, "invalid insert duplicate arithmetic assignment plan");
        return MYLITE_ERROR;
    }
    current_value = &current_values[column_index];
    planned_value_deinit(out_value);
    if (current_value->is_null) {
        *out_value = (struct planned_value){.is_null = true};
        return MYLITE_OK;
    }
    if (current_value->is_text || current_value->is_blob || current_value->is_real) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }

    rc = parse_insert_duplicate_arithmetic_delta(
        database,
        duplicate_assignment->arithmetic_delta_node,
        &delta
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    struct insert_duplicate_arithmetic_result_request request = {
        .column = &plan->columns[column_index],
        .row_number = row_index + 1U,
        .operator_kind = duplicate_assignment->arithmetic_operator,
        .current_value = current_value->integer,
        .delta = delta,
    };

    return assign_insert_duplicate_arithmetic_result(database, &request, out_value);
}

static int parse_insert_duplicate_arithmetic_delta(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_delta
) {
    const uint64_t int64_positive_max = 9223372036854775807ULL;

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER ||
        parse_unsigned_integer_literal(&literal->span, out_delta) != MYLITE_OK ||
        *out_delta > int64_positive_max) {
        set_insert_duplicate_arithmetic_delta_out_of_range_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int assign_insert_duplicate_arithmetic_result(
    struct mylite_db *database,
    const struct insert_duplicate_arithmetic_result_request *request,
    struct planned_value *out_value
) {
    struct integer_column_range range = {0};
    enum update_arithmetic_range_error_kind error_kind = UPDATE_ARITHMETIC_RANGE_ERROR_COLUMN;
    int rc = MYLITE_OK;
    int64_t result = 0;

    if (request == NULL || request->column == NULL) {
        set_runtime_error(database, "invalid insert duplicate arithmetic assignment request");
        return MYLITE_ERROR;
    }
    rc = integer_range_for_column(
        database,
        request->column,
        "INSERT ... ON DUPLICATE KEY UPDATE arithmetic assignment supports only integer columns",
        &range
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (request->operator_kind == MYLITE_SQL_AST_OPERATOR_SUBTRACT &&
        update_arithmetic_column_is_unsigned(request->column)) {
        error_kind = UPDATE_ARITHMETIC_RANGE_ERROR_UNSIGNED_BIGINT;
    } else if (update_arithmetic_column_is_signed_bigint(request->column)) {
        error_kind = UPDATE_ARITHMETIC_RANGE_ERROR_SIGNED_BIGINT;
    }

    if (request->delta > update_arithmetic_domain_width(&range)) {
        set_insert_duplicate_arithmetic_range_error(
            database,
            &(struct insert_duplicate_arithmetic_range_error){
                .column = request->column,
                .row_number = request->row_number,
                .error_kind = error_kind,
            }
        );
        return MYLITE_ERROR;
    }
    if (request->operator_kind == MYLITE_SQL_AST_OPERATOR_ADD) {
        if (request->current_value > update_arithmetic_addition_threshold(&range, request->delta)) {
            set_insert_duplicate_arithmetic_range_error(
                database,
                &(struct insert_duplicate_arithmetic_range_error){
                    .column = request->column,
                    .row_number = request->row_number,
                    .error_kind = error_kind,
                }
            );
            return MYLITE_ERROR;
        }
        result = request->current_value + (int64_t)request->delta;
    } else if (request->operator_kind == MYLITE_SQL_AST_OPERATOR_SUBTRACT) {
        if (request->current_value <
            update_arithmetic_subtraction_threshold(&range, request->delta)) {
            set_insert_duplicate_arithmetic_range_error(
                database,
                &(struct insert_duplicate_arithmetic_range_error){
                    .column = request->column,
                    .row_number = request->row_number,
                    .error_kind = error_kind,
                }
            );
            return MYLITE_ERROR;
        }
        result = request->current_value - (int64_t)request->delta;
    } else {
        set_insert_duplicate_arithmetic_unsupported_error(database);
        return MYLITE_ERROR;
    }

    *out_value = (struct planned_value){.is_null = false, .is_text = false, .integer = result};
    return MYLITE_OK;
}

static void set_insert_duplicate_arithmetic_delta_out_of_range_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "INSERT ... ON DUPLICATE KEY UPDATE arithmetic assignment supports only unsigned integer "
        "deltas in signed 64-bit range"
    );
}

static void set_insert_duplicate_arithmetic_range_error(
    struct mylite_db *database,
    const struct insert_duplicate_arithmetic_range_error *error
) {
    if (error == NULL || error->column == NULL) {
        set_runtime_error(database, "invalid insert duplicate arithmetic range error");
        return;
    }
    if (error->error_kind == UPDATE_ARITHMETIC_RANGE_ERROR_SIGNED_BIGINT) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            mysql_error_bigint_out_of_range,
            "22003",
            "BIGINT value is out of range in INSERT ... ON DUPLICATE KEY UPDATE arithmetic "
            "assignment"
        );
        return;
    }
    if (error->error_kind == UPDATE_ARITHMETIC_RANGE_ERROR_UNSIGNED_BIGINT) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            mysql_error_bigint_out_of_range,
            "22003",
            "BIGINT UNSIGNED value is out of range in INSERT ... ON DUPLICATE KEY UPDATE "
            "arithmetic assignment"
        );
        return;
    }

    set_out_of_range_error(database, error->column->name, error->row_number);
}

static bool insert_duplicate_update_allows_null_adjustment(
    const struct mylite_db *database,
    const struct planned_insert *plan,
    const struct mylite_catalog_column_descriptor *column
) {
    if (database == NULL || plan == NULL || column == NULL) {
        return false;
    }
    if (plan->row_count <= 1U || session_sql_mode_is_strict(database)) {
        return false;
    }
    return strcmp(column->physical_type, "INTEGER") == 0;
}

static int convert_insert_duplicate_update_values(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    struct planned_value **out_values
) {
    struct planned_value *current_values = NULL;
    struct planned_value *values = NULL;
    int rc = MYLITE_OK;

    *out_values = NULL;
    if (plan->duplicate_update.assignment_count == 0U ||
        plan->duplicate_update.assignments == NULL) {
        set_runtime_error(database, "invalid insert duplicate assignment plan");
        return MYLITE_ERROR;
    }
    if (plan->duplicate_update.assignment_count > SIZE_MAX / sizeof(*values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    values = calloc(plan->duplicate_update.assignment_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (insert_duplicate_update_has_same_column_arithmetic(plan)) {
        rc = fetch_insert_duplicate_current_row_values(
            database,
            plan,
            row_index,
            conflicting_index,
            &current_values
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < plan->duplicate_update.assignment_count;
         ++index) {
        rc = convert_insert_duplicate_update_value(
            database,
            plan,
            row_index,
            current_values,
            &plan->duplicate_update.assignments[index],
            &values[index]
        );
    }
    if (rc != MYLITE_OK) {
        insert_duplicate_update_values_deinit(&current_values, plan->column_count);
        insert_duplicate_update_values_deinit(&values, plan->duplicate_update.assignment_count);
        return rc;
    }

    insert_duplicate_update_values_deinit(&current_values, plan->column_count);
    *out_values = values;
    return MYLITE_OK;
}

static bool insert_duplicate_update_has_same_column_arithmetic(const struct planned_insert *plan) {
    if (plan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < plan->duplicate_update.assignment_count; ++index) {
        if (plan->duplicate_update.assignments[index].value_kind ==
            PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_SAME_COLUMN_ARITHMETIC) {
            return true;
        }
    }

    return false;
}

static void insert_duplicate_update_values_deinit(
    struct planned_value **values,
    size_t value_count
) {
    if (values == NULL || *values == NULL) {
        return;
    }

    for (size_t index = 0U; index < value_count; ++index) {
        planned_value_deinit(&(*values)[index]);
    }
    free(*values);
    *values = NULL;
}

static int copy_planned_value(
    struct mylite_db *database,
    const struct planned_value *source,
    struct planned_value *out_value
) {
    planned_value_deinit(out_value);
    if (source->is_text) {
        return copy_text_value(database, source->text, out_value);
    }
    if (source->is_blob) {
        return copy_blob_value(database, source->text, source->text_length, out_value);
    }

    *out_value = *source;
    out_value->text = NULL;
    out_value->text_length = 0U;
    return MYLITE_OK;
}

static int build_insert_duplicate_update_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    const struct planned_value *assignment_values,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t executable_assignment_count = count_executable_insert_duplicate_assignments(plan);
    size_t next_parameter = 0U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    if (conflicting_index == NULL || conflicting_index->part_count == 0U) {
        mylite_dynamic_string_deinit(&string);
        return MYLITE_ERROR;
    }
    if (executable_assignment_count == 0U) {
        mylite_dynamic_string_deinit(&string);
        return MYLITE_ERROR;
    }
    next_parameter = executable_assignment_count + conflicting_index->part_count + 1U;

    rc = mylite_dynamic_string_append(&string, "UPDATE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " SET ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_assignments_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_key_predicate_sql(&string, plan, conflicting_index);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AND ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_changed_conditions_sql(
            &string,
            plan,
            assignment_values,
            &next_parameter
        );
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

static int append_insert_duplicate_key_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index
) {
    return append_insert_duplicate_key_predicate_sql_from_parameters(
        string,
        conflicting_index,
        count_executable_insert_duplicate_assignments(plan) + 1U
    );
}

static int append_insert_duplicate_key_predicate_sql_from_parameters(
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *index,
    size_t first_parameter
) {
    int rc = MYLITE_OK;

    if (index == NULL || index->part_count == 0U) {
        return MYLITE_ERROR;
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(string, &index->parts[part_index], NULL);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_parameter_sql(
                string,
                &index->parts[part_index],
                first_parameter + part_index
            );
        }
    }

    return rc;
}

static int append_insert_duplicate_assignments_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert *plan
) {
    int rc = MYLITE_OK;
    size_t parameter_index = 1U;
    bool has_previous = false;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->duplicate_update.assignment_count;
         ++index) {
        const size_t column_index =
            plan->duplicate_update.assignments[index].assignment_column_index;

        if (insert_duplicate_assignment_is_noop(&plan->duplicate_update.assignments[index])) {
            continue;
        }
        if (has_previous) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->columns[column_index].name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, parameter_index);
        }
        has_previous = true;
        ++parameter_index;
    }

    return rc;
}

static int append_insert_duplicate_changed_conditions_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert *plan,
    const struct planned_value *assignment_values,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append_char(string, '(');
    bool has_previous = false;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->duplicate_update.assignment_count;
         ++index) {
        const size_t column_index =
            plan->duplicate_update.assignments[index].assignment_column_index;

        if (insert_duplicate_assignment_is_noop(&plan->duplicate_update.assignments[index])) {
            continue;
        }
        if (has_previous) {
            rc = mylite_dynamic_string_append(string, " OR ");
        }
        if (rc == MYLITE_OK) {
            rc = append_insert_duplicate_changed_condition_sql(
                string,
                &plan->columns[column_index],
                &assignment_values[index],
                next_parameter
            );
        }
        has_previous = true;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_insert_duplicate_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    const struct planned_value *assignment_value,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (assignment_value->is_null) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NOT NULL");
        }
        return rc;
    }

    rc = mylite_dynamic_string_append_char(string, '(');
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL OR ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " <> ");
    }
    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int bind_insert_duplicate_update_parameters(
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    const struct planned_value *assignment_values
) {
    int parameter_index = 1;
    int rc = bind_insert_duplicate_assignment_parameters(
        statement,
        &parameter_index,
        plan,
        assignment_values
    );

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < conflicting_index->part_count;
         ++part_index) {
        size_t key_column_index = conflicting_index->parts[part_index].column_index;

        if (key_column_index >= plan->column_count) {
            return MYLITE_ERROR;
        }
        rc = bind_planned_value_parameter(
            statement,
            parameter_index,
            &plan->rows[row_index].values[key_column_index]
        );
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK) {
        rc = bind_insert_duplicate_changed_condition_parameters(
            statement,
            &parameter_index,
            plan,
            assignment_values
        );
    }

    return rc;
}

static int bind_insert_duplicate_assignment_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_insert *plan,
    const struct planned_value *assignment_values
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->duplicate_update.assignment_count;
         ++index) {
        if (insert_duplicate_assignment_is_noop(&plan->duplicate_update.assignments[index])) {
            continue;
        }
        rc = bind_planned_value_parameter(statement, *parameter_index, &assignment_values[index]);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int bind_insert_duplicate_key_tuple_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        size_t column_index = index->parts[part_index].column_index;

        if (column_index >= value_count) {
            return MYLITE_ERROR;
        }
        rc = bind_planned_value_parameter(statement, *parameter_index, &values[column_index]);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int bind_insert_duplicate_changed_condition_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_insert *plan,
    const struct planned_value *assignment_values
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->duplicate_update.assignment_count;
         ++index) {
        if (insert_duplicate_assignment_is_noop(&plan->duplicate_update.assignments[index])) {
            continue;
        }
        if (assignment_values[index].is_null) {
            continue;
        }
        rc = bind_planned_value_parameter(statement, *parameter_index, &assignment_values[index]);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int handle_update_unique_key_conflict(
    struct mylite_db *database,
    const struct planned_update *executable_plan,
    const struct planned_update *plan,
    int sqlite_rc
) {
    char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY] = "";
    const struct loaded_index_info *conflicting_index = NULL;
    int rc = MYLITE_OK;

    if (executable_plan->assignment_value.is_null) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_count; ++index) {
        const struct loaded_index_info *candidate = &plan->indexes[index];
        bool exists = false;
        bool found = false;

        if (!candidate->index.is_unique ||
            !loaded_index_contains_column_id(candidate, plan->assignment_column.column_id)) {
            continue;
        }
        if (conflicting_index == NULL) {
            conflicting_index = candidate;
        }
        if (candidate->part_count == 1U && !candidate->parts[0].index_column.has_prefix_length) {
            rc = unique_key_single_value_exists(
                database,
                plan->table.physical_name,
                candidate,
                &executable_plan->assignment_value,
                &exists
            );
            if (rc == MYLITE_OK && exists) {
                conflicting_index = candidate;
                rc = format_key_value(
                    &executable_plan->assignment_value,
                    value_text,
                    sizeof(value_text)
                );
                break;
            }
        } else {
            rc = find_update_unique_key_conflict_tuple(
                database,
                executable_plan,
                plan,
                candidate,
                value_text,
                sizeof(value_text),
                &found
            );
            if (rc == MYLITE_OK && found) {
                conflicting_index = candidate;
                break;
            }
        }
    }

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (conflicting_index == NULL) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    if (value_text[0] == '\0') {
        rc = format_key_value(&executable_plan->assignment_value, value_text, sizeof(value_text));
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    set_duplicate_key_error(
        database,
        plan->target.table_name,
        conflicting_index->index.name,
        value_text
    );
    return MYLITE_ERROR;
}

static int validate_child_foreign_keys_after_write(
    struct mylite_db *database,
    int64_t child_table_id
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t column_count = 0U;
    size_t foreign_key_count = 0U;
    int rc = MYLITE_OK;

    if (child_table_id <= 0) {
        return MYLITE_OK;
    }
    if (!foreign_key_checks_system_variable_value(database, false)) {
        return MYLITE_OK;
    }

    rc = load_table_columns(database, child_table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = load_table_foreign_key_infos(
            database,
            child_table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        rc = validate_foreign_key_references(database, &foreign_keys[index], false);
    }

    loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    free(columns);
    return rc;
}

static int validate_parent_foreign_keys_after_write(
    struct mylite_db *database,
    int64_t parent_table_id
) {
    struct validate_parent_foreign_keys_context context = {
        .database = database,
        .rc = MYLITE_OK,
    };
    int rc = MYLITE_OK;

    if (parent_table_id <= 0) {
        return MYLITE_OK;
    }
    if (!foreign_key_checks_system_variable_value(database, false)) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_for_each_foreign_key_for_parent_table(
        database,
        parent_table_id,
        validate_parent_foreign_key_descriptor,
        &context
    );

    if (rc == MYLITE_OK) {
        rc = context.rc;
    }
    return rc;
}

static int validate_parent_foreign_key_descriptor(
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    void *user_data
) {
    struct validate_parent_foreign_keys_context *context = user_data;
    struct loaded_foreign_key_info loaded = {0};
    int rc = MYLITE_OK;

    if (foreign_key == NULL || context == NULL || context->database == NULL) {
        return MYLITE_MISUSE;
    }

    rc = load_foreign_key_info(context->database, foreign_key, NULL, 0U, &loaded);
    if (rc == MYLITE_OK) {
        rc = validate_foreign_key_references(context->database, &loaded, true);
    }
    loaded_foreign_key_info_deinit(&loaded);
    context->rc = rc;
    return rc;
}

static int validate_foreign_key_references(
    struct mylite_db *database,
    const struct loaded_foreign_key_info *foreign_key,
    bool parent_write
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = build_foreign_key_validation_sql(foreign_key, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            if (parent_write) {
                set_row_is_referenced_error(database);
            } else {
                set_no_referenced_row_error(database);
            }
            rc = MYLITE_ERROR;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (
        rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
    ) {
        set_physical_sqlite_row_error(database);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int build_foreign_key_validation_sql(
    const struct loaded_foreign_key_info *foreign_key,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);
    if (foreign_key == NULL || foreign_key->part_count == 0U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(&string, "SELECT 1 FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            foreign_key->child_table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, "c");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_foreign_key_child_not_null_checks(&string, foreign_key);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AND NOT EXISTS (SELECT 1 FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            foreign_key->parent_table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, "p");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_foreign_key_parent_match_checks(&string, foreign_key);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") LIMIT 1");
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

static int append_foreign_key_child_not_null_checks(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_foreign_key_column_reference(
                string,
                "c",
                foreign_key->parts[part_index].child_column.name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NOT NULL");
        }
    }

    return rc;
}

static int append_foreign_key_child_set_null_assignments_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                foreign_key->parts[part_index].child_column.name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = NULL");
        }
    }

    return rc;
}

static int append_foreign_key_parent_match_checks(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        const struct loaded_foreign_key_part *part = &foreign_key->parts[part_index];

        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_foreign_key_column_reference(string, "p", part->parent_column.name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_foreign_key_column_reference(string, "c", part->child_column.name);
        }
    }

    return rc;
}

static int append_foreign_key_column_reference(
    struct mylite_dynamic_string *string,
    const char *table_alias,
    const char *column_name
) {
    int rc = mylite_dynamic_string_append_quoted_identifier(string, table_alias);

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '.');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column_name);
    }

    return rc;
}

static int unique_key_tuple_exists(
    struct mylite_db *database,
    const char *physical_table_name,
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count,
    bool *out_exists
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_exists = false;
    rc = build_unique_key_lookup_sql(physical_table_name, index, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_unique_key_tuple_parameters(statement, index, values, value_count);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            *out_exists = true;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int build_unique_key_lookup_sql(
    const char *physical_table_name,
    const struct loaded_index_info *index,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT 1 FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, physical_table_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(&string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(&string, &index->parts[part_index], NULL);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_parameter_sql(
                &string,
                &index->parts[part_index],
                part_index + 1U
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " LIMIT 1");
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

static bool loaded_index_contains_column_id(
    const struct loaded_index_info *index,
    int64_t column_id
) {
    if (index == NULL) {
        return false;
    }
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        if (index->parts[part_index].index_column.column_id == column_id) {
            return true;
        }
    }

    return false;
}

static int unique_key_single_value_exists(
    struct mylite_db *database,
    const char *physical_table_name,
    const struct loaded_index_info *index,
    const struct planned_value *value,
    bool *out_exists
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_exists = false;
    rc = build_unique_key_lookup_sql(physical_table_name, index, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_planned_value_parameter(statement, 1, value);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            *out_exists = true;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int find_update_unique_key_conflict_tuple(
    struct mylite_db *database,
    const struct planned_update *executable_plan,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    char *destination,
    size_t destination_size,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_found = false;
    destination[0] = '\0';
    rc = build_update_unique_key_conflict_sql(executable_plan, index, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        const struct update_unique_key_conflict_bind_request request = {
            .executable_plan = executable_plan,
            .plan = plan,
            .index = index,
        };
        rc = bind_update_unique_key_conflict_parameters(statement, &request);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = format_sqlite_key_tuple(
                statement,
                index->parts,
                index->part_count,
                destination,
                destination_size
            );
            if (rc == MYLITE_OK) {
                *out_found = true;
            }
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    statement = NULL;
    sql = NULL;
    if (rc == MYLITE_OK && !*out_found) {
        rc = build_update_unique_key_internal_conflict_sql(executable_plan, index, &sql);
        if (rc == MYLITE_OK) {
            rc = prepare_sqlite_statement(database, sql, &statement);
        }
        if (rc == MYLITE_OK) {
            const struct update_unique_key_conflict_bind_request request = {
                .executable_plan = executable_plan,
                .plan = plan,
                .index = index,
            };
            rc = bind_update_unique_key_internal_conflict_parameters(statement, &request);
        }
        if (rc == MYLITE_OK) {
            sqlite_rc = sqlite3_step(statement);
            if (sqlite_rc == SQLITE_ROW) {
                rc = format_sqlite_key_tuple(
                    statement,
                    index->parts,
                    index->part_count,
                    destination,
                    destination_size
                );
                if (rc == MYLITE_OK) {
                    *out_found = true;
                }
            } else if (sqlite_rc != SQLITE_DONE) {
                rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            }
        }
        rc = finalize_sqlite_statement(statement, rc);
        free(sql);
    }

    return rc;
}

static int build_update_unique_key_conflict_sql(
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    char **out_sql
) {
    static const char conflict_alias[] = "_mylite_conflict";
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_conflict_select_list(&string, plan, index, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_conflict_candidate_source(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_conflict_target_filter(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_conflict_exists_clause(
            &string,
            plan,
            index,
            conflict_alias,
            &next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " LIMIT 1");
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

static int build_update_unique_key_internal_conflict_sql(
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    char **out_sql
) {
    static const char projected_alias[] = "_mylite_projected";
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_internal_conflict_alias_list(&string, index->part_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM (SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_internal_conflict_select_list(
            &string,
            plan,
            index,
            &next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_conflict_candidate_source(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_conflict_target_filter(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, projected_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_internal_conflict_not_null_filter(&string, index->part_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " GROUP BY ");
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_internal_conflict_alias_list(&string, index->part_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " HAVING COUNT(*) > 1 LIMIT 1");
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

static int append_update_unique_key_internal_conflict_alias_list(
    struct mylite_dynamic_string *string,
    size_t part_count
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        char alias[update_unique_internal_key_alias_capacity];
        int written = snprintf(alias, sizeof(alias), "_mylite_key_%zu", part_index);

        if (written < 0 || (size_t)written >= sizeof(alias)) {
            return MYLITE_NOMEM;
        }
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, alias);
        }
    }

    return rc;
}

static int append_update_unique_key_internal_conflict_select_list(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        char alias[update_unique_internal_key_alias_capacity];
        int written = snprintf(alias, sizeof(alias), "_mylite_key_%zu", part_index);

        if (written < 0 || (size_t)written >= sizeof(alias)) {
            return MYLITE_NOMEM;
        }
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_update_unique_key_conflict_expression(
                string,
                plan,
                &index->parts[part_index],
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " AS ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, alias);
        }
    }

    return rc;
}

static int append_update_unique_key_internal_conflict_not_null_filter(
    struct mylite_dynamic_string *string,
    size_t part_count
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        char alias[update_unique_internal_key_alias_capacity];
        int written = snprintf(alias, sizeof(alias), "_mylite_key_%zu", part_index);

        if (written < 0 || (size_t)written >= sizeof(alias)) {
            return MYLITE_NOMEM;
        }
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, alias);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NOT NULL");
        }
    }

    return rc;
}

static int append_update_unique_key_conflict_select_list(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_update_unique_key_conflict_expression(
                string,
                plan,
                &index->parts[part_index],
                next_parameter
            );
        }
    }

    return rc;
}

static int append_update_unique_key_conflict_candidate_source(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    int rc = mylite_dynamic_string_append(string, " FROM ");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->table.physical_name);
    }

    return rc;
}

static int append_update_unique_key_conflict_target_filter(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (plan->is_joined) {
        rc = append_joined_update_rowid_filter_sql(string, plan, next_parameter);
    } else if (plan->limit.has_limit) {
        rc = append_update_rowid_limited_sql(string, plan, next_parameter);
    } else {
        rc = append_select_predicate_sql(string, &plan->predicate, next_parameter);
    }
    if (rc == MYLITE_OK) {
        if (plan->is_joined || plan->limit.has_limit ||
            planned_select_predicate_has_expression(&plan->predicate)) {
            rc = mylite_dynamic_string_append(string, " AND ");
        } else {
            rc = mylite_dynamic_string_append(string, " WHERE ");
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_update_changed_condition_sql(string, plan, next_parameter);
    }

    return rc;
}

static int append_update_unique_key_conflict_exists_clause(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    const char *conflict_alias,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, " AND EXISTS (SELECT 1 FROM ");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, conflict_alias);
    }

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " WHERE ");
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(string, &index->parts[part_index], conflict_alias);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_update_unique_key_conflict_expression(
                string,
                plan,
                &index->parts[part_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_update_unique_key_conflict_expression(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_part *part,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (part->index_column.column_id == plan->assignment_column.column_id) {
        rc = append_loaded_key_part_parameter_sql(string, part, *next_parameter);
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
        return rc;
    }

    rc = append_loaded_key_part_sql(string, part, plan->table.physical_name);

    return rc;
}

static int bind_update_unique_key_conflict_parameters(
    sqlite3_stmt *statement,
    const struct update_unique_key_conflict_bind_request *request
) {
    int parameter_index = 1;
    int rc = bind_update_unique_key_assignment_parameters(statement, request, &parameter_index);

    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(
            statement,
            &request->plan->predicate,
            &parameter_index
        );
    }
    if (rc == MYLITE_OK && request->plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, parameter_index, request->plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK && !request->executable_plan->assignment_value.is_null) {
        rc = bind_planned_value_parameter(
            statement,
            parameter_index,
            &request->executable_plan->assignment_value
        );
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK) {
        rc = bind_update_unique_key_assignment_parameters(statement, request, &parameter_index);
    }

    return rc;
}

static int bind_update_unique_key_internal_conflict_parameters(
    sqlite3_stmt *statement,
    const struct update_unique_key_conflict_bind_request *request
) {
    int parameter_index = 1;
    int rc = bind_update_unique_key_assignment_parameters(statement, request, &parameter_index);

    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(
            statement,
            &request->plan->predicate,
            &parameter_index
        );
    }
    if (rc == MYLITE_OK && request->plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, parameter_index, request->plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK && !request->executable_plan->assignment_value.is_null) {
        rc = bind_planned_value_parameter(
            statement,
            parameter_index,
            &request->executable_plan->assignment_value
        );
    }

    return rc;
}

static int bind_update_unique_key_assignment_parameters(
    sqlite3_stmt *statement,
    const struct update_unique_key_conflict_bind_request *request,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < request->index->part_count;
         ++part_index) {
        if (request->index->parts[part_index].index_column.column_id !=
            request->plan->assignment_column.column_id) {
            continue;
        }
        rc = bind_planned_value_parameter(
            statement,
            *parameter_index,
            &request->executable_plan->assignment_value
        );
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int format_sqlite_key_tuple(
    sqlite3_stmt *statement,
    const struct loaded_index_part *parts,
    size_t part_count,
    char *destination,
    size_t destination_size
) {
    size_t used = 0U;
    int rc = MYLITE_OK;

    if (statement == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }
    destination[0] = '\0';
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

        if (part_index >= (size_t)INT_MAX) {
            return MYLITE_ERROR;
        }
        rc = format_sqlite_key_part(
            statement,
            parts == NULL ? NULL : &parts[part_index],
            (int)part_index,
            value_text,
            sizeof(value_text)
        );
        if (rc == MYLITE_OK) {
            rc = append_formatted_key_part(
                destination,
                destination_size,
                &used,
                part_index,
                value_text
            );
        }
    }

    return rc;
}

static int format_sqlite_key_part(
    sqlite3_stmt *statement,
    const struct loaded_index_part *part,
    int column_index,
    char *destination,
    size_t destination_size
) {
    int written = 0;
    int sqlite_type = SQLITE_NULL;

    if (destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }
    sqlite_type = sqlite3_column_type(statement, column_index);
    switch (sqlite_type) {
    case SQLITE_NULL:
        written = snprintf(destination, destination_size, "NULL");
        break;
    case SQLITE_INTEGER:
        written = snprintf(
            destination,
            destination_size,
            "%" PRId64,
            (int64_t)sqlite3_column_int64(statement, column_index)
        );
        break;
    default: {
        if (loaded_index_part_uses_binary_key_display(part)) {
            const void *bytes = sqlite3_column_blob(statement, column_index);
            int byte_count = sqlite3_column_bytes(statement, column_index);
            size_t display_byte_count = 0U;

            if ((bytes == NULL && byte_count != 0) || byte_count < 0) {
                return MYLITE_ERROR;
            }
            display_byte_count = binary_key_display_byte_count(part, bytes, (size_t)byte_count);
            return format_binary_key_bytes(
                bytes,
                display_byte_count,
                destination,
                destination_size
            );
        }
        const unsigned char *sqlite_text = sqlite3_column_text(statement, column_index);
        size_t text_length = 0U;

        if (sqlite_text == NULL) {
            return MYLITE_ERROR;
        }
        text_length = strlen((const char *)sqlite_text);
        if (text_length >= destination_size) {
            return MYLITE_ERROR;
        }
        memcpy(destination, sqlite_text, text_length);
        destination[text_length] = '\0';
        return MYLITE_OK;
    }
    }
    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int append_formatted_key_part(
    char *destination,
    size_t destination_size,
    size_t *used,
    size_t part_index,
    const char *value_text
) {
    size_t display_capacity = duplicate_key_value_display_length;
    size_t value_length = 0U;
    size_t copy_length = 0U;

    if (destination == NULL || destination_size == 0U || used == NULL || value_text == NULL) {
        return MYLITE_ERROR;
    }
    value_length = strlen(value_text);
    if (display_capacity > destination_size - 1U) {
        display_capacity = destination_size - 1U;
    }
    if (*used > display_capacity) {
        return MYLITE_ERROR;
    }
    if (*used == display_capacity) {
        return MYLITE_OK;
    }

    if (part_index != 0U) {
        destination[*used] = '-';
        ++(*used);
        destination[*used] = '\0';
        if (*used == display_capacity) {
            return MYLITE_OK;
        }
    }
    copy_length = value_length;
    if (copy_length > display_capacity - *used) {
        copy_length = display_capacity - *used;
    }
    memcpy(&destination[*used], value_text, copy_length);
    *used += copy_length;
    destination[*used] = '\0';

    return MYLITE_OK;
}

static int bind_unique_key_tuple_parameters(
    sqlite3_stmt *statement,
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        size_t column_index = index->parts[part_index].column_index;

        if (column_index >= value_count || part_index >= (size_t)INT_MAX) {
            return MYLITE_ERROR;
        }
        rc = bind_planned_value_parameter(statement, (int)part_index + 1, &values[column_index]);
    }

    return rc;
}

static int format_key_tuple(
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count,
    char *destination,
    size_t destination_size
) {
    size_t used = 0U;

    if (index == NULL || values == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }
    destination[0] = '\0';
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        char part_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
        size_t column_index = index->parts[part_index].column_index;

        if (column_index >= value_count) {
            return MYLITE_ERROR;
        }
        if (format_key_part_value(
                &index->parts[part_index],
                &values[column_index],
                part_text,
                sizeof(part_text)
            ) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        if (append_formatted_key_part(
                destination,
                destination_size,
                &used,
                part_index,
                part_text
            ) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int format_key_part_value(
    const struct loaded_index_part *part,
    const struct planned_value *value,
    char *destination,
    size_t destination_size
) {
    if (part == NULL || value == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }
    if (value->is_blob && loaded_index_part_uses_binary_key_display(part)) {
        return format_blob_key_part_value(part, value, true, destination, destination_size);
    }
    if (!part->index_column.has_prefix_length || value->is_null || !value->is_text) {
        if (part->index_column.has_prefix_length && value->is_blob) {
            return format_blob_key_part_value(part, value, false, destination, destination_size);
        }
        return format_key_value(value, destination, destination_size);
    }

    return format_text_prefix_key_part_value(part, value, destination, destination_size);
}

static int format_blob_key_part_value(
    const struct loaded_index_part *part,
    const struct planned_value *value,
    bool trim_fixed_binary_padding,
    char *destination,
    size_t destination_size
) {
    size_t copy_length = 0U;

    if (part == NULL || value == NULL) {
        return MYLITE_ERROR;
    }
    copy_length = value->text_length;
    if (part->index_column.has_prefix_length) {
        size_t prefix_length = 0U;

        if (index_part_prefix_length(part, &prefix_length) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        copy_length = limited_copy_length(value->text_length, prefix_length);
    }
    if (trim_fixed_binary_padding) {
        copy_length = binary_key_display_byte_count(part, value->text, copy_length);
    }

    return format_binary_key_bytes(value->text, copy_length, destination, destination_size);
}

static int format_text_prefix_key_part_value(
    const struct loaded_index_part *part,
    const struct planned_value *value,
    char *destination,
    size_t destination_size
) {
    size_t prefix_length = 0U;
    size_t copy_length = 0U;

    if (index_part_prefix_length(part, &prefix_length) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    copy_length = limited_copy_length(value->text_length, prefix_length);
    if (value->text == NULL || copy_length >= destination_size) {
        return MYLITE_ERROR;
    }
    memcpy(destination, value->text, copy_length);
    destination[copy_length] = '\0';

    return MYLITE_OK;
}

static int index_part_prefix_length(
    const struct loaded_index_part *part,
    size_t *out_prefix_length
) {
    if (part == NULL || out_prefix_length == NULL || part->index_column.prefix_length < 0 ||
        (uint64_t)part->index_column.prefix_length > SIZE_MAX) {
        return MYLITE_ERROR;
    }
    *out_prefix_length = (size_t)part->index_column.prefix_length;

    return MYLITE_OK;
}

static size_t limited_copy_length(size_t value_length, size_t maximum_length) {
    return value_length < maximum_length ? value_length : maximum_length;
}

static int format_key_value(
    const struct planned_value *value,
    char *destination,
    size_t destination_size
) {
    int written = 0;

    if (value == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }
    if (value->is_null) {
        written = snprintf(destination, destination_size, "NULL");
    } else if (value->is_text) {
        if (value->text == NULL || value->text_length >= destination_size) {
            return MYLITE_ERROR;
        }
        memcpy(destination, value->text, value->text_length);
        destination[value->text_length] = '\0';
        return MYLITE_OK;
    } else if (value->is_blob) {
        return format_binary_key_bytes(
            value->text,
            value->text_length,
            destination,
            destination_size
        );
    } else {
        written = snprintf(destination, destination_size, "%" PRId64, value->integer);
    }
    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static bool loaded_index_part_uses_binary_key_display(const struct loaded_index_part *part) {
    if (part == NULL) {
        return false;
    }

    return column_descriptor_is_binary_string_family(&part->column);
}

static size_t binary_key_display_byte_count(
    const struct loaded_index_part *part,
    const void *bytes,
    size_t byte_count
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info = NULL;
    const unsigned char *source = bytes;

    if (part == NULL || bytes == NULL || byte_count == 0U) {
        return byte_count;
    }
    info = binary_string_type_info_for_logical_type(part->column.logical_type, &storage);
    if (info == NULL || !info->fixed_length) {
        return byte_count;
    }
    while (byte_count > 0U && source[byte_count - 1U] == '\0') {
        --byte_count;
    }

    return byte_count;
}

static int format_binary_key_bytes(
    const void *bytes,
    size_t byte_count,
    char *destination,
    size_t destination_size
) {
    enum {
        printable_ascii_min = 0x20,
        printable_ascii_max = 0x7e,
        hex_high_nibble_shift = 4,
        hex_nibble_mask = 0x0f,
    };

    static const char hex_digits[] = "0123456789ABCDEF";
    const unsigned char *source = bytes;
    size_t used = 0U;

    if ((bytes == NULL && byte_count != 0U) || destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }
    destination[0] = '\0';
    for (size_t index = 0U; index < byte_count; ++index) {
        unsigned char byte = source[index];

        if (byte >= printable_ascii_min && byte <= printable_ascii_max) {
            if (used + 1U >= destination_size) {
                break;
            }
            destination[used] = (char)byte;
            ++used;
        } else {
            if (used + 4U >= destination_size) {
                break;
            }
            destination[used] = '\\';
            destination[used + 1U] = 'x';
            destination[used + 2U] = hex_digits[(byte >> hex_high_nibble_shift) & hex_nibble_mask];
            destination[used + 3U] = hex_digits[byte & hex_nibble_mask];
            used += 4U;
        }
        destination[used] = '\0';
    }

    return MYLITE_OK;
}

static int bind_select_parameters(sqlite3_stmt *statement, const struct planned_select *plan) {
    int parameter_index = 1;

    return bind_select_parameters_at(statement, plan, &parameter_index);
}

static int bind_select_parameters_at(
    sqlite3_stmt *statement,
    const struct planned_select *plan,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    rc = bind_select_predicate_parameters(statement, &plan->predicate, parameter_index);
    if (rc == MYLITE_OK) {
        rc = bind_select_order_parameters(statement, &plan->order, parameter_index);
    }
    if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }
    if (rc == MYLITE_OK && plan->limit.has_offset) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.offset);
    }

    return rc;
}

static int bind_row_scalar_select_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_select *plan
) {
    int parameter_index = 1;

    return bind_row_scalar_select_parameters_at(statement, plan, &parameter_index);
}

static int bind_row_scalar_select_parameters_at(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_select *plan,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->item_count; ++item_index) {
        rc = bind_row_scalar_expression_parameters(
            statement,
            &plan->items[item_index].expression,
            parameter_index
        );
    }
    if (rc == MYLITE_OK && plan->has_source) {
        rc = bind_select_predicate_parameters(statement, &plan->predicate, parameter_index);
    }
    if (rc == MYLITE_OK && plan->has_source) {
        rc = bind_select_order_parameters(statement, &plan->order, parameter_index);
    }
    if (rc == MYLITE_OK && plan->has_source && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }
    if (rc == MYLITE_OK && plan->has_source && plan->limit.has_offset) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.offset);
    }
    if (rc == MYLITE_OK && !plan->has_source && plan->tableless_filter.has_filter &&
        plan->tableless_filter.has_scalar_predicate) {
        rc = bind_select_predicate_parameters_without_exists(
            statement,
            &plan->tableless_filter.predicate,
            parameter_index
        );
    }
    if (rc == MYLITE_OK && !plan->has_source && plan->tableless_filter.has_filter &&
        !plan->tableless_filter.has_scalar_predicate) {
        rc = bind_select_exists_predicate_parameters(
            statement,
            &plan->tableless_filter.subquery,
            parameter_index
        );
    }
    if (rc == MYLITE_OK && !plan->has_source && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }
    if (rc == MYLITE_OK && !plan->has_source && plan->limit.has_offset) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.offset);
    }

    return rc;
}

static int bind_select_order_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_order *order,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (order == NULL || !order->has_order) {
        return MYLITE_OK;
    }
    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < order->item_count; ++item_index) {
        const struct planned_select_order_item *item = &order->items[item_index];

        if (item->kind == PLANNED_SELECT_ORDER_ITEM_FIELD ||
            item->kind == PLANNED_SELECT_ORDER_ITEM_RAND) {
            rc =
                bind_row_scalar_expression_parameters(statement, item->expression, parameter_index);
        }
    }

    return rc;
}

static int bind_insert_select_materialize_parameters(
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan
) {
    if (plan->source_kind == PLANNED_INSERT_SELECT_SOURCE_COMPOUND) {
        return bind_insert_select_compound_parameters(statement, plan);
    }

    return bind_select_parameters(statement, &plan->source);
}

static int bind_insert_select_compound_parameters(
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan
) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    for (size_t branch_index = 0U;
         rc == MYLITE_OK && branch_index < plan->compound_source.branch_count;
         ++branch_index) {
        const struct planned_insert_select_compound_branch *branch =
            &plan->compound_source.branches[branch_index];

        if (branch->kind == PLANNED_INSERT_SELECT_COMPOUND_BRANCH_ROW_SCALAR) {
            rc = bind_row_scalar_select_parameters_at(
                statement,
                &branch->row_source,
                &parameter_index
            );
        } else {
            rc = bind_select_parameters_at(statement, &branch->source, &parameter_index);
        }
    }

    return rc;
}

static int bind_row_scalar_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || parameter_index == NULL) {
        return MYLITE_MISUSE;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = bind_planned_value_parameter(statement, *parameter_index, &expression->value);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
        return bind_row_scalar_window_function_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
        return bind_row_scalar_integer_arithmetic_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
        for (size_t argument_index = 0U;
             rc == MYLITE_OK && argument_index < expression->argument_count;
             ++argument_index) {
            rc = bind_row_scalar_non_concat_expression_parameters(
                statement,
                &expression->arguments[argument_index],
                parameter_index
            );
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
        return bind_row_scalar_field_expression_parameters(statement, expression, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
        return bind_row_scalar_reversed_arguments_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
        return bind_row_scalar_interval_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
        return bind_row_scalar_date_format_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
        if (expression->argument_count != 2U) {
            return MYLITE_ERROR;
        }
        rc = bind_row_scalar_date_format_expression_parameters(
            statement,
            &expression->arguments[0],
            parameter_index
        );
        if (rc == MYLITE_OK) {
            rc = bind_row_scalar_non_concat_expression_parameters(
                statement,
                &expression->arguments[1],
                parameter_index
            );
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
        return bind_row_scalar_date_interval_second_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
        return bind_row_scalar_datediff_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
        return bind_row_scalar_timediff_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
        return bind_row_scalar_timestampdiff_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
        return bind_row_scalar_timestamp_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
        return bind_row_scalar_string_length_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
        return bind_row_scalar_string_codepoint_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
        return bind_row_scalar_string_case_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
        return bind_row_scalar_string_trim_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
        return bind_row_scalar_unix_timestamp_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
        return bind_row_scalar_temporal_extract_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
        return bind_row_scalar_sec_to_time_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
        return bind_row_scalar_from_unixtime_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
        return bind_row_scalar_temporal_constructor_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
        return bind_row_scalar_registered_function_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
        return bind_row_scalar_string_slice_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
        return bind_row_scalar_string_padding_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
        return bind_row_scalar_string_bitmask_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
        return bind_row_scalar_string_search_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
        return bind_row_scalar_string_replace_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
        return bind_row_scalar_string_insert_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
        return bind_row_scalar_string_reverse_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
        return bind_row_scalar_soundex_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
        return bind_row_scalar_string_quote_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
        return bind_row_scalar_substring_index_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
        return bind_row_scalar_find_in_set_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
        return bind_row_scalar_strcmp_expression_parameters(statement, expression, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
        return bind_row_scalar_regexp_like_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
        return bind_row_scalar_regexp_string_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
        return bind_row_scalar_json_valid_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
        return bind_row_scalar_json_search_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
        return bind_row_scalar_json_extract_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
        return bind_row_scalar_json_value_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
        return bind_row_scalar_json_unquote_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
        return bind_row_scalar_json_quote_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
        return bind_row_scalar_json_length_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
        return bind_row_scalar_json_keys_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
        return bind_row_scalar_json_type_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
        return bind_row_scalar_json_set_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
        return bind_row_scalar_json_constructor_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
        return bind_row_scalar_hex_expression_parameters(statement, expression, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
        return bind_row_scalar_base64_expression_parameters(statement, expression, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
        return bind_row_scalar_unhex_expression_parameters(statement, expression, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
        return bind_row_scalar_registered_function_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
        return bind_row_scalar_uuid_expression_parameters(statement, expression, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
        return bind_row_scalar_char_expression_parameters(statement, expression, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        return bind_row_scalar_control_flow_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        return bind_row_scalar_rand_expression_parameters(statement, expression, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
        return bind_row_scalar_conversion_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
        break;
    }

    return MYLITE_ERROR;
}

static int bind_row_scalar_window_function_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (statement == NULL || expression == NULL || parameter_index == NULL ||
        expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION) {
        return MYLITE_MISUSE;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_window_function_argument_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_window_function_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *argument,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (argument == NULL || parameter_index == NULL) {
        return MYLITE_MISUSE;
    }
    switch (argument->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = bind_planned_value_parameter(statement, *parameter_index, &argument->value);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    }
    return MYLITE_ERROR;
}

static int bind_row_scalar_rand_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (statement == NULL || expression == NULL || parameter_index == NULL ||
        expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_RAND) {
        return MYLITE_MISUSE;
    }
    if (!expression->has_rand_seed) {
        return MYLITE_OK;
    }

    rc = bind_int64_parameter(statement, *parameter_index, (int64_t)expression->rand_seed);
    if (rc == MYLITE_OK) {
        ++(*parameter_index);
    }
    return rc;
}

static int bind_row_scalar_conversion_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_conversion_base_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_conversion_base_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = bind_planned_value_parameter(statement, *parameter_index, &expression->value);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        break;
    }

    return MYLITE_ERROR;
}

// Row-scalar parameter binding follows the same recursive expression tree as SQL generation.
// NOLINTNEXTLINE(misc-no-recursion)
static int bind_row_scalar_non_concat_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = bind_planned_value_parameter(statement, *parameter_index, &expression->value);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
        return bind_row_scalar_conversion_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
        return bind_row_scalar_uuid_expression_parameters(statement, expression, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
        return bind_row_scalar_registered_function_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
        break;
    }

    return MYLITE_ERROR;
}

static int bind_row_scalar_integer_arithmetic_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    struct row_scalar_integer_arithmetic_bind_stack stack = {0};
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = row_scalar_integer_arithmetic_bind_stack_push(&stack, expression);
    while (rc == MYLITE_OK && stack.count != 0U) {
        const struct planned_row_scalar_expression *current = stack.items[--stack.count];

        rc = bind_row_scalar_integer_arithmetic_frame_parameters(
            statement,
            &stack,
            current,
            parameter_index
        );
    }
    row_scalar_integer_arithmetic_bind_stack_deinit(&stack);
    return rc;
}

static int bind_row_scalar_integer_arithmetic_frame_parameters(
    sqlite3_stmt *statement,
    struct row_scalar_integer_arithmetic_bind_stack *stack,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = bind_planned_value_parameter(statement, *parameter_index, &expression->value);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
        if (expression->arguments == NULL || expression->argument_count != 2U) {
            return MYLITE_ERROR;
        }
        rc = row_scalar_integer_arithmetic_bind_stack_push(stack, &expression->arguments[1]);
        if (rc == MYLITE_OK) {
            rc = row_scalar_integer_arithmetic_bind_stack_push(stack, &expression->arguments[0]);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        return MYLITE_ERROR;
    }
    return MYLITE_ERROR;
}

static void row_scalar_integer_arithmetic_bind_stack_deinit(
    struct row_scalar_integer_arithmetic_bind_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    free((void *)stack->items);
    *stack = (struct row_scalar_integer_arithmetic_bind_stack){0};
}

static int row_scalar_integer_arithmetic_bind_stack_push(
    struct row_scalar_integer_arithmetic_bind_stack *stack,
    const struct planned_row_scalar_expression *expression
) {
    const struct planned_row_scalar_expression **items = NULL;
    size_t capacity = 0U;

    if (stack == NULL) {
        return MYLITE_MISUSE;
    }
    if (stack->count == stack->capacity) {
        capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;
        if (capacity < stack->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            return MYLITE_NOMEM;
        }
        items = (const struct planned_row_scalar_expression **)
            realloc((void *)stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }
    stack->items[stack->count] = expression;
    ++stack->count;
    return MYLITE_OK;
}

static int bind_row_scalar_field_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count < 2U) {
        return MYLITE_ERROR;
    }

    rc = bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
    for (size_t argument_index = 1U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[0],
            parameter_index
        );
        if (rc == MYLITE_OK) {
            rc = bind_row_scalar_non_concat_expression_parameters(
                statement,
                &expression->arguments[argument_index],
                parameter_index
            );
        }
    }

    return rc;
}

static int bind_row_scalar_reversed_arguments_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count < 2U) {
        return MYLITE_ERROR;
    }
    for (size_t offset = 0U; rc == MYLITE_OK && offset < expression->argument_count; ++offset) {
        size_t argument_index = expression->argument_count - 1U - offset;

        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_interval_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL ||
        expression->argument_count < 2U) {
        return MYLITE_ERROR;
    }

    rc = bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
    for (size_t argument_index = 1U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[0],
            parameter_index
        );
        if (rc == MYLITE_OK) {
            rc = bind_row_scalar_non_concat_expression_parameters(
                statement,
                &expression->arguments[argument_index],
                parameter_index
            );
        }
    }
    return rc;
}

static int bind_row_scalar_date_format_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL ||
        (expression->argument_count != 2U && expression->argument_count != 3U)) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_date_interval_second_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != date_interval_planned_argument_count) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < date_interval_planned_argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_datediff_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 4U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 4U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_timediff_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 4U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 4U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_timestampdiff_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL ||
        expression->argument_count != planned_row_scalar_timestampdiff_argument_count) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < planned_row_scalar_timestampdiff_argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_timestamp_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 4U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 4U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_length_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_string_codepoint_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_string_case_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_string_trim_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_temporal_extract_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count == 0U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_sec_to_time_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_from_unixtime_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_temporal_constructor_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count == 0U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_unix_timestamp_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_slice_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    if (expression->string_slice_kind == PLANNED_STRING_SLICE_FUNCTION_SUBSTRING) {
        return bind_row_scalar_substring_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    }
    return bind_row_scalar_left_right_expression_parameters(statement, expression, parameter_index);
}

static int bind_row_scalar_string_padding_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_bitmask_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_search_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 3U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_replace_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 3U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_insert_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 4U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 4U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_reverse_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_soundex_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_string_quote_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_substring_index_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 3U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_find_in_set_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_strcmp_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_regexp_like_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    rc = bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[1],
        parameter_index
    );
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[0],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_regexp_string_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        (expression->argument_count != 2U && expression->argument_count != 3U)) {
        return MYLITE_ERROR;
    }
    rc = bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[1],
        parameter_index
    );
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[0],
            parameter_index
        );
    }
    if (rc == MYLITE_OK && expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[2],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_json_valid_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_json_search_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || (expression->argument_count != 0U && expression->arguments == NULL)) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_json_introspection_argument_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_json_extract_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    rc = bind_row_scalar_json_extract_document_argument_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[1],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_json_value_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    rc = bind_row_scalar_json_extract_document_argument_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[1],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_json_unquote_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_json_introspection_argument_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_json_quote_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_json_extract_document_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return bind_row_scalar_non_concat_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
        return bind_row_scalar_json_set_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        break;
    }

    return MYLITE_ERROR;
}

static int bind_row_scalar_json_introspection_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return bind_row_scalar_non_concat_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
        return bind_row_scalar_json_set_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
        return bind_row_scalar_json_extract_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    }

    return MYLITE_ERROR;
}

static int bind_row_scalar_json_length_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        (expression->argument_count != 1U && expression->argument_count != 2U)) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_json_introspection_argument_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_json_keys_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        (expression->argument_count != 1U && expression->argument_count != 2U)) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_json_introspection_argument_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_json_type_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_json_introspection_argument_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_json_set_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    if (expression->json_mutation_kind == PLANNED_JSON_MUTATION_REMOVE) {
        if (expression->argument_count < 2U) {
            return MYLITE_ERROR;
        }
    } else if (expression->argument_count < 3U || (expression->argument_count % 2U) == 0U) {
        return MYLITE_ERROR;
    }

    rc = bind_row_scalar_json_set_document_argument_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
    if (expression->json_mutation_kind == PLANNED_JSON_MUTATION_REMOVE) {
        for (size_t argument_index = 1U;
             rc == MYLITE_OK && argument_index < expression->argument_count;
             ++argument_index) {
            rc = bind_row_scalar_non_concat_expression_parameters(
                statement,
                &expression->arguments[argument_index],
                parameter_index
            );
        }
    } else {
        for (size_t argument_index = 1U;
             rc == MYLITE_OK && argument_index < expression->argument_count;
             argument_index += 2U) {
            rc = bind_row_scalar_non_concat_expression_parameters(
                statement,
                &expression->arguments[argument_index],
                parameter_index
            );
            if (rc != MYLITE_OK) {
                break;
            }
            rc = bind_row_scalar_json_set_value_argument_parameters(
                statement,
                &expression->arguments[argument_index + 1U],
                parameter_index
            );
        }
    }
    return rc;
}

static int bind_row_scalar_json_set_document_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return bind_row_scalar_non_concat_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    }

    return MYLITE_ERROR;
}

static int bind_row_scalar_json_set_value_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
        return bind_row_scalar_json_constructor_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    default:
        return bind_row_scalar_non_concat_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    }
}

static int bind_row_scalar_json_constructor_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || (expression->argument_count != 0U && expression->arguments == NULL)) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_control_flow_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || (expression->argument_count != 0U && expression->arguments == NULL)) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_control_flow_argument_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_nested_control_flow_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || (expression->argument_count != 0U && expression->arguments == NULL)) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_control_flow_leaf_argument_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_control_flow_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return bind_row_scalar_non_concat_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        return bind_row_scalar_nested_control_flow_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    }
    return MYLITE_ERROR;
}

static int bind_row_scalar_control_flow_leaf_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return bind_row_scalar_non_concat_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        break;
    }
    return MYLITE_ERROR;
}

static int bind_row_scalar_left_right_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    const struct planned_row_scalar_expression *value = NULL;
    const struct planned_row_scalar_expression *length = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    if (expression->string_slice_kind != PLANNED_STRING_SLICE_FUNCTION_LEFT &&
        expression->string_slice_kind != PLANNED_STRING_SLICE_FUNCTION_RIGHT) {
        return MYLITE_ERROR;
    }
    value = &expression->arguments[0];
    length = &expression->arguments[1];

    rc = bind_row_scalar_non_concat_expression_parameters(statement, value, parameter_index);
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, length, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, length, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, value, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, length, parameter_index);
    }
    return rc;
}

static int bind_row_scalar_substring_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    const struct planned_row_scalar_expression *value = NULL;
    const struct planned_row_scalar_expression *position = NULL;
    const struct planned_row_scalar_expression *length = NULL;
    bool has_length = false;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        (expression->argument_count != 2U && expression->argument_count != 3U)) {
        return MYLITE_ERROR;
    }
    if (expression->string_slice_kind != PLANNED_STRING_SLICE_FUNCTION_SUBSTRING) {
        return MYLITE_ERROR;
    }

    value = &expression->arguments[0];
    position = &expression->arguments[1];
    has_length = expression->argument_count == 3U;
    if (has_length) {
        length = &expression->arguments[2];
    }

    rc = bind_row_scalar_non_concat_expression_parameters(statement, value, parameter_index);
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, position, parameter_index);
    }
    if (rc == MYLITE_OK && has_length) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, length, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, position, parameter_index);
    }
    if (rc == MYLITE_OK && has_length) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, length, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, position, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, value, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, position, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, value, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, position, parameter_index);
    }
    if (rc == MYLITE_OK && has_length) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, length, parameter_index);
    }
    return rc;
}

static int bind_row_scalar_hex_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    const struct planned_row_scalar_expression *argument = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    argument = &expression->arguments[0];

    rc = bind_row_scalar_non_concat_expression_parameters(statement, argument, parameter_index);
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, argument, parameter_index);
    }
    return rc;
}

static int bind_row_scalar_unhex_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_base64_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

// Registered scalar functions delegate argument binding back to the general row-scalar walker.
// NOLINTNEXTLINE(misc-no-recursion)
static int bind_row_scalar_registered_function_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || (expression->argument_count != 0U && expression->arguments == NULL)) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_uuid_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count > 2U ||
        (expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_UUID &&
         (expression->argument_count == 0U || expression->arguments == NULL))) {
        return MYLITE_ERROR;
    }

    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_uuid_argument_parameters(
            statement,
            &expression->arguments[argument_index],
            argument_index,
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_uuid_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *argument,
    size_t argument_index,
    int *parameter_index
) {
    if (argument == NULL) {
        return MYLITE_ERROR;
    }

    switch (argument->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return bind_row_scalar_uuid_leaf_argument_parameters(statement, argument, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
        if (argument_index == 0U) {
            return bind_row_scalar_uuid_inner_expression_parameters(
                statement,
                argument,
                parameter_index
            );
        }
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
        break;
    }
    return MYLITE_ERROR;
}

static int bind_row_scalar_uuid_inner_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count > 2U ||
        (expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_UUID &&
         (expression->argument_count == 0U || expression->arguments == NULL))) {
        return MYLITE_ERROR;
    }

    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_uuid_leaf_argument_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_uuid_leaf_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *argument,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (argument == NULL) {
        return MYLITE_ERROR;
    }

    switch (argument->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = bind_planned_value_parameter(statement, *parameter_index, &argument->value);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
        break;
    }
    return MYLITE_ERROR;
}

static int bind_row_scalar_char_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count == 0U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_select_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate *predicate,
    int *parameter_index
) {
    struct predicate_sql_work_item *items = NULL;
    size_t item_count = 0U;
    int rc = MYLITE_OK;

    if (!planned_select_predicate_has_expression(predicate)) {
        return MYLITE_OK;
    }

    rc = append_predicate_sql_work_node(&items, &item_count, predicate->root_index);
    while (rc == MYLITE_OK && item_count > 0U) {
        struct predicate_sql_work_item item = items[--item_count];
        const struct planned_select_predicate_node *node = NULL;

        if (item.node_index >= predicate->node_count) {
            rc = MYLITE_ERROR;
            continue;
        }

        node = &predicate->nodes[item.node_index];
        if (node->kind == PLANNED_SELECT_PREDICATE_AND ||
            node->kind == PLANNED_SELECT_PREDICATE_OR ||
            node->kind == PLANNED_SELECT_PREDICATE_XOR) {
            rc = append_predicate_sql_work_node(&items, &item_count, node->right_index);
            if (rc == MYLITE_OK) {
                rc = append_predicate_sql_work_node(&items, &item_count, node->left_index);
            }
            continue;
        }
        if (node->kind == PLANNED_SELECT_PREDICATE_NOT) {
            rc = append_predicate_sql_work_node(&items, &item_count, node->left_index);
            continue;
        }
        if (node->kind == PLANNED_SELECT_PREDICATE_EXISTS) {
            rc = bind_select_exists_predicate_parameters(
                statement,
                node->exists_subquery,
                parameter_index
            );
            continue;
        }
        rc = bind_select_predicate_node_parameters(statement, node, parameter_index);
    }

    free(items);
    return rc;
}

static int bind_select_exists_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_exists_subquery *subquery,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (subquery == NULL) {
        return MYLITE_ERROR;
    }
    if (subquery->limit.has_limit && subquery->limit.row_count == 0) {
        return MYLITE_OK;
    }

    rc = bind_select_predicate_parameters_without_exists(
        statement,
        &subquery->predicate,
        parameter_index
    );
    if (rc == MYLITE_OK && subquery->limit.has_limit) {
        rc = bind_int64_parameter(statement, *parameter_index, subquery->limit.row_count);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int bind_select_predicate_parameters_without_exists(
    sqlite3_stmt *statement,
    const struct planned_select_predicate *predicate,
    int *parameter_index
) {
    struct predicate_sql_work_item *items = NULL;
    size_t item_count = 0U;
    int rc = MYLITE_OK;

    if (!planned_select_predicate_has_expression(predicate)) {
        return MYLITE_OK;
    }

    rc = append_predicate_sql_work_node(&items, &item_count, predicate->root_index);
    while (rc == MYLITE_OK && item_count > 0U) {
        struct predicate_sql_work_item item = items[--item_count];
        const struct planned_select_predicate_node *node = NULL;

        if (item.node_index >= predicate->node_count) {
            rc = MYLITE_ERROR;
            continue;
        }

        node = &predicate->nodes[item.node_index];
        if (node->kind == PLANNED_SELECT_PREDICATE_AND ||
            node->kind == PLANNED_SELECT_PREDICATE_OR ||
            node->kind == PLANNED_SELECT_PREDICATE_XOR) {
            rc = append_predicate_sql_work_node(&items, &item_count, node->right_index);
            if (rc == MYLITE_OK) {
                rc = append_predicate_sql_work_node(&items, &item_count, node->left_index);
            }
            continue;
        }
        if (node->kind == PLANNED_SELECT_PREDICATE_NOT) {
            rc = append_predicate_sql_work_node(&items, &item_count, node->left_index);
            continue;
        }
        if (node->kind == PLANNED_SELECT_PREDICATE_EXISTS) {
            rc = MYLITE_ERROR;
            continue;
        }
        rc = bind_select_predicate_node_parameters_without_subqueries(
            statement,
            node,
            parameter_index
        );
    }

    free(items);
    return rc;
}

static int bind_select_predicate_node_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
) {
    if (node->kind == PLANNED_SELECT_PREDICATE_IN) {
        return bind_select_in_predicate_parameters(statement, node, parameter_index);
    }

    return bind_select_non_in_predicate_node_parameters(statement, node, parameter_index);
}

static int bind_select_non_in_predicate_node_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH ||
        node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON ||
        node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL) {
        rc = bind_row_scalar_expression_parameters(
            statement,
            node->row_scalar_expression,
            parameter_index
        );
        if (rc == MYLITE_OK && node->kind == PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON) {
            rc = bind_planned_value_parameter(statement, *parameter_index, &node->value);
            if (rc == MYLITE_OK) {
                ++(*parameter_index);
            }
        }
        return rc;
    }
    if (node->kind != PLANNED_SELECT_PREDICATE_COMPARISON &&
        node->kind != PLANNED_SELECT_PREDICATE_BETWEEN) {
        return MYLITE_OK;
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON && node->value_is_column_reference) {
        return MYLITE_OK;
    }

    rc = bind_planned_value_parameter(statement, *parameter_index, &node->value);
    if (rc == MYLITE_OK) {
        ++(*parameter_index);
    }
    if (rc == MYLITE_OK && node->kind == PLANNED_SELECT_PREDICATE_BETWEEN) {
        rc = bind_planned_value_parameter(statement, *parameter_index, &node->upper_value);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int bind_select_predicate_node_parameters_without_subqueries(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
) {
    if (node->kind == PLANNED_SELECT_PREDICATE_IN) {
        return bind_select_in_literal_predicate_parameters(statement, node, parameter_index);
    }

    return bind_select_non_in_predicate_node_parameters(statement, node, parameter_index);
}

static int bind_select_in_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
) {
    if (node->in_subquery != NULL) {
        return bind_select_in_subquery_predicate_parameters(
            statement,
            node->in_subquery,
            parameter_index
        );
    }

    return bind_select_in_literal_predicate_parameters(statement, node, parameter_index);
}

static int bind_select_in_literal_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (node->in_subquery != NULL) {
        return MYLITE_ERROR;
    }

    for (size_t value_index = 0U; rc == MYLITE_OK && value_index < node->value_count;
         ++value_index) {
        rc = bind_planned_value_parameter(statement, *parameter_index, &node->values[value_index]);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int bind_select_in_subquery_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_in_subquery *subquery,
    int *parameter_index
) {
    if (subquery == NULL) {
        return MYLITE_ERROR;
    }
    return bind_select_predicate_parameters_without_exists(
        statement,
        &subquery->predicate,
        parameter_index
    );
}

static int bind_count_parameters(sqlite3_stmt *statement, const struct planned_count *plan) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    if (plan->function == PLANNED_COUNT_LITERAL) {
        rc = bind_planned_value_parameter(statement, parameter_index, &plan->count_literal);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);
    }

    return rc;
}

static int bind_column_aggregate_parameters(
    sqlite3_stmt *statement,
    const struct planned_column_aggregate *plan
) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    if (plan->has_separator) {
        rc = bind_planned_value_parameter(statement, parameter_index, &plan->separator);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);
    }

    return rc;
}

static int bind_grouped_aggregate_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan
) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    for (size_t aggregate_index = 0U; rc == MYLITE_OK && aggregate_index < plan->aggregate_count;
         ++aggregate_index) {
        const struct planned_grouped_aggregate_item *item = &plan->aggregates[aggregate_index];

        if (item->has_separator) {
            rc = bind_planned_value_parameter(statement, parameter_index, &item->separator);
            if (rc == MYLITE_OK) {
                ++parameter_index;
            }
        }
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);
    }
    if (rc == MYLITE_OK && plan->having.kind == PLANNED_GROUPED_HAVING_COMPARISON) {
        rc = bind_int64_parameter(statement, parameter_index, plan->having.value.integer);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, parameter_index, plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK && plan->limit.has_offset) {
        rc = bind_int64_parameter(statement, parameter_index, plan->limit.offset);
    }

    return rc;
}

static int bind_delete_parameters(sqlite3_stmt *statement, const struct planned_delete *plan) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);
    if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, parameter_index, plan->limit.row_count);
    }

    return rc;
}

static int bind_update_parameters(sqlite3_stmt *statement, const struct planned_update *plan) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    if (planned_update_has_multiple_assignments(plan)) {
        rc = bind_update_multiple_assignment_parameters(statement, &parameter_index, plan);
    } else if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL) {
        rc = bind_update_date_interval_parameters(statement, &parameter_index, plan);
    } else {
        rc = bind_update_assignment_parameter(statement, parameter_index, plan);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK) {
        rc = bind_update_auto_update_parameters(statement, &parameter_index, plan);
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);
    }
    if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, parameter_index, plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK && planned_update_has_multiple_assignments(plan)) {
        rc = bind_update_multiple_changed_condition_parameters(statement, &parameter_index, plan);
    } else if (
        rc == MYLITE_OK && plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL
    ) {
        rc = bind_update_date_interval_parameters(statement, &parameter_index, plan);
    } else if (
        rc == MYLITE_OK &&
        (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC ||
         !plan->assignment_value.is_null)
    ) {
        rc = bind_update_changed_condition_parameter(statement, parameter_index, plan);
    }

    return rc;
}

static int bind_update_multiple_assignment_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->assignment_count; ++index) {
        if (planned_update_assignment_is_noop(&plan->assignments[index])) {
            continue;
        }
        rc = bind_planned_value_parameter(
            statement,
            *parameter_index,
            &plan->assignments[index].value
        );
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int bind_update_date_interval_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
) {
    const char *input_kind_name =
        mylite_date_interval_second_input_kind_name(plan->date_interval_input_kind);
    const char *unit_name = mylite_date_interval_unit_name(plan->date_interval_unit);
    int64_t operation_value = 0;
    int rc = bind_text_parameter(statement, *parameter_index, input_kind_name);

    if (plan->date_interval_subtract) {
        operation_value = 1;
    }

    if (rc == MYLITE_OK) {
        ++(*parameter_index);
        if (plan->date_interval_is_null) {
            rc = mylite_sqlite_status_to_mylite(sqlite3_bind_null(statement, *parameter_index));
        } else {
            rc = bind_int64_parameter(statement, *parameter_index, plan->date_interval_value);
        }
    }
    if (rc == MYLITE_OK) {
        ++(*parameter_index);
        rc = bind_text_parameter(statement, *parameter_index, unit_name);
    }
    if (rc == MYLITE_OK) {
        ++(*parameter_index);
        rc = bind_int64_parameter(statement, *parameter_index, operation_value);
    }
    if (rc == MYLITE_OK) {
        ++(*parameter_index);
    }
    return rc;
}

static int bind_update_auto_update_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (!planned_update_column_has_auto_update(plan, &plan->columns[column_index])) {
            continue;
        }
        rc = bind_planned_value_parameter(statement, *parameter_index, &plan->auto_update_value);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int bind_update_multiple_changed_condition_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->assignment_count; ++index) {
        if (planned_update_assignment_is_noop(&plan->assignments[index])) {
            continue;
        }
        if (plan->assignments[index].value.is_null) {
            continue;
        }
        rc = bind_planned_value_parameter(
            statement,
            *parameter_index,
            &plan->assignments[index].value
        );
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int bind_update_assignment_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct planned_update *plan
) {
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC) {
        return bind_int64_parameter(statement, parameter_index, plan->arithmetic_delta);
    }

    return bind_planned_value_parameter(statement, parameter_index, &plan->assignment_value);
}

static int bind_update_changed_condition_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct planned_update *plan
) {
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC) {
        return bind_int64_parameter(statement, parameter_index, plan->arithmetic_delta);
    }

    return bind_planned_value_parameter(statement, parameter_index, &plan->assignment_value);
}

static int bind_update_matched_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan
) {
    int parameter_index = 1;

    return bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);
}

static int bind_update_matched_count_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan
) {
    int parameter_index = 1;
    int rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);

    if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, parameter_index, plan->limit.row_count);
    }

    return rc;
}

static int bind_planned_value_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct planned_value *value
) {
    int sqlite_rc = SQLITE_OK;

    if (parameter_index <= 0 || value == NULL) {
        return MYLITE_ERROR;
    }
    if (value->is_null) {
        sqlite_rc = sqlite3_bind_null(statement, parameter_index);
    } else if (value->is_text) {
        if (value->text_length > (size_t)INT_MAX) {
            return MYLITE_ERROR;
        }
        sqlite_rc = sqlite3_bind_text(
            statement,
            parameter_index,
            value->text,
            (int)value->text_length,
            SQLITE_TRANSIENT
        );
    } else if (value->is_blob) {
        if (value->text_length > (size_t)INT_MAX) {
            return MYLITE_ERROR;
        }
        sqlite_rc = sqlite3_bind_blob(
            statement,
            parameter_index,
            value->text,
            (int)value->text_length,
            SQLITE_TRANSIENT
        );
    } else if (value->is_real) {
        sqlite_rc = sqlite3_bind_double(statement, parameter_index, value->real);
    } else {
        sqlite_rc = sqlite3_bind_int64(statement, parameter_index, (sqlite3_int64)value->integer);
    }
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int bind_int64_parameter(sqlite3_stmt *statement, int parameter_index, int64_t value) {
    int sqlite_rc = SQLITE_OK;

    if (parameter_index <= 0) {
        return MYLITE_ERROR;
    }

    sqlite_rc = sqlite3_bind_int64(statement, parameter_index, (sqlite3_int64)value);
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int bind_text_parameter(sqlite3_stmt *statement, int parameter_index, const char *value) {
    int sqlite_rc = SQLITE_OK;

    if (parameter_index <= 0 || value == NULL) {
        return MYLITE_ERROR;
    }

    sqlite_rc = sqlite3_bind_text(statement, parameter_index, value, -1, SQLITE_TRANSIENT);
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int append_selected_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    mylite_result *result,
    const struct mylite_catalog_column_descriptor *columns,
    size_t descriptor_count
) {
    size_t column_count = mylite_result_column_count(result);
    struct mylite_result_cell *values = NULL;
    char *texts = NULL;
    size_t text_capacity = approximate_numeric_text_capacity;
    int rc = MYLITE_OK;

    if (column_count > (size_t)INT_MAX || column_count > SIZE_MAX / text_capacity) {
        return MYLITE_NOMEM;
    }

    values = calloc(column_count, sizeof(*values));
    texts = calloc(column_count, text_capacity);
    if (values == NULL || texts == NULL) {
        free(values);
        free(texts);
        return MYLITE_NOMEM;
    }

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        char *text = &texts[column_index * text_capacity];

        rc = append_selected_sqlite_row_value(
            database,
            statement,
            column_index,
            columns,
            descriptor_count,
            text,
            text_capacity,
            &values[column_index]
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_bytes_row(result, values);
    }

    free(values);
    free(texts);

    return rc;
}

static int append_selected_sqlite_row_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *columns,
    size_t descriptor_count,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
) {
    const struct mylite_catalog_column_descriptor *column =
        columns != NULL && column_index < descriptor_count ? &columns[column_index] : NULL;

    return append_selected_sqlite_row_value_with_descriptor(
        database,
        statement,
        column_index,
        column,
        text,
        text_capacity,
        out_value
    );
}

static int append_selected_sqlite_row_value_with_descriptor(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *column,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
) {
    int sqlite_type = SQLITE_NULL;

    if (column_index > (size_t)INT_MAX || out_value == NULL) {
        return MYLITE_ERROR;
    }
    *out_value = (struct mylite_result_cell){.bytes = NULL, .size = 0U, .is_null = true};

    sqlite_type = sqlite3_column_type(statement, (int)column_index);
    if (sqlite_type == SQLITE_NULL) {
        return MYLITE_OK;
    }
    if (sqlite_type == SQLITE_INTEGER) {
        return append_selected_sqlite_integer_value(
            statement,
            column_index,
            text,
            text_capacity,
            out_value
        );
    }
    if (sqlite_type == SQLITE_FLOAT) {
        return append_selected_sqlite_float_value(
            database,
            statement,
            column_index,
            column,
            text,
            text_capacity,
            out_value
        );
    }
    if (sqlite_type == SQLITE_TEXT) {
        return append_selected_sqlite_text_value(statement, column_index, out_value);
    }
    if (sqlite_type == SQLITE_BLOB) {
        return append_selected_sqlite_blob_value(statement, column_index, out_value);
    }

    return MYLITE_ERROR;
}

static int append_selected_sqlite_integer_value(
    sqlite3_stmt *statement,
    size_t column_index,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
) {
    int written = snprintf(
        text,
        text_capacity,
        "%" PRId64,
        (int64_t)sqlite3_column_int64(statement, (int)column_index)
    );

    if (written < 0 || (size_t)written >= text_capacity) {
        return MYLITE_ERROR;
    }
    *out_value = (struct mylite_result_cell){
        .bytes = text,
        .size = (size_t)written,
        .is_null = false,
    };
    return MYLITE_OK;
}

static int append_selected_sqlite_float_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *column,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
) {
    double value = sqlite3_column_double(statement, (int)column_index);
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    int rc = MYLITE_OK;

    if (column != NULL && column_descriptor_is_approximate(column)) {
        rc = approximate_type_info_for_logical_type(column->logical_type, &info);
    }
    if (rc == MYLITE_OK) {
        rc = format_approximate_value_text(
            database,
            value,
            &info,
            "approximate result",
            text,
            text_capacity
        );
    }
    if (rc == MYLITE_OK) {
        *out_value = (struct mylite_result_cell){
            .bytes = text,
            .size = strlen(text),
            .is_null = false,
        };
    }

    return rc;
}

static int append_selected_sqlite_text_value(
    sqlite3_stmt *statement,
    size_t column_index,
    struct mylite_result_cell *out_value
) {
    const unsigned char *sqlite_text = sqlite3_column_text(statement, (int)column_index);
    int byte_count = sqlite3_column_bytes(statement, (int)column_index);
    size_t character_count = 0U;

    if (sqlite_text == NULL || byte_count < 0 ||
        memchr(sqlite_text, '\0', (size_t)byte_count) != NULL ||
        validate_utf8_text((const char *)sqlite_text, (size_t)byte_count, &character_count) !=
            MYLITE_OK) {
        return MYLITE_ERROR;
    }

    (void)character_count;
    *out_value = (struct mylite_result_cell){
        .bytes = sqlite_text,
        .size = (size_t)byte_count,
        .is_null = false,
    };
    return MYLITE_OK;
}

static int append_selected_sqlite_blob_value(
    sqlite3_stmt *statement,
    size_t column_index,
    struct mylite_result_cell *out_value
) {
    const void *sqlite_blob = sqlite3_column_blob(statement, (int)column_index);
    int byte_count = sqlite3_column_bytes(statement, (int)column_index);

    if ((sqlite_blob == NULL && byte_count != 0) || byte_count < 0) {
        return MYLITE_ERROR;
    }

    *out_value = (struct mylite_result_cell){
        .bytes = sqlite_blob,
        .size = (size_t)byte_count,
        .is_null = false,
    };
    return MYLITE_OK;
}

static int choose_sqlite_rowid_alias(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *unsupported_message,
    const char **out_alias
) {
    static const char *const rowid_aliases[] = {"rowid", "_rowid_", "oid"};

    if (out_alias == NULL) {
        return MYLITE_MISUSE;
    }
    *out_alias = NULL;
    for (size_t index = 0U; index < sizeof(rowid_aliases) / sizeof(rowid_aliases[0]); ++index) {
        if (!column_name_exists(columns, column_count, rowid_aliases[index])) {
            *out_alias = rowid_aliases[index];
            return MYLITE_OK;
        }
    }

    set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static bool column_name_exists(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *name
) {
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        if (text_equals_ascii_case_insensitive(columns[column_index].name, name)) {
            return true;
        }
    }

    return false;
}

static const struct mylite_sql_ast_node *child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }

    child = node->first_child;
    for (size_t current = 0U; current < index && child != NULL; ++current) {
        child = child->next_sibling;
    }
    return child;
}

static const struct mylite_sql_ast_node *child_with_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }

    child = node->first_child;
    while (child != NULL) {
        if (child->kind == kind) {
            return child;
        }
        child = child->next_sibling;
    }
    return NULL;
}

static int script_statement_count(const struct mylite_sql_ast_node *root, size_t *out_count) {
    if (root == NULL || root->kind != MYLITE_SQL_AST_SCRIPT || out_count == NULL) {
        return MYLITE_ERROR;
    }

    *out_count = mylite_sql_ast_node_child_count(root);

    return MYLITE_OK;
}

static void set_parse_error(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *near_text = "end of input";
    int written = 0;

    if (parse_result != NULL && parse_result->error_token.text != NULL &&
        parse_result->error_token.length != 0U) {
        near_text = parse_result->error_token.text;
        written = snprintf(
            message,
            sizeof(message),
            "You have an error in your SQL syntax near '%.*s' at line %zu",
            (int)parse_result->error_token.length,
            near_text,
            parse_result->error_token.line
        );
    } else {
        written = snprintf(
            message,
            sizeof(message),
            "You have an error in your SQL syntax near '%s'",
            near_text
        );
    }
    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        message
    );
}
