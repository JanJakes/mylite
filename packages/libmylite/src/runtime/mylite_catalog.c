#include "mylite_catalog.h"

#include "mylite_catalog_schema.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <string.h>

static const char table_catalog_name[] = "__mylite_table_catalog";
static const char column_catalog_name[] = "__mylite_column_catalog";
static const char index_catalog_name[] = "__mylite_index_catalog";
static const char check_constraint_catalog_name[] = "__mylite_check_constraint_catalog";
static const char foreign_key_catalog_name[] = "__mylite_foreign_key_catalog";
static const char temporary_table_catalog_name[] = "__mylite_temp_table_catalog";
static const char temporary_column_catalog_name[] = "__mylite_temp_column_catalog";
static const char temporary_index_catalog_name[] = "__mylite_temp_index_catalog";
static const char temporary_check_constraint_catalog_name[] =
    "__mylite_temp_check_constraint_catalog";
static const char temporary_foreign_key_catalog_name[] = "__mylite_temp_foreign_key_catalog";

enum { mylite_catalog_innodb_page_bytes = 16384 };

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
    "display_index_type INTEGER NOT NULL DEFAULT 0,"
    "comment TEXT NOT NULL,"
    "index_comment TEXT NOT NULL,"
    "is_visible TEXT NOT NULL,"
    "expression TEXT,"
    "PRIMARY KEY(table_schema, table_name, index_name, seq_in_index))";

static const char check_constraint_catalog_sql[] =
    "CREATE TABLE IF NOT EXISTS __mylite_check_constraint_catalog("
    "constraint_catalog TEXT NOT NULL,"
    "constraint_schema TEXT NOT NULL,"
    "constraint_name TEXT NOT NULL,"
    "table_schema TEXT NOT NULL,"
    "table_name TEXT NOT NULL,"
    "check_clause TEXT NOT NULL,"
    "enforced TEXT NOT NULL,"
    "ordinal_position INTEGER NOT NULL,"
    "PRIMARY KEY(constraint_schema, table_name, constraint_name))";

static const char foreign_key_catalog_sql[] =
    "CREATE TABLE IF NOT EXISTS __mylite_foreign_key_catalog("
    "constraint_catalog TEXT NOT NULL,"
    "constraint_schema TEXT NOT NULL,"
    "constraint_name TEXT NOT NULL,"
    "table_schema TEXT NOT NULL,"
    "table_name TEXT NOT NULL,"
    "column_name TEXT NOT NULL,"
    "ordinal_position INTEGER NOT NULL,"
    "supporting_index_name TEXT NOT NULL,"
    "unique_constraint_catalog TEXT NOT NULL,"
    "unique_constraint_schema TEXT NOT NULL,"
    "unique_constraint_name TEXT,"
    "match_option TEXT NOT NULL,"
    "update_rule TEXT NOT NULL,"
    "delete_rule TEXT NOT NULL,"
    "referenced_table_schema TEXT NOT NULL,"
    "referenced_table_name TEXT NOT NULL,"
    "referenced_column_name TEXT NOT NULL,"
    "position_in_unique_constraint INTEGER NOT NULL,"
    "PRIMARY KEY(constraint_schema, table_name, constraint_name, ordinal_position))";

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
    "display_index_type INTEGER NOT NULL DEFAULT 0,"
    "comment TEXT NOT NULL,"
    "index_comment TEXT NOT NULL,"
    "is_visible TEXT NOT NULL,"
    "expression TEXT,"
    "PRIMARY KEY(table_schema, table_name, index_name, seq_in_index))";

static const char temporary_check_constraint_catalog_sql[] =
    "CREATE TEMPORARY TABLE IF NOT EXISTS __mylite_temp_check_constraint_catalog("
    "constraint_catalog TEXT NOT NULL,"
    "constraint_schema TEXT NOT NULL,"
    "constraint_name TEXT NOT NULL,"
    "table_schema TEXT NOT NULL,"
    "table_name TEXT NOT NULL,"
    "check_clause TEXT NOT NULL,"
    "enforced TEXT NOT NULL,"
    "ordinal_position INTEGER NOT NULL,"
    "PRIMARY KEY(constraint_schema, table_name, constraint_name))";

