#include "mylite_catalog.h"

#include "mylite_catalog_schema.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "sqlite3.h"

static const char table_catalog_name[] = "__mylite_table_catalog";
static const char column_catalog_name[] = "__mylite_column_catalog";
static const char index_catalog_name[] = "__mylite_index_catalog";
static const char temporary_table_catalog_name[] = "__mylite_temp_table_catalog";
static const char temporary_column_catalog_name[] = "__mylite_temp_column_catalog";
static const char temporary_index_catalog_name[] = "__mylite_temp_index_catalog";
static const char schema_catalog_sql[] = "CREATE TABLE IF NOT EXISTS __mylite_schema_catalog("
                                         "name TEXT PRIMARY KEY COLLATE BINARY,"
                                         "default_character_set TEXT NOT NULL,"
                                         "default_collation TEXT NOT NULL,"
                                         "default_encryption TEXT NOT NULL,"
                                         "read_only INTEGER NOT NULL,"
                                         "is_system INTEGER NOT NULL)";

static const char table_catalog_sql[] = "CREATE TABLE IF NOT EXISTS __mylite_table_catalog("
                                        "table_catalog TEXT NOT NULL,"
                                        "table_schema TEXT NOT NULL,"
                                        "table_name TEXT NOT NULL,"
                                        "table_type TEXT NOT NULL,"
                                        "engine TEXT,"
                                        "version INTEGER,"
                                        "row_format TEXT,"
                                        "table_rows INTEGER,"
                                        "avg_row_length INTEGER,"
                                        "data_length INTEGER,"
                                        "max_data_length INTEGER,"
                                        "index_length INTEGER,"
                                        "data_free INTEGER,"
                                        "auto_increment INTEGER,"
                                        "create_time TEXT NOT NULL,"
                                        "update_time TEXT,"
                                        "check_time TEXT,"
                                        "table_collation TEXT,"
                                        "checksum INTEGER,"
                                        "create_options TEXT,"
                                        "table_comment TEXT,"
                                        "PRIMARY KEY(table_schema, table_name))";

static const char column_catalog_sql[] = "CREATE TABLE IF NOT EXISTS __mylite_column_catalog("
                                         "table_catalog TEXT NOT NULL,"
                                         "table_schema TEXT NOT NULL,"
                                         "table_name TEXT NOT NULL,"
                                         "column_name TEXT,"
                                         "ordinal_position INTEGER NOT NULL,"
                                         "column_default TEXT,"
                                         "is_nullable TEXT NOT NULL,"
                                         "data_type TEXT,"
                                         "character_maximum_length INTEGER,"
                                         "character_octet_length INTEGER,"
                                         "numeric_precision INTEGER,"
                                         "numeric_scale INTEGER,"
                                         "datetime_precision INTEGER,"
                                         "character_set_name TEXT,"
                                         "collation_name TEXT,"
                                         "column_type TEXT NOT NULL,"
                                         "column_key TEXT NOT NULL,"
                                         "extra TEXT,"
                                         "privileges TEXT,"
                                         "column_comment TEXT NOT NULL,"
                                         "generation_expression TEXT NOT NULL,"
                                         "srs_id INTEGER,"
                                         "PRIMARY KEY(table_schema, table_name, ordinal_position))";

static const char index_catalog_sql[] =
    "CREATE TABLE IF NOT EXISTS __mylite_index_catalog("
    "table_catalog TEXT NOT NULL,"
    "table_schema TEXT NOT NULL,"
    "table_name TEXT NOT NULL,"
    "non_unique INTEGER NOT NULL,"
    "index_schema TEXT NOT NULL,"
    "index_name TEXT,"
    "seq_in_index INTEGER NOT NULL,"
    "column_name TEXT,"
    "collation TEXT,"
    "cardinality INTEGER,"
    "sub_part INTEGER,"
    "packed TEXT,"
    "nullable TEXT NOT NULL,"
    "index_type TEXT NOT NULL,"
    "comment TEXT NOT NULL,"
    "index_comment TEXT NOT NULL,"
    "is_visible TEXT NOT NULL,"
    "expression TEXT,"
    "PRIMARY KEY(table_schema, table_name, index_name, seq_in_index))";

