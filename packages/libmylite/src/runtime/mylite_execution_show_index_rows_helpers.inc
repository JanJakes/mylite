static int append_show_index(
    struct mylite_db *database,
    mylite_result *result,
    const struct table_name_resolution *target,
    const struct loaded_index_info *index,
    const struct mylite_sql_ast_node *where_clause
) {
    static const char cardinality[] = "0";
    static const char non_unique[] = "1";
    static const char unique[] = "0";
    char sequence[integer_text_capacity];
    const char *non_unique_text = non_unique;
    int rc = MYLITE_OK;

    if (database == NULL || result == NULL || target == NULL || index == NULL) {
        return MYLITE_MISUSE;
    }
    if (index->index.is_unique) {
        non_unique_text = unique;
    }

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        const struct loaded_index_part *part = &index->parts[part_index];
        char prefix_text[integer_text_capacity];
        const char *nullable = show_index_part_nullable_text(part);
        const char *collation = show_index_part_collation_text(index, part);
        const char *prefix_value = NULL;
        const char *index_type = show_index_index_type_text(index);
        bool where_matches = true;

        rc = show_index_part_prefix_value(
            database,
            index,
            part,
            prefix_text,
            sizeof(prefix_text),
            &prefix_value
        );
        if (rc == MYLITE_OK) {
            rc = information_schema_format_i64(
                database,
                part->index_column.ordinal_position,
                sequence,
                sizeof(sequence)
            );
        }
        if (rc == MYLITE_OK) {
            const char *values[show_index_result_column_count] = {
                target->table_name,
                non_unique_text,
                index->index.name,
                sequence,
                part->column.name,
                collation,
                cardinality,
                prefix_value,
                NULL,
                nullable,
                index_type,
                "",
                index->index.comment,
                index_visibility_text(index->index.is_visible),
                NULL,
            };

            if (where_clause != NULL) {
                rc =
                    show_index_where_clause_matches(database, where_clause, values, &where_matches);
            }
            if (rc == MYLITE_OK && where_matches) {
                rc = mylite_result_append_text_row(result, values);
            }
        }
    }

    return rc;
}

static const char *show_index_part_nullable_text(const struct loaded_index_part *part) {
    if (part->column.is_nullable) {
        return "YES";
    }

    return "";
}

static const char *show_index_part_collation_text(
    const struct loaded_index_info *index,
    const struct loaded_index_part *part
) {
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        return NULL;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        return "A";
    }
    if (part->index_column.sort_direction == MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC) {
        return "D";
    }

    return "A";
}

static int show_index_part_prefix_value(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    const struct loaded_index_part *part,
    char *prefix_text,
    size_t prefix_text_size,
    const char **out_prefix_value
) {
    *out_prefix_value = NULL;
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        *out_prefix_value = "32";
        return MYLITE_OK;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT ||
        !part->index_column.has_prefix_length) {
        return MYLITE_OK;
    }

    int rc = information_schema_format_i64(
        database,
        part->index_column.prefix_length,
        prefix_text,
        prefix_text_size
    );

    if (rc == MYLITE_OK) {
        *out_prefix_value = prefix_text;
    }
    return rc;
}

static const char *show_index_index_type_text(const struct loaded_index_info *index) {
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        return "FULLTEXT";
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        return "SPATIAL";
    }

    return "BTREE";
}