static const char temporary_foreign_key_catalog_sql[] =
    "CREATE TEMPORARY TABLE IF NOT EXISTS __mylite_temp_foreign_key_catalog("
    "constraint_catalog TEXT NOT NULL,"
    "constraint_schema TEXT NOT NULL,"
    "constraint_name TEXT NOT NULL,"
    "table_schema TEXT NOT NULL,"
    "table_name TEXT NOT NULL,"
    "column_name TEXT NOT NULL,"
    "ordinal_position INTEGER NOT NULL,"
    "supporting_index_name TEXT NOT NULL,"
    "unique_constraint_catalog TEXT NOT NULL,"
    "unique_constraint_schema TEXT NOT NULL,"
    "unique_constraint_name TEXT,"
    "match_option TEXT NOT NULL,"
    "update_rule TEXT NOT NULL,"
    "delete_rule TEXT NOT NULL,"
    "referenced_table_schema TEXT NOT NULL,"
    "referenced_table_name TEXT NOT NULL,"
    "referenced_column_name TEXT NOT NULL,"
    "position_in_unique_constraint INTEGER NOT NULL,"
    "PRIMARY KEY(constraint_schema, table_name, constraint_name, ordinal_position))";

struct mylite_catalog_table_key {
    const char *schema_name;
    const char *table_name;
};

static int delete_table_catalog_row(
    mylite_db *database,
    const char *sql,
    const struct mylite_catalog_table_key *key
);

static int catalog_table_exists_in(
    mylite_db *database,
    const char *catalog_name,
    const char *schema_name,
    const char *table_name,
    bool *out_exists
);

static int load_table_metadata_from_catalog(
    mylite_db *database,
    const char *catalog_name,
    const char *schema_name,
    const char *table_name,
    struct mylite_catalog_table_metadata *out_metadata,
    bool *out_found
);

static int resolve_table_catalog_name(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char **out_catalog_name,
    bool *out_temporary,
    bool *out_exists
);

static int read_physical_table_row_count(
    mylite_db *database,
    const char *physical_name,
    sqlite3_int64 *out_row_count
);

static int read_secondary_index_count(
    mylite_db *database,
    const char *index_catalog,
    const char *schema_name,
    const char *table_name,
    sqlite3_int64 *out_index_count
);

static int update_table_statistics(
    mylite_db *database,
    const char *table_catalog,
    const char *schema_name,
    const char *table_name,
    sqlite3_int64 row_count,
    sqlite3_int64 secondary_index_count
);

static int ensure_index_catalog_display_type_column(mylite_db *database, const char *catalog_name);

