#include "mylite_catalog.h"

#include "mylite_catalog_internal.h"

#include "mylite_connection.h"
#include "mylite_sqlite_registration.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>

enum catalog_table_select_column_index {
    catalog_table_select_table_id_column = 0,
    catalog_table_select_schema_id_column = 1,
    catalog_table_select_name_column = 2,
    catalog_table_select_kind_column = 3,
    catalog_table_select_physical_name_column = 4,
    catalog_table_select_auto_increment_next_column = 5,
    catalog_table_select_auto_increment_status_column = 6,
    catalog_table_select_default_charset_column = 7,
    catalog_table_select_default_collation_column = 8,
    catalog_table_select_comment_column = 9,
    catalog_table_select_row_format_column = 10,
    catalog_table_select_key_block_size_column = 11,
    catalog_table_select_pack_keys_column = 12,
    catalog_table_select_checksum_column = 13,
    catalog_table_select_stats_persistent_column = 14,
    catalog_table_select_stats_auto_recalc_column = 15,
    catalog_table_select_stats_sample_pages_column = 16,
    catalog_table_select_min_rows_column = 17,
    catalog_table_select_max_rows_column = 18,
    catalog_table_select_avg_row_length_column = 19,
    catalog_table_select_delay_key_write_column = 20,
    catalog_table_select_fulltext_doc_id_initialized_column = 21,
    catalog_table_select_created_time_column = 22,
    catalog_table_select_updated_time_column = 23,
    catalog_table_select_descriptor_version_column = 24,
    catalog_table_select_created_generation_column = 25,
    catalog_table_select_updated_generation_column = 26,
};

enum catalog_column_select_column_index {
    catalog_column_select_column_id_column = 0,
    catalog_column_select_table_id_column = 1,
    catalog_column_select_ordinal_position_column = 2,
    catalog_column_select_name_column = 3,
    catalog_column_select_logical_type_column = 4,
    catalog_column_select_physical_type_column = 5,
    catalog_column_select_is_nullable_column = 6,
    catalog_column_select_is_visible_column = 7,
    catalog_column_select_is_auto_increment_column = 8,
    catalog_column_select_default_kind_column = 9,
    catalog_column_select_default_integer_column = 10,
    catalog_column_select_default_text_column = 11,
    catalog_column_select_on_update_current_timestamp_column = 12,
    catalog_column_select_character_set_name_column = 13,
    catalog_column_select_collation_name_column = 14,
    catalog_column_select_comment_column = 15,
    catalog_column_select_is_generated_column = 16,
    catalog_column_select_generated_kind_column = 17,
    catalog_column_select_generation_expression_column = 18,
    catalog_column_select_sqlite_generation_expression_column = 19,
    catalog_column_select_descriptor_version_column = 20,
    catalog_column_select_created_generation_column = 21,
    catalog_column_select_updated_generation_column = 22,
};

enum catalog_view_select_column_index {
    catalog_view_select_table_id_column = 0,
    catalog_view_select_view_definition_column = 1,
    catalog_view_select_show_create_sql_column = 2,
    catalog_view_select_check_option_column = 3,
    catalog_view_select_is_updatable_column = 4,
    catalog_view_select_definer_column = 5,
    catalog_view_select_security_type_column = 6,
    catalog_view_select_character_set_client_column = 7,
    catalog_view_select_collation_connection_column = 8,
    catalog_view_select_source_schema_id_column = 9,
    catalog_view_select_source_table_id_column = 10,
    catalog_view_select_source_schema_name_column = 11,
    catalog_view_select_source_table_name_column = 12,
    catalog_view_select_descriptor_version_column = 13,
    catalog_view_select_created_generation_column = 14,
    catalog_view_select_updated_generation_column = 15,
};

enum catalog_index_select_column_index {
    catalog_index_select_index_id_column = 0,
    catalog_index_select_table_id_column = 1,
    catalog_index_select_name_column = 2,
    catalog_index_select_kind_column = 3,
    catalog_index_select_is_unique_column = 4,
    catalog_index_select_is_visible_column = 5,
    catalog_index_select_physical_name_column = 6,
    catalog_index_select_comment_column = 7,
    catalog_index_select_show_create_explicit_btree_column = 8,
    catalog_index_select_descriptor_version_column = 9,
    catalog_index_select_created_generation_column = 10,
    catalog_index_select_updated_generation_column = 11,
};

enum catalog_index_column_select_column_index {
    catalog_index_column_select_index_column_id_column = 0,
    catalog_index_column_select_index_id_column = 1,
    catalog_index_column_select_table_id_column = 2,
    catalog_index_column_select_column_id_column = 3,
    catalog_index_column_select_ordinal_position_column = 4,
    catalog_index_column_select_prefix_length_column = 5,
    catalog_index_column_select_sort_direction_column = 6,
    catalog_index_column_select_descriptor_version_column = 7,
    catalog_index_column_select_created_generation_column = 8,
    catalog_index_column_select_updated_generation_column = 9,
};

enum catalog_foreign_key_select_column_index {
    catalog_foreign_key_select_foreign_key_id_column = 0,
    catalog_foreign_key_select_child_table_id_column = 1,
    catalog_foreign_key_select_parent_table_id_column = 2,
    catalog_foreign_key_select_name_column = 3,
    catalog_foreign_key_select_parent_index_id_column = 4,
    catalog_foreign_key_select_child_index_id_column = 5,
    catalog_foreign_key_select_update_rule_column = 6,
    catalog_foreign_key_select_delete_rule_column = 7,
    catalog_foreign_key_select_match_option_column = 8,
    catalog_foreign_key_select_descriptor_version_column = 9,
    catalog_foreign_key_select_created_generation_column = 10,
    catalog_foreign_key_select_updated_generation_column = 11,
};