static const char temporary_table_catalog_sql[] =
    "CREATE TEMPORARY TABLE IF NOT EXISTS __mylite_temp_table_catalog("
    "table_catalog TEXT NOT NULL,"
    "table_schema TEXT NOT NULL,"
    "table_name TEXT NOT NULL,"
    "table_type TEXT NOT NULL,"
    "engine TEXT,"
    "version INTEGER,"
    "row_format TEXT,"
    "table_rows INTEGER,"
    "avg_row_length INTEGER,"
    "data_length INTEGER,"
    "max_data_length INTEGER,"
    "index_length INTEGER,"
    "data_free INTEGER,"
    "auto_increment INTEGER,"
    "create_time TEXT NOT NULL,"
    "update_time TEXT,"
    "check_time TEXT,"
    "table_collation TEXT,"
    "checksum INTEGER,"
    "create_options TEXT,"
    "table_comment TEXT,"
    "PRIMARY KEY(table_schema, table_name))";

static const char temporary_column_catalog_sql[] =
    "CREATE TEMPORARY TABLE IF NOT EXISTS __mylite_temp_column_catalog("
    "table_catalog TEXT NOT NULL,"
    "table_schema TEXT NOT NULL,"
    "table_name TEXT NOT NULL,"
    "column_name TEXT,"
    "ordinal_position INTEGER NOT NULL,"
    "column_default TEXT,"
    "is_nullable TEXT NOT NULL,"
    "data_type TEXT,"
    "character_maximum_length INTEGER,"
    "character_octet_length INTEGER,"
    "numeric_precision INTEGER,"
    "numeric_scale INTEGER,"
    "datetime_precision INTEGER,"
    "character_set_name TEXT,"
    "collation_name TEXT,"
    "column_type TEXT NOT NULL,"
    "column_key TEXT NOT NULL,"
    "extra TEXT,"
    "privileges TEXT,"
    "column_comment TEXT NOT NULL,"
    "generation_expression TEXT NOT NULL,"
    "srs_id INTEGER,"
    "PRIMARY KEY(table_schema, table_name, ordinal_position))";

static const char temporary_index_catalog_sql[] =
    "CREATE TEMPORARY TABLE IF NOT EXISTS __mylite_temp_index_catalog("
    "table_catalog TEXT NOT NULL,"
    "table_schema TEXT NOT NULL,"
    "table_name TEXT NOT NULL,"
    "non_unique INTEGER NOT NULL,"
    "index_schema TEXT NOT NULL,"
    "index_name TEXT,"
    "seq_in_index INTEGER NOT NULL,"
    "column_name TEXT,"
    "collation TEXT,"
    "cardinality INTEGER,"
    "sub_part INTEGER,"
    "packed TEXT,"
    "nullable TEXT NOT NULL,"
    "index_type TEXT NOT NULL,"
    "comment TEXT NOT NULL,"
    "index_comment TEXT NOT NULL,"
    "is_visible TEXT NOT NULL,"
    "expression TEXT,"
    "PRIMARY KEY(table_schema, table_name, index_name, seq_in_index))";

struct mylite_catalog_table_key {
    const char *schema_name;
    const char *table_name;
};

struct mylite_resolved_table_catalog {
    const char *catalog_name;
    bool temporary;
    bool exists;
};

static int delete_table_catalog_row(
    mylite_db *database,
    const char *sql,
    const struct mylite_catalog_table_key *key
);

static int catalog_table_exists_in(
    mylite_db *database,
    const char *catalog_name,
    const struct mylite_catalog_table_key *key,
    bool *out_exists
);

static int load_table_metadata_from_catalog(
    mylite_db *database,
    const char *catalog_name,
    const struct mylite_catalog_table_key *key,
    struct mylite_catalog_table_metadata *out_metadata,
    bool *out_found
);

static int resolve_table_catalog_name(
    mylite_db *database,
    const struct mylite_catalog_table_key *key,
    struct mylite_resolved_table_catalog *out_catalog
);