static int catalog_column_exists(
    mylite_db *database,
    const char *catalog_name,
    const char *column_name,
    bool *out_exists
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
    rc = ensure_index_catalog_display_type_column(database, index_catalog_name);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = sqlite3_exec(database->sqlite, check_constraint_catalog_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    rc = sqlite3_exec(database->sqlite, foreign_key_catalog_sql, NULL, NULL, NULL);
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
    rc = ensure_index_catalog_display_type_column(database, temporary_index_catalog_name);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = sqlite3_exec(database->sqlite, temporary_check_constraint_catalog_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    rc = sqlite3_exec(database->sqlite, temporary_foreign_key_catalog_sql, NULL, NULL, NULL);
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

static int ensure_index_catalog_display_type_column(mylite_db *database, const char *catalog_name) {
    bool exists = false;
    char *sql = NULL;
    int rc = SQLITE_OK;
    int status = catalog_column_exists(database, catalog_name, "display_index_type", &exists);

    if (status != MYLITE_OK || exists) {
        return status;
    }

    sql = sqlite3_mprintf(
        "ALTER TABLE %s ADD COLUMN display_index_type INTEGER NOT NULL DEFAULT 0",
        catalog_name
    );
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int catalog_column_exists(
    mylite_db *database,
    const char *catalog_name,
    const char *column_name,
    bool *out_exists
) {
    sqlite3_stmt *select = NULL;
    char *sql = sqlite3_mprintf("PRAGMA table_info(%s)", catalog_name);
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    *out_exists = false;
    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        enum { table_info_name_column = 1 };

        const char *name = (const char *)sqlite3_column_text(select, table_info_name_column);

        if (name != NULL && strcmp(name, column_name) == 0) {
            *out_exists = true;
            break;
        }
    }
    sqlite3_finalize(select);
    return rc == SQLITE_DONE || *out_exists ? MYLITE_OK
                                            : mylite_diagnostics_set_sqlite_error(database);
}

int mylite_catalog_update_auto_increment(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    uint64_t next_auto_increment
) {
    sqlite3_stmt *update = NULL;
    const char *catalog_name = NULL;
    bool temporary = false;
    bool exists = false;
    char *sql = NULL;
    int rc = SQLITE_OK;
    int status = resolve_table_catalog_name(
        database,
        schema_name,
        table_name,
        &catalog_name,
        &temporary,
        &exists
    );

    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        return mylite_diagnostics_set_table_doesnt_exist_error(database, schema_name, table_name);
    }

    sql = sqlite3_mprintf(
        "UPDATE %s SET auto_increment = ? "
        "WHERE table_schema = ? AND table_name = ?",
        catalog_name
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

int mylite_catalog_refresh_table_statistics(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *physical_name
) {
    const char *table_catalog = NULL;
    bool temporary = false;
    bool exists = false;
    sqlite3_int64 row_count = 0;
    sqlite3_int64 secondary_index_count = 0;
    int status = MYLITE_OK;

    if (database == NULL || schema_name == NULL || table_name == NULL || physical_name == NULL) {
        return MYLITE_MISUSE;
    }

    status = resolve_table_catalog_name(
        database,
        schema_name,
        table_name,
        &table_catalog,
        &temporary,
        &exists
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        return mylite_diagnostics_set_table_doesnt_exist_error(database, schema_name, table_name);
    }

    status = read_physical_table_row_count(database, physical_name, &row_count);
    if (status == MYLITE_OK) {
        status = read_secondary_index_count(
            database,
            mylite_catalog_index_catalog_name(temporary),
            schema_name,
            table_name,
            &secondary_index_count
        );
    }
    if (status == MYLITE_OK) {
        status = update_table_statistics(
            database,
            table_catalog,
            schema_name,
            table_name,
            row_count,
            secondary_index_count
        );
    }
    return status;
}

int mylite_catalog_delete_table_rows(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    unsigned int flags
) {
    static const char delete_indexes[] =
        "DELETE FROM __mylite_index_catalog WHERE table_schema = ? AND table_name = ?";
    static const char delete_checks[] =
        "DELETE FROM __mylite_check_constraint_catalog WHERE table_schema = ? AND table_name = ?";
    static const char delete_foreign_keys[] =
        "DELETE FROM __mylite_foreign_key_catalog WHERE table_schema = ? AND table_name = ?";
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
    if (status == MYLITE_OK && (flags & MYLITE_CATALOG_DELETE_TABLE_CHECKS) != 0U) {
        status = delete_table_catalog_row(database, delete_checks, &key);
    }
    if (status == MYLITE_OK && (flags & MYLITE_CATALOG_DELETE_TABLE_FOREIGN_KEYS) != 0U) {
        status = delete_table_catalog_row(database, delete_foreign_keys, &key);
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
    static const char delete_checks[] = "DELETE FROM __mylite_temp_check_constraint_catalog "
                                        "WHERE table_schema = ? AND table_name = ?";
    static const char delete_foreign_keys[] = "DELETE FROM __mylite_temp_foreign_key_catalog "
                                              "WHERE table_schema = ? AND table_name = ?";
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
    if (status == MYLITE_OK && (flags & MYLITE_CATALOG_DELETE_TABLE_CHECKS) != 0U) {
        status = delete_table_catalog_row(database, delete_checks, &key);
    }
    if (status == MYLITE_OK && (flags & MYLITE_CATALOG_DELETE_TABLE_FOREIGN_KEYS) != 0U) {
        status = delete_table_catalog_row(database, delete_foreign_keys, &key);
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
    return catalog_table_exists_in(
        database,
        table_catalog_name,
        schema_name,
        table_name,
        out_exists
    );
}

int mylite_catalog_temporary_table_exists(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    bool *out_exists
) {
    return catalog_table_exists_in(
        database,
        temporary_table_catalog_name,
        schema_name,
        table_name,
        out_exists
    );
}

int mylite_catalog_load_table_metadata(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    struct mylite_catalog_table_metadata *out_metadata
) {
    bool found = false;
    int status = MYLITE_OK;

    if (out_metadata == NULL) {
        return MYLITE_MISUSE;
    }
    *out_metadata = (struct mylite_catalog_table_metadata){0};
    status = load_table_metadata_from_catalog(
        database,
        temporary_table_catalog_name,
        schema_name,
        table_name,
        out_metadata,
        &found
    );
    if (status != MYLITE_OK || found) {
        return status;
    }
    status = load_table_metadata_from_catalog(
        database,
        table_catalog_name,
        schema_name,
        table_name,
        out_metadata,
        &found
    );
    if (status != MYLITE_OK) {
        return status;
    }
    return found
               ? MYLITE_OK
               : mylite_diagnostics_set_table_doesnt_exist_error(database, schema_name, table_name);
}

int mylite_catalog_load_table_columns(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    mylite_catalog_column_callback callback,
    void *context
) {
    sqlite3_stmt *stmt = NULL;
    const char *catalog_name = NULL;
    bool temporary = false;
    bool exists = false;
    char *sql = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (callback == NULL) {
        return MYLITE_MISUSE;
    }
    status = resolve_table_catalog_name(
        database,
        schema_name,
        table_name,
        &catalog_name,
        &temporary,
        &exists
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        return MYLITE_OK;
    }
    sql = sqlite3_mprintf(
        "SELECT column_name, column_default, is_nullable, data_type, extra "
        "FROM %s WHERE table_schema = ? AND table_name = ? "
        "ORDER BY ordinal_position",
        mylite_catalog_column_catalog_name(temporary)
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
            .extra = (const char *)sqlite3_column_text(stmt, 4),
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
    const char *catalog_name = NULL;
    bool temporary = false;
    bool exists = false;
    char *sql = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (callback == NULL) {
        return MYLITE_MISUSE;
    }
    status = resolve_table_catalog_name(
        database,
        schema_name,
        table_name,
        &catalog_name,
        &temporary,
        &exists
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        return MYLITE_OK;
    }
    sql = sqlite3_mprintf(
        "SELECT index_name, column_name, sub_part FROM %s "
        "WHERE table_schema = ? AND table_name = ? AND non_unique = 0 "
        "ORDER BY rowid",
        mylite_catalog_index_catalog_name(temporary)
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
    return temporary ? temporary_table_catalog_name : table_catalog_name;
}

const char *mylite_catalog_column_catalog_name(bool temporary) {
    return temporary ? temporary_column_catalog_name : column_catalog_name;
}

const char *mylite_catalog_index_catalog_name(bool temporary) {
    return temporary ? temporary_index_catalog_name : index_catalog_name;
}

const char *mylite_catalog_check_constraint_catalog_name(bool temporary) {
    return temporary ? temporary_check_constraint_catalog_name : check_constraint_catalog_name;
}

const char *mylite_catalog_foreign_key_catalog_name(bool temporary) {
    return temporary ? temporary_foreign_key_catalog_name : foreign_key_catalog_name;
}

static int catalog_table_exists_in(
    mylite_db *database,
    const char *catalog_name,
    const char *schema_name,
    const char *table_name,
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

    sqlite3_bind_text(stmt, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(stmt, 2, table_name, -1, sqlite_transient_destructor());
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
    const char *schema_name,
    const char *table_name,
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

    sqlite3_bind_text(stmt, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(stmt, 2, table_name, -1, sqlite_transient_destructor());
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

static int read_physical_table_row_count(
    mylite_db *database,
    const char *physical_name,
    sqlite3_int64 *out_row_count
) {
    sqlite3_stmt *stmt = NULL;
    char *sql = sqlite3_mprintf("SELECT COUNT(*) FROM \"%w\"", physical_name);
    int rc = SQLITE_OK;

    *out_row_count = 0;
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        *out_row_count = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return rc == SQLITE_ROW ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int read_secondary_index_count(
    mylite_db *database,
    const char *index_catalog,
    const char *schema_name,
    const char *table_name,
    sqlite3_int64 *out_index_count
) {
    sqlite3_stmt *stmt = NULL;
    char *sql = sqlite3_mprintf(
        "SELECT COUNT(*) FROM ("
        "SELECT index_name FROM %s "
        "WHERE table_schema = ? AND table_name = ? AND index_name <> 'PRIMARY' "
        "GROUP BY index_name)",
        index_catalog
    );
    int rc = SQLITE_OK;

    *out_index_count = 0;
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
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        *out_index_count = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return rc == SQLITE_ROW ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int update_table_statistics(
    mylite_db *database,
    const char *table_catalog,
    const char *schema_name,
    const char *table_name,
    sqlite3_int64 row_count,
    sqlite3_int64 secondary_index_count
) {
    enum {
        bind_rows = 1,
        bind_avg_row_length = 2,
        bind_data_length = 3,
        bind_index_length = 4,
        bind_schema = 5,
        bind_table = 6,
    };

    sqlite3_stmt *update = NULL;
    char *sql = sqlite3_mprintf(
        "UPDATE %s SET table_rows = ?, avg_row_length = ?, data_length = ?, "
        "max_data_length = 0, index_length = ?, data_free = 0 "
        "WHERE table_schema = ? AND table_name = ?",
        table_catalog
    );
    sqlite3_int64 data_length = mylite_catalog_innodb_page_bytes;
    sqlite3_int64 avg_row_length = row_count == 0 ? 0 : data_length / row_count;
    sqlite3_int64 index_length = secondary_index_count * mylite_catalog_innodb_page_bytes;
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_int64(update, bind_rows, row_count);
    sqlite3_bind_int64(update, bind_avg_row_length, avg_row_length);
    sqlite3_bind_int64(update, bind_data_length, data_length);
    sqlite3_bind_int64(update, bind_index_length, index_length);
    sqlite3_bind_text(update, bind_schema, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, bind_table, table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(update);
    sqlite3_finalize(update);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int resolve_table_catalog_name(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char **out_catalog_name,
    bool *out_temporary,
    bool *out_exists
) {
    int status =
        mylite_catalog_temporary_table_exists(database, schema_name, table_name, out_exists);

    *out_catalog_name = NULL;
    *out_temporary = false;
    if (status != MYLITE_OK) {
        return status;
    }
    if (*out_exists) {
        *out_catalog_name = temporary_table_catalog_name;
        *out_temporary = true;
        return MYLITE_OK;
    }

    status = mylite_catalog_persistent_table_exists(database, schema_name, table_name, out_exists);
    if (status == MYLITE_OK && *out_exists) {
        *out_catalog_name = table_catalog_name;
    }
    return status;
}

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