enum catalog_foreign_key_column_select_column_index {
    catalog_foreign_key_column_select_foreign_key_column_id_column = 0,
    catalog_foreign_key_column_select_foreign_key_id_column = 1,
    catalog_foreign_key_column_select_child_table_id_column = 2,
    catalog_foreign_key_column_select_parent_table_id_column = 3,
    catalog_foreign_key_column_select_child_column_id_column = 4,
    catalog_foreign_key_column_select_parent_column_id_column = 5,
    catalog_foreign_key_column_select_ordinal_position_column = 6,
    catalog_foreign_key_column_select_position_in_unique_constraint_column = 7,
    catalog_foreign_key_column_select_descriptor_version_column = 8,
    catalog_foreign_key_column_select_created_generation_column = 9,
    catalog_foreign_key_column_select_updated_generation_column = 10,
};

enum catalog_check_constraint_select_column_index {
    catalog_check_constraint_select_check_constraint_id_column = 0,
    catalog_check_constraint_select_table_id_column = 1,
    catalog_check_constraint_select_name_column = 2,
    catalog_check_constraint_select_physical_name_column = 3,
    catalog_check_constraint_select_check_clause_column = 4,
    catalog_check_constraint_select_sqlite_expression_column = 5,
    catalog_check_constraint_select_is_enforced_column = 6,
    catalog_check_constraint_select_name_is_generated_column = 7,
    catalog_check_constraint_select_generated_ordinal_column = 8,
    catalog_check_constraint_select_ordinal_position_column = 9,
    catalog_check_constraint_select_descriptor_version_column = 10,
    catalog_check_constraint_select_created_generation_column = 11,
    catalog_check_constraint_select_updated_generation_column = 12,
};

enum catalog_next_table_id_column_index {
    catalog_next_table_id_column = 0,
};

enum catalog_schema_select_column_index {
    catalog_schema_select_schema_id_column = 0,
    catalog_schema_select_name_column = 1,
    catalog_schema_select_default_charset_column = 2,
    catalog_schema_select_default_collation_column = 3,
    catalog_schema_select_descriptor_version_column = 4,
    catalog_schema_select_created_generation_column = 5,
    catalog_schema_select_updated_generation_column = 6,
};

