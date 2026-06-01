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