static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_catalog_initialize(mylite_db *database) {
    int rc = sqlite3_exec(database->sqlite, schema_catalog_sql, NULL, NULL, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    rc = sqlite3_exec(database->sqlite, table_catalog_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    rc = sqlite3_exec(database->sqlite, column_catalog_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    rc = sqlite3_exec(database->sqlite, index_catalog_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    rc = sqlite3_exec(database->sqlite, temporary_table_catalog_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    rc = sqlite3_exec(database->sqlite, temporary_column_catalog_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    rc = sqlite3_exec(database->sqlite, temporary_index_catalog_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    rc = mylite_catalog_seed_system_schema(
        database,
        "information_schema",
        "utf8mb3",
        "utf8mb3_general_ci"
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_seed_system_schema(database, "mysql", "utf8mb4", "utf8mb4_0900_ai_ci");
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_seed_system_schema(
        database,
        "performance_schema",
        "utf8mb4",
        "utf8mb4_0900_ai_ci"
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    return mylite_catalog_seed_system_schema(database, "sys", "utf8mb4", "utf8mb4_0900_ai_ci");
}

int mylite_catalog_update_auto_increment(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    uint64_t next_auto_increment
) {
    sqlite3_stmt *update = NULL;
    const struct mylite_catalog_table_key key = {
        .schema_name = schema_name,
        .table_name = table_name,
    };
    struct mylite_resolved_table_catalog catalog = {0};
    char *sql = NULL;
    int rc = SQLITE_OK;
    int status = resolve_table_catalog_name(database, &key, &catalog);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!catalog.exists) {
        return mylite_diagnostics_set_table_doesnt_exist_error(database, schema_name, table_name);
    }

    sql = sqlite3_mprintf(
        "UPDATE %s SET auto_increment = ? "
        "WHERE table_schema = ? AND table_name = ?",
        catalog.catalog_name
    );
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);
    sqlite3_free(sql);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_int64(update, 1, (sqlite3_int64)next_auto_increment);
    sqlite3_bind_text(update, 2, schema_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(update, 3, table_name, -1, SQLITE_STATIC);
    rc = sqlite3_step(update);
    sqlite3_finalize(update);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

int mylite_catalog_delete_table_rows(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    unsigned int flags
) {
    static const char delete_indexes[] =
        "DELETE FROM __mylite_index_catalog WHERE table_schema = ? AND table_name = ?";
    static const char delete_columns[] =
        "DELETE FROM __mylite_column_catalog WHERE table_schema = ? AND table_name = ?";
    static const char delete_tables[] =
        "DELETE FROM __mylite_table_catalog WHERE table_schema = ? AND table_name = ?";
    const struct mylite_catalog_table_key key = {
        .schema_name = schema_name,
        .table_name = table_name,
    };
    int status = MYLITE_OK;

    if ((flags & MYLITE_CATALOG_DELETE_TABLE_INDEXES) != 0U) {
        status = delete_table_catalog_row(database, delete_indexes, &key);
    }
    if (status == MYLITE_OK && (flags & MYLITE_CATALOG_DELETE_TABLE_COLUMNS) != 0U) {
        status = delete_table_catalog_row(database, delete_columns, &key);
    }
    if (status == MYLITE_OK && (flags & MYLITE_CATALOG_DELETE_TABLE_ROW) != 0U) {
        status = delete_table_catalog_row(database, delete_tables, &key);
    }
    return status;
}

int mylite_catalog_delete_temporary_table_rows(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    unsigned int flags
) {
    static const char delete_indexes[] =
        "DELETE FROM __mylite_temp_index_catalog WHERE table_schema = ? AND table_name = ?";
    static const char delete_columns[] =
        "DELETE FROM __mylite_temp_column_catalog WHERE table_schema = ? AND table_name = ?";
    static const char delete_tables[] =
        "DELETE FROM __mylite_temp_table_catalog WHERE table_schema = ? AND table_name = ?";
    const struct mylite_catalog_table_key key = {
        .schema_name = schema_name,
        .table_name = table_name,
    };
    int status = MYLITE_OK;

    if ((flags & MYLITE_CATALOG_DELETE_TABLE_INDEXES) != 0U) {
        status = delete_table_catalog_row(database, delete_indexes, &key);
    }
    if (status == MYLITE_OK && (flags & MYLITE_CATALOG_DELETE_TABLE_COLUMNS) != 0U) {
        status = delete_table_catalog_row(database, delete_columns, &key);
    }
    if (status == MYLITE_OK && (flags & MYLITE_CATALOG_DELETE_TABLE_ROW) != 0U) {
        status = delete_table_catalog_row(database, delete_tables, &key);
    }
    return status;
}

static int delete_table_catalog_row(
    mylite_db *database,
    const char *sql,
    const struct mylite_catalog_table_key *key
) {
    sqlite3_stmt *delete_stmt = NULL;
    int rc = sqlite3_prepare_v3(
        database->sqlite,
        sql,
        -1,
        SQLITE_PREPARE_PERSISTENT,
        &delete_stmt,
        NULL
    );

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(delete_stmt, 1, key->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(delete_stmt, 2, key->table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(delete_stmt);
    sqlite3_finalize(delete_stmt);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

int mylite_catalog_table_exists(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    bool *out_exists
) {
    int status =
        mylite_catalog_temporary_table_exists(database, schema_name, table_name, out_exists);

    if (status != MYLITE_OK || *out_exists) {
        return status;
    }
    return mylite_catalog_persistent_table_exists(database, schema_name, table_name, out_exists);
}

int mylite_catalog_persistent_table_exists(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    bool *out_exists
) {
    const struct mylite_catalog_table_key key = {
        .schema_name = schema_name,
        .table_name = table_name,
    };

    return catalog_table_exists_in(database, table_catalog_name, &key, out_exists);
}

int mylite_catalog_temporary_table_exists(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    bool *out_exists
) {
    const struct mylite_catalog_table_key key = {
        .schema_name = schema_name,
        .table_name = table_name,
    };

    return catalog_table_exists_in(database, temporary_table_catalog_name, &key, out_exists);
}

int mylite_catalog_load_table_metadata(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    struct mylite_catalog_table_metadata *out_metadata
) {
    const struct mylite_catalog_table_key key = {
        .schema_name = schema_name,
        .table_name = table_name,
    };
    bool found = false;
    int status = MYLITE_OK;

    if (out_metadata == NULL) {
        return MYLITE_MISUSE;
    }
    *out_metadata = (struct mylite_catalog_table_metadata){0};
    status = load_table_metadata_from_catalog(
        database,
        temporary_table_catalog_name,
        &key,
        out_metadata,
        &found
    );
    if (status != MYLITE_OK || found) {
        return status;
    }
    status =
        load_table_metadata_from_catalog(database, table_catalog_name, &key, out_metadata, &found);
    if (status != MYLITE_OK) {
        return status;
    }
    if (found) {
        return MYLITE_OK;
    }
    return mylite_diagnostics_set_table_doesnt_exist_error(database, schema_name, table_name);
}

int mylite_catalog_load_table_columns(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    mylite_catalog_column_callback callback,
    void *context
) {
    sqlite3_stmt *stmt = NULL;
    const struct mylite_catalog_table_key key = {
        .schema_name = schema_name,
        .table_name = table_name,
    };
    struct mylite_resolved_table_catalog catalog = {0};
    char *sql = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (callback == NULL) {
        return MYLITE_MISUSE;
    }
    status = resolve_table_catalog_name(database, &key, &catalog);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!catalog.exists) {
        return MYLITE_OK;
    }
    sql = sqlite3_mprintf(
        "SELECT column_name, column_default, is_nullable, data_type, column_type, "
        "character_maximum_length, numeric_precision, numeric_scale, datetime_precision, extra "
        "FROM %s WHERE table_schema = ? AND table_name = ? "
        "ORDER BY ordinal_position",
        mylite_catalog_column_catalog_name(catalog.temporary)
    );
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(stmt, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(stmt, 2, table_name, -1, sqlite_transient_destructor());
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const struct mylite_catalog_column_row row = {
            .name = (const char *)sqlite3_column_text(stmt, 0),
            .default_text = (const char *)sqlite3_column_text(stmt, 1),
            .is_nullable = (const char *)sqlite3_column_text(stmt, 2),
            .data_type = (const char *)sqlite3_column_text(stmt, 3),
            .column_type = (const char *)sqlite3_column_text(stmt, 4),
            .character_maximum_length = (uint64_t)sqlite3_column_int64(stmt, 5),
            .has_character_maximum_length = sqlite3_column_type(stmt, 5) != SQLITE_NULL,
            .numeric_precision = (uint64_t)sqlite3_column_int64(stmt, 6),
            .has_numeric_precision = sqlite3_column_type(stmt, 6) != SQLITE_NULL,
            .numeric_scale = (uint64_t)sqlite3_column_int64(stmt, 7),
            .has_numeric_scale = sqlite3_column_type(stmt, 7) != SQLITE_NULL,
            .datetime_precision = (uint64_t)sqlite3_column_int64(stmt, 8),
            .has_datetime_precision = sqlite3_column_type(stmt, 8) != SQLITE_NULL,
            .extra = (const char *)sqlite3_column_text(stmt, 9),
        };
        int callback_status = callback(context, &row);

        if (callback_status != MYLITE_OK) {
            sqlite3_finalize(stmt);
            return callback_status;
        }
    }
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

int mylite_catalog_load_unique_index_parts(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    mylite_catalog_unique_index_part_callback callback,
    void *context
) {
    sqlite3_stmt *stmt = NULL;
    const struct mylite_catalog_table_key key = {
        .schema_name = schema_name,
        .table_name = table_name,
    };
    struct mylite_resolved_table_catalog catalog = {0};
    char *sql = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (callback == NULL) {
        return MYLITE_MISUSE;
    }
    status = resolve_table_catalog_name(database, &key, &catalog);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!catalog.exists) {
        return MYLITE_OK;
    }
    sql = sqlite3_mprintf(
        "SELECT index_name, column_name, sub_part FROM %s "
        "WHERE table_schema = ? AND table_name = ? AND non_unique = 0 "
        "ORDER BY rowid",
        mylite_catalog_index_catalog_name(catalog.temporary)
    );
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(stmt, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(stmt, 2, table_name, -1, sqlite_transient_destructor());
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const struct mylite_catalog_unique_index_part_row row = {
            .index_name = (const char *)sqlite3_column_text(stmt, 0),
            .column_name = (const char *)sqlite3_column_text(stmt, 1),
            .prefix_length = (uint64_t)sqlite3_column_int64(stmt, 2),
            .has_prefix_length = sqlite3_column_type(stmt, 2) != SQLITE_NULL,
        };
        int callback_status = callback(context, &row);

        if (callback_status != MYLITE_OK) {
            sqlite3_finalize(stmt);
            return callback_status;
        }
    }
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

const char *mylite_catalog_table_catalog_name(bool temporary) {
    if (temporary) {
        return temporary_table_catalog_name;
    }
    return table_catalog_name;
}

const char *mylite_catalog_column_catalog_name(bool temporary) {
    if (temporary) {
        return temporary_column_catalog_name;
    }
    return column_catalog_name;
}

const char *mylite_catalog_index_catalog_name(bool temporary) {
    if (temporary) {
        return temporary_index_catalog_name;
    }
    return index_catalog_name;
}

static int catalog_table_exists_in(
    mylite_db *database,
    const char *catalog_name,
    const struct mylite_catalog_table_key *key,
    bool *out_exists
) {
    sqlite3_stmt *stmt = NULL;
    char *sql =
        sqlite3_mprintf("SELECT 1 FROM %s WHERE table_schema = ? AND table_name = ?", catalog_name);
    int rc = SQLITE_OK;

    if (out_exists == NULL) {
        return MYLITE_MISUSE;
    }
    *out_exists = false;
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(stmt, 1, key->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(stmt, 2, key->table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        *out_exists = true;
        sqlite3_finalize(stmt);
        return MYLITE_OK;
    }

    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int load_table_metadata_from_catalog(
    mylite_db *database,
    const char *catalog_name,
    const struct mylite_catalog_table_key *key,
    struct mylite_catalog_table_metadata *out_metadata,
    bool *out_found
) {
    sqlite3_stmt *stmt = NULL;
    char *sql = sqlite3_mprintf(
        "SELECT auto_increment FROM %s WHERE table_schema = ? "
        "AND table_name = ?",
        catalog_name
    );
    int rc = SQLITE_OK;

    *out_found = false;
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(stmt, 1, key->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(stmt, 2, key->table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        *out_found = true;
        if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
            sqlite3_int64 value = sqlite3_column_int64(stmt, 0);

            if (value > 0) {
                out_metadata->auto_increment = (uint64_t)value;
                out_metadata->has_auto_increment = true;
            }
        }
    }

    sqlite3_finalize(stmt);
    return (rc == SQLITE_ROW || rc == SQLITE_DONE) ? MYLITE_OK
                                                   : mylite_diagnostics_set_sqlite_error(database);
}

static int resolve_table_catalog_name(
    mylite_db *database,
    const struct mylite_catalog_table_key *key,
    struct mylite_resolved_table_catalog *out_catalog
) {
    int status = MYLITE_OK;

    *out_catalog = (struct mylite_resolved_table_catalog){0};
    status = mylite_catalog_temporary_table_exists(
        database,
        key->schema_name,
        key->table_name,
        &out_catalog->exists
    );

    if (status != MYLITE_OK) {
        return status;
    }
    if (out_catalog->exists) {
        out_catalog->catalog_name = temporary_table_catalog_name;
        out_catalog->temporary = true;
        return MYLITE_OK;
    }

    status = mylite_catalog_persistent_table_exists(
        database,
        key->schema_name,
        key->table_name,
        &out_catalog->exists
    );
    if (status == MYLITE_OK && out_catalog->exists) {
        out_catalog->catalog_name = table_catalog_name;
    }
    return status;
}

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