static int materialize_schema(
    sqlite3_stmt *statement,
    struct mylite_catalog_schema_descriptor *out_schema
);
static int materialize_table(
    sqlite3_stmt *statement,
    struct mylite_catalog_table_descriptor *out_table
);
static int materialize_table_identity(
    sqlite3_stmt *statement,
    struct mylite_catalog_table_descriptor *out_table
);
static int materialize_table_storage_statistics(
    sqlite3_stmt *statement,
    struct mylite_catalog_table_descriptor *out_table
);
static int materialize_table_lifecycle(
    sqlite3_stmt *statement,
    struct mylite_catalog_table_descriptor *out_table
);
static int validate_materialized_table(const struct mylite_catalog_table_descriptor *table);
static int materialize_view(
    sqlite3_stmt *statement,
    struct mylite_catalog_view_descriptor *out_view
);
static int materialize_view_text_fields(
    sqlite3_stmt *statement,
    struct mylite_catalog_view_descriptor *out_view
);
static int materialize_view_source(
    sqlite3_stmt *statement,
    struct mylite_catalog_view_descriptor *out_view
);
static int materialize_view_generations(
    sqlite3_stmt *statement,
    struct mylite_catalog_view_descriptor *out_view
);
static int validate_materialized_view(const struct mylite_catalog_view_descriptor *view);
static int materialize_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_column_identity(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_column_flags(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_column_defaults(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_column_generated(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_column_generations(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_index(
    sqlite3_stmt *statement,
    struct mylite_catalog_index_descriptor *out_index
);
static int materialize_index_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_index_column_descriptor *out_index_column
);
static int materialize_foreign_key(
    sqlite3_stmt *statement,
    struct mylite_catalog_foreign_key_descriptor *out_foreign_key
);
static int materialize_foreign_key_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_foreign_key_column_descriptor *out_foreign_key_column
);
static int materialize_check_constraint(
    sqlite3_stmt *statement,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint
);
static int try_read_schema_by_name(
    sqlite3 *sqlite,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema,
    bool *out_found
);
static int try_read_table_by_name(
    sqlite3 *sqlite,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table,
    bool *out_found
);

int mylite_catalog_for_each_schema(
    struct mylite_db *database,
    mylite_catalog_schema_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_schema_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT schema_id, name, default_charset, default_collation, "
        "descriptor_version, created_catalog_generation, "
        "updated_catalog_generation FROM _mylite_catalog_schemas ORDER BY name",
        &statement
    );
    while (rc == MYLITE_OK) {
        struct mylite_catalog_schema_descriptor schema = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_schema(statement, &schema);
        if (rc == MYLITE_OK) {
            rc = callback(&schema, user_data);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_table_in_schema(
    struct mylite_db *database,
    int64_t schema_id,
    mylite_catalog_table_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_table_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT table_id, schema_id, name, kind, physical_name, auto_increment_next, "
        "auto_increment_status, default_charset, default_collation, comment, row_format_option, "
        "key_block_size, "
        "pack_keys, checksum, stats_persistent, stats_auto_recalc, stats_sample_pages, "
        "min_rows, max_rows, avg_row_length, delay_key_write, "
        "fulltext_doc_id_initialized, "
        "created_time_utc_epoch, updated_time_utc_epoch, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_tables WHERE schema_id = ?1 ORDER BY name",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_table_descriptor table = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_table(statement, &table);
        if (rc == MYLITE_OK) {
            rc = callback(&table, user_data);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_column_in_table(
    struct mylite_db *database,
    int64_t table_id,
    mylite_catalog_column_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_column_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, character_set_name, collation_name, comment, "
        "is_generated, generated_kind, generation_expression, sqlite_generation_expression, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns WHERE table_id = ?1 ORDER BY ordinal_position",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_column_descriptor column = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_column(statement, &column);
        if (rc == MYLITE_OK) {
            rc = callback(&column, user_data);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_index_in_table(
    struct mylite_db *database,
    int64_t table_id,
    mylite_catalog_index_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_index_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT index_id, table_id, name, kind, is_unique, is_visible, physical_name, "
        "comment, show_create_explicit_btree, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_indexes WHERE table_id = ?1 ORDER BY index_id",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_index_descriptor index = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_index(statement, &index);
        if (rc == MYLITE_OK) {
            rc = callback(&index, user_data);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_index_column_in_index(
    struct mylite_db *database,
    int64_t index_id,
    mylite_catalog_index_column_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(index_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_index_column_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT index_column_id, index_id, table_id, column_id, ordinal_position, "
        "prefix_length, sort_direction, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_index_columns "
        "WHERE index_id = ?1 ORDER BY ordinal_position",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, index_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_index_column_descriptor index_column = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_index_column(statement, &index_column);
        if (rc == MYLITE_OK) {
            rc = callback(&index_column, user_data);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_foreign_key_in_child_table(
    struct mylite_db *database,
    int64_t child_table_id,
    mylite_catalog_foreign_key_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(child_table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_foreign_key_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT foreign_key_id, child_table_id, parent_table_id, name, parent_index_id, "
        "child_index_id, update_rule, delete_rule, match_option, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_keys WHERE child_table_id = ?1 ORDER BY foreign_key_id",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, child_table_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_foreign_key_descriptor foreign_key = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_foreign_key(statement, &foreign_key);
        if (rc == MYLITE_OK) {
            rc = callback(&foreign_key, user_data);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_foreign_key_for_parent_table(
    struct mylite_db *database,
    int64_t parent_table_id,
    mylite_catalog_foreign_key_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(parent_table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_foreign_key_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT foreign_key_id, child_table_id, parent_table_id, name, parent_index_id, "
        "child_index_id, update_rule, delete_rule, match_option, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_keys WHERE parent_table_id = ?1 ORDER BY foreign_key_id",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, parent_table_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_foreign_key_descriptor foreign_key = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_foreign_key(statement, &foreign_key);
        if (rc == MYLITE_OK) {
            rc = callback(&foreign_key, user_data);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_foreign_key_column_in_foreign_key(
    struct mylite_db *database,
    int64_t foreign_key_id,
    mylite_catalog_foreign_key_column_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(foreign_key_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_foreign_key_column_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT foreign_key_column_id, foreign_key_id, child_table_id, parent_table_id, "
        "child_column_id, parent_column_id, ordinal_position, position_in_unique_constraint, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_key_columns "
        "WHERE foreign_key_id = ?1 ORDER BY ordinal_position",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, foreign_key_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_foreign_key_column_descriptor foreign_key_column = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_foreign_key_column(statement, &foreign_key_column);
        if (rc == MYLITE_OK) {
            rc = callback(&foreign_key_column, user_data);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_check_constraint_in_table(
    struct mylite_db *database,
    int64_t table_id,
    mylite_catalog_check_constraint_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_check_constraint_callback(callback);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT check_constraint_id, table_id, name, physical_name, check_clause, "
        "sqlite_expression, is_enforced, name_is_generated, generated_ordinal, "
        "ordinal_position, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_check_constraints WHERE table_id = ?1 ORDER BY name",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_check_constraint_descriptor check_constraint = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_check_constraint(statement, &check_constraint);
        if (rc == MYLITE_OK) {
            rc = callback(&check_constraint, user_data);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_check_constraint_in_schema(
    struct mylite_db *database,
    int64_t schema_id,
    mylite_catalog_check_constraint_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_check_constraint_callback(callback);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT c.check_constraint_id, c.table_id, c.name, c.physical_name, c.check_clause, "
        "c.sqlite_expression, c.is_enforced, c.name_is_generated, c.generated_ordinal, "
        "c.ordinal_position, c.descriptor_version, c.created_catalog_generation, "
        "c.updated_catalog_generation "
        "FROM _mylite_catalog_check_constraints c "
        "JOIN _mylite_catalog_tables t ON t.table_id = c.table_id "
        "WHERE t.schema_id = ?1 ORDER BY c.name",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_check_constraint_descriptor check_constraint = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_check_constraint(statement, &check_constraint);
        if (rc == MYLITE_OK) {
            rc = callback(&check_constraint, user_data);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_try_read_check_constraint_by_physical_name(
    struct mylite_db *database,
    int64_t table_id,
    const char *physical_name,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (out_check_constraint != NULL) {
        *out_check_constraint = (struct mylite_catalog_check_constraint_descriptor){0};
    }
    if (out_found != NULL) {
        *out_found = false;
    }
    if (rc == MYLITE_OK && (out_check_constraint == NULL || out_found == NULL)) {
        return MYLITE_MISUSE;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            physical_name,
            MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT check_constraint_id, table_id, name, physical_name, check_clause, "
        "sqlite_expression, is_enforced, name_is_generated, generated_ordinal, "
        "ordinal_position, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_check_constraints "
        "WHERE table_id = ?1 AND physical_name = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, physical_name);
    }
    if (rc == MYLITE_OK) {
        int sqlite_rc = sqlite3_step(statement);

        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_check_constraint(statement, out_check_constraint);
            *out_found = rc == MYLITE_OK;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_try_read_primary_index_by_table_id(
    struct mylite_db *database,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index,
    bool *out_found
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_index == NULL || out_found == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found = false;
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_catalog_try_read_primary_index_by_table_id_from_sqlite(
        database->sqlite,
        table_id,
        out_index,
        out_found
    );
}

int mylite_catalog_read_schema_by_name(
    struct mylite_db *database,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    bool found = false;
    int rc = mylite_catalog_try_read_schema_by_name(database, name, out_schema, &found);

    if (rc != MYLITE_OK) {
        return rc;
    }

    if (!found) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

int mylite_catalog_read_schema_by_id(
    struct mylite_db *database,
    int64_t schema_id,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(schema_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_catalog_read_schema_by_id_from_sqlite(database->sqlite, schema_id, out_schema);
}

int mylite_catalog_try_read_schema_by_name(
    struct mylite_db *database,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema,
    bool *out_found
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_schema == NULL || out_found == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found = false;
    rc = mylite_catalog_validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return try_read_schema_by_name(database->sqlite, name, out_schema, out_found);
}

int mylite_catalog_read_table_by_name(
    struct mylite_db *database,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table
) {
    bool found = false;
    int rc = mylite_catalog_try_read_table_by_name(database, schema_id, name, out_table, &found);

    if (rc != MYLITE_OK) {
        return rc;
    }

    if (!found) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

int mylite_catalog_try_read_table_by_name(
    struct mylite_db *database,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table,
    bool *out_found
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_table == NULL || out_found == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found = false;
    rc = mylite_catalog_validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return try_read_table_by_name(database->sqlite, schema_id, name, out_table, out_found);
}

int mylite_catalog_read_table_by_id(
    struct mylite_db *database,
    int64_t table_id,
    struct mylite_catalog_table_descriptor *out_table
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_table == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_catalog_read_table_by_id_from_sqlite(database->sqlite, table_id, out_table);
}

int mylite_catalog_read_view_by_table_id(
    struct mylite_db *database,
    int64_t table_id,
    struct mylite_catalog_view_descriptor *out_view
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_view == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_catalog_read_view_by_table_id_from_sqlite(database->sqlite, table_id, out_view);
}

int mylite_catalog_read_column_by_name(
    struct mylite_db *database,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_column_descriptor *out_column
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_column == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_catalog_read_column_by_name_from_sqlite(
        database->sqlite,
        table_id,
        name,
        out_column
    );
}

int mylite_catalog_read_inserted_index_column(
    struct mylite_db *database,
    int64_t index_id,
    int64_t ordinal_position,
    struct mylite_catalog_index_column_descriptor *out_index_column
) {
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT index_column_id, index_id, table_id, column_id, ordinal_position, "
        "prefix_length, sort_direction, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_index_columns "
        "WHERE index_id = ?1 AND ordinal_position = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        int sqlite_rc = sqlite3_step(statement);

        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_index_column(statement, out_index_column);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            if (sqlite_rc == SQLITE_DONE) {
                rc = MYLITE_ERROR;
            }
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_inserted_foreign_key(
    struct mylite_db *database,
    int64_t child_table_id,
    const char *name,
    struct mylite_catalog_foreign_key_descriptor *out_foreign_key
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT foreign_key_id, child_table_id, parent_table_id, name, parent_index_id, "
        "child_index_id, update_rule, delete_rule, match_option, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_keys WHERE child_table_id = ?1 AND name = ?2",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, name);
    }
    if (rc == MYLITE_OK) {
        int sqlite_rc = sqlite3_step(statement);

        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_foreign_key(statement, out_foreign_key);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            if (sqlite_rc == SQLITE_DONE) {
                rc = MYLITE_ERROR;
            }
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_inserted_foreign_key_column(
    struct mylite_db *database,
    int64_t foreign_key_id,
    int64_t ordinal_position,
    struct mylite_catalog_foreign_key_column_descriptor *out_foreign_key_column
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT foreign_key_column_id, foreign_key_id, child_table_id, parent_table_id, "
        "child_column_id, parent_column_id, ordinal_position, position_in_unique_constraint, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_key_columns "
        "WHERE foreign_key_id = ?1 AND ordinal_position = ?2",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, foreign_key_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        int sqlite_rc = sqlite3_step(statement);

        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_foreign_key_column(statement, out_foreign_key_column);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            if (sqlite_rc == SQLITE_DONE) {
                rc = MYLITE_ERROR;
            }
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_inserted_check_constraint(
    struct mylite_db *database,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT check_constraint_id, table_id, name, physical_name, check_clause, "
        "sqlite_expression, is_enforced, name_is_generated, generated_ordinal, "
        "ordinal_position, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_check_constraints WHERE table_id = ?1 AND name = ?2",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, name);
    }
    if (rc == MYLITE_OK) {
        int sqlite_rc = sqlite3_step(statement);

        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_check_constraint(statement, out_check_constraint);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            if (sqlite_rc == SQLITE_DONE) {
                rc = MYLITE_ERROR;
            }
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_schema_by_name_from_sqlite(
    sqlite3 *sqlite,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    bool found = false;
    int rc = try_read_schema_by_name(sqlite, name, out_schema, &found);

    if (rc != MYLITE_OK) {
        return rc;
    }

    if (!found) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int try_read_schema_by_name(
    sqlite3 *sqlite,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT schema_id, name, default_charset, default_collation, "
        "descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_schemas WHERE name = ?1",
        &statement
    );

    *out_schema = (struct mylite_catalog_schema_descriptor){0};
    *out_found = false;
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_schema(statement, out_schema);
            if (rc == MYLITE_OK) {
                *out_found = true;
            }
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        } else {
            *out_schema = (struct mylite_catalog_schema_descriptor){0};
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_schema_by_id_from_sqlite(
    sqlite3 *sqlite,
    int64_t schema_id,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT schema_id, name, default_charset, default_collation, "
        "descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_schemas WHERE schema_id = ?1",
        &statement
    );

    *out_schema = (struct mylite_catalog_schema_descriptor){0};
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_schema(statement, out_schema);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_table_by_name_from_sqlite(
    sqlite3 *sqlite,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table
) {
    bool found = false;
    int rc = try_read_table_by_name(sqlite, schema_id, name, out_table, &found);

    if (rc != MYLITE_OK) {
        return rc;
    }

    if (!found) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int try_read_table_by_name(
    sqlite3 *sqlite,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT table_id, schema_id, name, kind, physical_name, auto_increment_next, "
        "auto_increment_status, default_charset, default_collation, comment, row_format_option, "
        "key_block_size, "
        "pack_keys, checksum, stats_persistent, stats_auto_recalc, stats_sample_pages, "
        "min_rows, max_rows, avg_row_length, delay_key_write, "
        "fulltext_doc_id_initialized, "
        "created_time_utc_epoch, updated_time_utc_epoch, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_tables WHERE schema_id = ?1 AND name = ?2",
        &statement
    );

    *out_table = (struct mylite_catalog_table_descriptor){0};
    *out_found = false;
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, name);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_table(statement, out_table);
            if (rc == MYLITE_OK) {
                *out_found = true;
            }
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        } else {
            *out_table = (struct mylite_catalog_table_descriptor){0};
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_table_by_id_from_sqlite(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_table_descriptor *out_table
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT table_id, schema_id, name, kind, physical_name, auto_increment_next, "
        "auto_increment_status, default_charset, default_collation, comment, row_format_option, "
        "key_block_size, "
        "pack_keys, checksum, stats_persistent, stats_auto_recalc, stats_sample_pages, "
        "min_rows, max_rows, avg_row_length, delay_key_write, "
        "fulltext_doc_id_initialized, "
        "created_time_utc_epoch, updated_time_utc_epoch, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_tables WHERE table_id = ?1",
        &statement
    );

    *out_table = (struct mylite_catalog_table_descriptor){0};
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_table(statement, out_table);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_view_by_table_id_from_sqlite(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_view_descriptor *out_view
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT table_id, view_definition, show_create_sql, check_option, is_updatable, "
        "definer, security_type, character_set_client, collation_connection, "
        "source_schema_id, source_table_id, source_schema_name, source_table_name, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_views WHERE table_id = ?1",
        &statement
    );

    *out_view = (struct mylite_catalog_view_descriptor){0};
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_view(statement, out_view);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_column_by_name_from_sqlite(
    sqlite3 *sqlite,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_column_descriptor *out_column
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, character_set_name, collation_name, comment, "
        "is_generated, generated_kind, generation_expression, sqlite_generation_expression, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns WHERE table_id = ?1 AND name = ?2",
        &statement
    );

    *out_column = (struct mylite_catalog_column_descriptor){0};
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, name);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_column(statement, out_column);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_try_read_primary_index_by_table_id_from_sqlite(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT index_id, table_id, name, kind, is_unique, is_visible, physical_name, "
        "comment, show_create_explicit_btree, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_indexes WHERE table_id = ?1 AND kind = 1",
        &statement
    );

    *out_index = (struct mylite_catalog_index_descriptor){0};
    *out_found = false;
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_index(statement, out_index);
            if (rc == MYLITE_OK) {
                *out_found = true;
            }
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        } else {
            *out_index = (struct mylite_catalog_index_descriptor){0};
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_next_table_id(sqlite3 *sqlite, int64_t *out_table_id) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT COALESCE(MAX(table_id), 0) + 1 FROM _mylite_catalog_tables",
        &statement
    );

    *out_table_id = 0;
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = mylite_catalog_checked_column_i64(
                statement,
                catalog_next_table_id_column,
                out_table_id
            );
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(*out_table_id);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_next_index_id(sqlite3 *sqlite, int64_t *out_index_id) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT COALESCE(MAX(index_id), 0) + 1 FROM _mylite_catalog_indexes",
        &statement
    );

    *out_index_id = 0;
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = mylite_catalog_checked_column_i64(statement, 0, out_index_id);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(*out_index_id);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_next_foreign_key_id(sqlite3 *sqlite, int64_t *out_foreign_key_id) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT COALESCE(MAX(foreign_key_id), 0) + 1 FROM _mylite_catalog_foreign_keys",
        &statement
    );

    *out_foreign_key_id = 0;
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = mylite_catalog_checked_column_i64(statement, 0, out_foreign_key_id);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(*out_foreign_key_id);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_read_next_check_constraint_id(
    sqlite3 *sqlite,
    int64_t *out_check_constraint_id
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT COALESCE(MAX(check_constraint_id), 0) + 1 "
        "FROM _mylite_catalog_check_constraints",
        &statement
    );

    *out_check_constraint_id = 0;
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = mylite_catalog_checked_column_i64(statement, 0, out_check_constraint_id);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(*out_check_constraint_id);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int materialize_schema(
    sqlite3_stmt *statement,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_schema_select_schema_id_column,
        &out_schema->schema_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_schema_select_name_column,
            out_schema->name,
            sizeof(out_schema->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_schema_select_default_charset_column,
            out_schema->default_charset,
            sizeof(out_schema->default_charset)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_schema_select_default_collation_column,
            out_schema->default_collation,
            sizeof(out_schema->default_collation)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_schema_select_descriptor_version_column,
            &out_schema->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_schema_select_created_generation_column,
            &out_schema->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_schema_select_updated_generation_column,
            &out_schema->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_table(
    sqlite3_stmt *statement,
    struct mylite_catalog_table_descriptor *out_table
) {
    int rc = materialize_table_identity(statement, out_table);

    if (rc == MYLITE_OK) {
        rc = materialize_table_storage_statistics(statement, out_table);
    }
    if (rc == MYLITE_OK) {
        rc = materialize_table_lifecycle(statement, out_table);
    }
    if (rc == MYLITE_OK) {
        rc = validate_materialized_table(out_table);
    }
    return rc;
}

static int materialize_table_identity(
    sqlite3_stmt *statement,
    struct mylite_catalog_table_descriptor *out_table
) {
    int64_t kind = 0;
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_table_select_table_id_column,
        &out_table->table_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_schema_id_column,
            &out_table->schema_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_table_select_name_column,
            out_table->name,
            sizeof(out_table->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(statement, catalog_table_select_kind_column, &kind);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_table_kind((enum mylite_catalog_table_kind)kind);
    }
    if (rc == MYLITE_OK) {
        out_table->kind = (enum mylite_catalog_table_kind)kind;
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_table_select_physical_name_column,
            out_table->physical_name,
            sizeof(out_table->physical_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_auto_increment_next_column,
            &out_table->auto_increment_next
        );
    }
    if (rc == MYLITE_OK && out_table->auto_increment_next <= 0) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_auto_increment_status_column,
            &out_table->auto_increment_status
        );
    }
    if (rc == MYLITE_OK && out_table->auto_increment_status < 0) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_table_select_default_charset_column,
            out_table->default_charset,
            sizeof(out_table->default_charset)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_table_select_default_collation_column,
            out_table->default_collation,
            sizeof(out_table->default_collation)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_table_select_comment_column,
            out_table->comment,
            sizeof(out_table->comment)
        );
    }
    return rc;
}

static int materialize_table_storage_statistics(
    sqlite3_stmt *statement,
    struct mylite_catalog_table_descriptor *out_table
) {
    int rc = mylite_catalog_checked_column_text(
        statement,
        catalog_table_select_row_format_column,
        out_table->row_format_option,
        sizeof(out_table->row_format_option)
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_key_block_size_column,
            &out_table->key_block_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_pack_keys_column,
            &out_table->pack_keys
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_checksum_column,
            &out_table->checksum
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_stats_persistent_column,
            &out_table->stats_persistent
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_stats_auto_recalc_column,
            &out_table->stats_auto_recalc
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_stats_sample_pages_column,
            &out_table->stats_sample_pages
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_min_rows_column,
            &out_table->min_rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_max_rows_column,
            &out_table->max_rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_avg_row_length_column,
            &out_table->avg_row_length
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_delay_key_write_column,
            &out_table->delay_key_write
        );
    }
    return rc;
}

static int materialize_table_lifecycle(
    sqlite3_stmt *statement,
    struct mylite_catalog_table_descriptor *out_table
) {
    int64_t fulltext_doc_id_initialized = 0;
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_table_select_fulltext_doc_id_initialized_column,
        &fulltext_doc_id_initialized
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(
            fulltext_doc_id_initialized,
            &out_table->fulltext_doc_id_initialized
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_created_time_column,
            &out_table->created_time_utc_epoch
        );
    }
    if (rc == MYLITE_OK && out_table->created_time_utc_epoch < 0) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_updated_time_column,
            &out_table->updated_time_utc_epoch
        );
    }
    if (rc == MYLITE_OK && out_table->updated_time_utc_epoch < 0) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_table_select_descriptor_version_column,
            &out_table->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_table_select_created_generation_column,
            &out_table->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_table_select_updated_generation_column,
            &out_table->updated_catalog_generation
        );
    }

    return rc;
}

static int validate_materialized_table(const struct mylite_catalog_table_descriptor *table) {
    if (table == NULL) {
        return MYLITE_MISUSE;
    }
    return mylite_catalog_validate_table_descriptor_input(
        &(const struct mylite_catalog_table_descriptor_input){
            .schema_id = table->schema_id,
            .name = table->name,
            .physical_name = table->physical_name,
            .kind = table->kind,
            .auto_increment_status = table->auto_increment_status,
            .default_charset = table->default_charset,
            .default_collation = table->default_collation,
            .comment = table->comment,
            .row_format_option = table->row_format_option,
            .key_block_size = table->key_block_size,
            .pack_keys = table->pack_keys,
            .checksum = table->checksum,
            .stats_persistent = table->stats_persistent,
            .stats_auto_recalc = table->stats_auto_recalc,
            .stats_sample_pages = table->stats_sample_pages,
            .min_rows = table->min_rows,
            .max_rows = table->max_rows,
            .avg_row_length = table->avg_row_length,
            .delay_key_write = table->delay_key_write,
            .created_time_utc_epoch = table->created_time_utc_epoch,
            .updated_time_utc_epoch = table->updated_time_utc_epoch,
        }
    );
}

static int materialize_view(
    sqlite3_stmt *statement,
    struct mylite_catalog_view_descriptor *out_view
) {
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_view_select_table_id_column,
        &out_view->table_id
    );

    if (rc == MYLITE_OK) {
        rc = materialize_view_text_fields(statement, out_view);
    }
    if (rc == MYLITE_OK) {
        rc = materialize_view_source(statement, out_view);
    }
    if (rc == MYLITE_OK) {
        rc = materialize_view_generations(statement, out_view);
    }
    if (rc == MYLITE_OK) {
        rc = validate_materialized_view(out_view);
    }
    return rc;
}

static int materialize_view_text_fields(
    sqlite3_stmt *statement,
    struct mylite_catalog_view_descriptor *out_view
) {
    int rc = mylite_catalog_checked_column_text(
        statement,
        catalog_view_select_view_definition_column,
        out_view->view_definition,
        sizeof(out_view->view_definition)
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_view_select_show_create_sql_column,
            out_view->show_create_sql,
            sizeof(out_view->show_create_sql)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_view_select_check_option_column,
            out_view->check_option,
            sizeof(out_view->check_option)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_view_select_is_updatable_column,
            out_view->is_updatable,
            sizeof(out_view->is_updatable)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_view_select_definer_column,
            out_view->definer,
            sizeof(out_view->definer)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_view_select_security_type_column,
            out_view->security_type,
            sizeof(out_view->security_type)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_view_select_character_set_client_column,
            out_view->character_set_client,
            sizeof(out_view->character_set_client)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_view_select_collation_connection_column,
            out_view->collation_connection,
            sizeof(out_view->collation_connection)
        );
    }

    return rc;
}

static int materialize_view_source(
    sqlite3_stmt *statement,
    struct mylite_catalog_view_descriptor *out_view
) {
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_view_select_source_schema_id_column,
        &out_view->source_schema_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_view_select_source_table_id_column,
            &out_view->source_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_view_select_source_schema_name_column,
            out_view->source_schema_name,
            sizeof(out_view->source_schema_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_view_select_source_table_name_column,
            out_view->source_table_name,
            sizeof(out_view->source_table_name)
        );
    }

    return rc;
}

static int materialize_view_generations(
    sqlite3_stmt *statement,
    struct mylite_catalog_view_descriptor *out_view
) {
    int rc = mylite_catalog_checked_column_u64(
        statement,
        catalog_view_select_descriptor_version_column,
        &out_view->descriptor_version
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_view_select_created_generation_column,
            &out_view->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_view_select_updated_generation_column,
            &out_view->updated_catalog_generation
        );
    }

    return rc;
}

static int validate_materialized_view(const struct mylite_catalog_view_descriptor *view) {
    int rc = mylite_catalog_validate_positive_id(view->table_id);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            view->view_definition,
            MYLITE_CATALOG_VIEW_DEFINITION_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            view->show_create_sql,
            MYLITE_CATALOG_VIEW_SHOW_CREATE_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(view->check_option, sizeof(view->check_option));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(view->is_updatable, sizeof(view->is_updatable));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(view->definer, sizeof(view->definer));
    }
    if (rc == MYLITE_OK) {
        rc =
            mylite_catalog_validate_required_name(view->security_type, sizeof(view->security_type));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            view->character_set_client,
            sizeof(view->character_set_client)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            view->collation_connection,
            sizeof(view->collation_connection)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(view->source_schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(view->source_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            view->source_schema_name,
            sizeof(view->source_schema_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            view->source_table_name,
            sizeof(view->source_table_name)
        );
    }
    return rc;
}

static int materialize_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int rc = materialize_column_identity(statement, out_column);

    if (rc == MYLITE_OK) {
        rc = materialize_column_flags(statement, out_column);
    }
    if (rc == MYLITE_OK) {
        rc = materialize_column_defaults(statement, out_column);
    }
    if (rc == MYLITE_OK) {
        rc = materialize_column_generated(statement, out_column);
    }
    if (rc == MYLITE_OK) {
        rc = materialize_column_generations(statement, out_column);
    }

    return rc;
}

static int materialize_column_identity(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_column_select_column_id_column,
        &out_column->column_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_column_select_table_id_column,
            &out_column->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_column_select_ordinal_position_column,
            &out_column->ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_name_column,
            out_column->name,
            sizeof(out_column->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_logical_type_column,
            out_column->logical_type,
            sizeof(out_column->logical_type)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_physical_type_column,
            out_column->physical_type,
            sizeof(out_column->physical_type)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_character_set_name_column,
            out_column->character_set_name,
            sizeof(out_column->character_set_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_collation_name_column,
            out_column->collation_name,
            sizeof(out_column->collation_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_comment_column,
            out_column->comment,
            sizeof(out_column->comment)
        );
    }

    return rc;
}

static int materialize_column_flags(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int64_t nullable = 0;
    int64_t visible = 0;
    int64_t auto_increment = 0;
    int64_t on_update_current_timestamp = 0;
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_column_select_is_nullable_column,
        &nullable
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(nullable, &out_column->is_nullable);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_column_select_is_visible_column,
            &visible
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(visible, &out_column->is_visible);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_column_select_is_auto_increment_column,
            &auto_increment
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(auto_increment, &out_column->is_auto_increment);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_column_select_on_update_current_timestamp_column,
            &on_update_current_timestamp
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(
            on_update_current_timestamp,
            &out_column->on_update_current_timestamp
        );
    }

    return rc;
}

static int materialize_column_defaults(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int64_t default_kind = 0;
    bool has_default_integer = false;
    bool has_default_text = false;
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_column_select_default_kind_column,
        &default_kind
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_column_default_kind((enum mylite_catalog_column_default_kind
        )default_kind);
    }
    if (rc == MYLITE_OK) {
        out_column->default_kind = (enum mylite_catalog_column_default_kind)default_kind;
        rc = mylite_catalog_checked_nullable_column_i64(
            statement,
            catalog_column_select_default_integer_column,
            &has_default_integer,
            &out_column->default_integer
        );
    }
    if (rc == MYLITE_OK &&
        ((mylite_catalog_column_default_kind_stores_integer(out_column->default_kind) &&
          !has_default_integer) ||
         (!mylite_catalog_column_default_kind_stores_integer(out_column->default_kind) &&
          has_default_integer))) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_nullable_column_text(
            statement,
            catalog_column_select_default_text_column,
            &has_default_text,
            out_column->default_text,
            sizeof(out_column->default_text)
        );
    }
    if (rc == MYLITE_OK &&
        ((mylite_catalog_column_default_kind_stores_text(out_column->default_kind) &&
          !has_default_text) ||
         (!mylite_catalog_column_default_kind_stores_text(out_column->default_kind) &&
          has_default_text))) {
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int materialize_column_generations(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int rc = mylite_catalog_checked_column_u64(
        statement,
        catalog_column_select_descriptor_version_column,
        &out_column->descriptor_version
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_column_select_created_generation_column,
            &out_column->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_column_select_updated_generation_column,
            &out_column->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_column_generated(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int64_t is_generated = 0;
    int64_t generated_kind = 0;
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_column_select_is_generated_column,
        &is_generated
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(is_generated, &out_column->is_generated);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_column_select_generated_kind_column,
            &generated_kind
        );
    }
    if (rc == MYLITE_OK) {
        out_column->generated_kind = (enum mylite_catalog_generated_column_kind)generated_kind;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_generation_expression_column,
            out_column->generation_expression,
            sizeof(out_column->generation_expression)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_sqlite_generation_expression_column,
            out_column->sqlite_generation_expression,
            sizeof(out_column->sqlite_generation_expression)
        );
    }

    return rc;
}

static int materialize_index(
    sqlite3_stmt *statement,
    struct mylite_catalog_index_descriptor *out_index
) {
    int64_t kind = 0;
    int64_t is_unique = 0;
    int64_t is_visible = 0;
    int64_t show_create_explicit_btree = 0;
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_index_select_index_id_column,
        &out_index->index_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_select_table_id_column,
            &out_index->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_index_select_name_column,
            out_index->name,
            sizeof(out_index->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(statement, catalog_index_select_kind_column, &kind);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_index_kind((enum mylite_catalog_index_kind)kind);
    }
    if (rc == MYLITE_OK) {
        out_index->kind = (enum mylite_catalog_index_kind)kind;
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_select_is_unique_column,
            &is_unique
        );
    }
    if (rc == MYLITE_OK && is_unique != 0 && is_unique != 1) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_index->is_unique = is_unique != 0;
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_select_is_visible_column,
            &is_visible
        );
    }
    if (rc == MYLITE_OK && is_visible != 0 && is_visible != 1) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_index->is_visible = is_visible != 0;
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_index_select_physical_name_column,
            out_index->physical_name,
            sizeof(out_index->physical_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_index_select_comment_column,
            out_index->comment,
            sizeof(out_index->comment)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_select_show_create_explicit_btree_column,
            &show_create_explicit_btree
        );
    }
    if (rc == MYLITE_OK && show_create_explicit_btree != 0 && show_create_explicit_btree != 1) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_index->show_create_explicit_btree = show_create_explicit_btree != 0;
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_index_select_descriptor_version_column,
            &out_index->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_index_select_created_generation_column,
            &out_index->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_index_select_updated_generation_column,
            &out_index->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_index_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_index_column_descriptor *out_index_column
) {
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_index_column_select_index_column_id_column,
        &out_index_column->index_column_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_column_select_index_id_column,
            &out_index_column->index_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_column_select_table_id_column,
            &out_index_column->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_column_select_column_id_column,
            &out_index_column->column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_column_select_ordinal_position_column,
            &out_index_column->ordinal_position
        );
    }
    if (rc == MYLITE_OK &&
        sqlite3_column_type(statement, catalog_index_column_select_prefix_length_column) ==
            SQLITE_NULL) {
        out_index_column->has_prefix_length = false;
        out_index_column->prefix_length = 0;
    } else if (rc == MYLITE_OK) {
        out_index_column->has_prefix_length = true;
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_column_select_prefix_length_column,
            &out_index_column->prefix_length
        );
    }
    if (rc == MYLITE_OK) {
        int64_t sort_direction = 0;

        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_column_select_sort_direction_column,
            &sort_direction
        );
        if (rc == MYLITE_OK && (sort_direction == MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC ||
                                sort_direction == MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC)) {
            out_index_column->sort_direction =
                (enum mylite_catalog_index_sort_direction)sort_direction;
        } else if (rc == MYLITE_OK) {
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_index_column_select_descriptor_version_column,
            &out_index_column->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_index_column_select_created_generation_column,
            &out_index_column->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_index_column_select_updated_generation_column,
            &out_index_column->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_foreign_key(
    sqlite3_stmt *statement,
    struct mylite_catalog_foreign_key_descriptor *out_foreign_key
) {
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_foreign_key_select_foreign_key_id_column,
        &out_foreign_key->foreign_key_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_select_child_table_id_column,
            &out_foreign_key->child_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_select_parent_table_id_column,
            &out_foreign_key->parent_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_foreign_key_select_name_column,
            out_foreign_key->name,
            sizeof(out_foreign_key->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_select_parent_index_id_column,
            &out_foreign_key->parent_index_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_select_child_index_id_column,
            &out_foreign_key->child_index_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_foreign_key_select_update_rule_column,
            out_foreign_key->update_rule,
            sizeof(out_foreign_key->update_rule)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_foreign_key_select_delete_rule_column,
            out_foreign_key->delete_rule,
            sizeof(out_foreign_key->delete_rule)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_foreign_key_select_match_option_column,
            out_foreign_key->match_option,
            sizeof(out_foreign_key->match_option)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_foreign_key_select_descriptor_version_column,
            &out_foreign_key->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_foreign_key_select_created_generation_column,
            &out_foreign_key->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_foreign_key_select_updated_generation_column,
            &out_foreign_key->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_foreign_key_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_foreign_key_column_descriptor *out_foreign_key_column
) {
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_foreign_key_column_select_foreign_key_column_id_column,
        &out_foreign_key_column->foreign_key_column_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_foreign_key_id_column,
            &out_foreign_key_column->foreign_key_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_child_table_id_column,
            &out_foreign_key_column->child_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_parent_table_id_column,
            &out_foreign_key_column->parent_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_child_column_id_column,
            &out_foreign_key_column->child_column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_parent_column_id_column,
            &out_foreign_key_column->parent_column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_ordinal_position_column,
            &out_foreign_key_column->ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_position_in_unique_constraint_column,
            &out_foreign_key_column->position_in_unique_constraint
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_foreign_key_column_select_descriptor_version_column,
            &out_foreign_key_column->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_foreign_key_column_select_created_generation_column,
            &out_foreign_key_column->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_foreign_key_column_select_updated_generation_column,
            &out_foreign_key_column->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_check_constraint(
    sqlite3_stmt *statement,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint
) {
    int64_t is_enforced = 0;
    int64_t name_is_generated = 0;
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_check_constraint_select_check_constraint_id_column,
        &out_check_constraint->check_constraint_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_check_constraint_select_table_id_column,
            &out_check_constraint->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_check_constraint_select_name_column,
            out_check_constraint->name,
            sizeof(out_check_constraint->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_check_constraint_select_physical_name_column,
            out_check_constraint->physical_name,
            sizeof(out_check_constraint->physical_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_check_constraint_select_check_clause_column,
            out_check_constraint->check_clause,
            sizeof(out_check_constraint->check_clause)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_check_constraint_select_sqlite_expression_column,
            out_check_constraint->sqlite_expression,
            sizeof(out_check_constraint->sqlite_expression)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_check_constraint_select_is_enforced_column,
            &is_enforced
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(is_enforced, &out_check_constraint->is_enforced);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_check_constraint_select_name_is_generated_column,
            &name_is_generated
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(
            name_is_generated,
            &out_check_constraint->name_is_generated
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_check_constraint_select_generated_ordinal_column,
            &out_check_constraint->generated_ordinal
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_check_constraint_select_ordinal_position_column,
            &out_check_constraint->ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_check_constraint_select_descriptor_version_column,
            &out_check_constraint->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_check_constraint_select_created_generation_column,
            &out_check_constraint->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_check_constraint_select_updated_generation_column,
            &out_check_constraint->updated_catalog_generation
        );
    }

    return rc;
}
