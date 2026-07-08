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

enum catalog_table_kind_lookup_column_index {
    catalog_table_kind_lookup_kind_column = 0,
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
static int try_read_table_kind_by_schema_table_name(
    sqlite3 *sqlite,
    const char *schema_name,
    const char *table_name,
    enum mylite_catalog_table_kind *out_kind,
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

int mylite_catalog_try_read_table_kind_by_schema_table_name(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name,
    enum mylite_catalog_table_kind *out_kind,
    bool *out_found
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_kind == NULL || out_found == NULL) {
        return MYLITE_MISUSE;
    }
    *out_kind = MYLITE_CATALOG_TABLE_KIND_INVALID;
    *out_found = false;
    rc = mylite_catalog_validate_required_name(schema_name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(table_name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return try_read_table_kind_by_schema_table_name(
        database->sqlite,
        schema_name,
        table_name,
        out_kind,
        out_found
    );
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

static int try_read_table_kind_by_schema_table_name(
    sqlite3 *sqlite,
    const char *schema_name,
    const char *table_name,
    enum mylite_catalog_table_kind *out_kind,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT t.kind "
        "FROM _mylite_catalog_tables AS t "
        "JOIN _mylite_catalog_schemas AS s ON s.schema_id = t.schema_id "
        "WHERE s.name = ?1 AND t.name = ?2",
        &statement
    );

    *out_kind = MYLITE_CATALOG_TABLE_KIND_INVALID;
    *out_found = false;
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, schema_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, table_name);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            int64_t kind = 0;

            rc = mylite_catalog_checked_column_i64(
                statement,
                catalog_table_kind_lookup_kind_column,
                &kind
            );
            if (rc == MYLITE_OK) {
                rc = mylite_catalog_validate_table_kind((enum mylite_catalog_table_kind)kind);
            }
            if (rc == MYLITE_OK) {
                *out_kind = (enum mylite_catalog_table_kind)kind;
                *out_found = true;
            }
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
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
