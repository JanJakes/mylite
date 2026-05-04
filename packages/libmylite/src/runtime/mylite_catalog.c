#include "mylite_catalog.h"

#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "sqlite3.h"

#include <stdlib.h>
#include <string.h>

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

struct mylite_catalog_table_key {
    const char *schema_name;
    const char *table_name;
};

static int seed_system_schema(mylite_db *database, const char *name, const char *character_set,
                              const char *collation);
static int delete_table_catalog_row(mylite_db *database, const char *sql,
                                    const struct mylite_catalog_table_key *key);
static sqlite3_destructor_type sqlite_transient_destructor(void);
static bool hex_encoded_text_length(size_t text_length, size_t *out_length);
static char *append_hex_encoded_text(char *target, const char *source);

int mylite_catalog_initialize(mylite_db *database)
{
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

    rc = seed_system_schema(database, "information_schema", "utf8mb3", "utf8mb3_general_ci");
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = seed_system_schema(database, "mysql", "utf8mb4", "utf8mb4_0900_ai_ci");
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = seed_system_schema(database, "performance_schema", "utf8mb4", "utf8mb4_0900_ai_ci");
    if (rc != MYLITE_OK) {
        return rc;
    }
    return seed_system_schema(database, "sys", "utf8mb4", "utf8mb4_0900_ai_ci");
}

int mylite_catalog_update_auto_increment(mylite_db *database, const char *schema_name,
                                         const char *table_name, uint64_t next_auto_increment)
{
    sqlite3_stmt *update = NULL;
    static const char sql[] = "UPDATE __mylite_table_catalog SET auto_increment = ? "
                              "WHERE table_schema = ? AND table_name = ?";
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);

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

int mylite_catalog_delete_table_rows(mylite_db *database, const char *schema_name,
                                     const char *table_name, unsigned int flags)
{
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

static int delete_table_catalog_row(mylite_db *database, const char *sql,
                                    const struct mylite_catalog_table_key *key)
{
    sqlite3_stmt *delete_stmt = NULL;
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &delete_stmt,
                                NULL);

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

int mylite_catalog_selected_schema_default(mylite_db *database,
                                           struct mylite_schema_default *out_default)
{
    sqlite3_stmt *stmt = NULL;
    static const char sql[] =
        "SELECT default_character_set, default_collation FROM __mylite_schema_catalog "
        "WHERE name = ?";
    int rc = SQLITE_OK;

    *out_default = (struct mylite_schema_default){
        .character_set = mylite_charset_default_name(),
        .collation = mylite_charset_default_collation_name(),
    };
    if (database->selected_schema == NULL) {
        return MYLITE_OK;
    }

    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(stmt, 1, database->selected_schema, -1, sqlite_transient_destructor());
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *character_set = (const char *)sqlite3_column_text(stmt, 0);
        const char *collation = (const char *)sqlite3_column_text(stmt, 1);
        const struct mylite_charset *character_set_entry = mylite_charset_lookup(character_set);
        const struct mylite_collation *collation_entry = mylite_collation_lookup(collation);

        if (character_set_entry == NULL) {
            int status = mylite_diagnostics_set_unknown_charset_error(database, character_set);

            sqlite3_finalize(stmt);
            return status;
        }
        if (collation_entry == NULL) {
            int status = mylite_diagnostics_set_unknown_collation_error(database, collation);

            sqlite3_finalize(stmt);
            return status;
        }
        sqlite3_finalize(stmt);
        *out_default = (struct mylite_schema_default){
            .character_set = character_set_entry->name,
            .collation = collation_entry->name,
        };
        return MYLITE_OK;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    if (mylite_diagnostics_set_error_message(
            database, "Selected schema default charset is unavailable") == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}

int mylite_catalog_schema_exists(mylite_db *database, const char *schema_name,
                                 struct mylite_schema_presence *out_presence)
{
    sqlite3_stmt *stmt = NULL;
    static const char sql[] = "SELECT is_system FROM __mylite_schema_catalog WHERE name = ?";
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);

    *out_presence = (struct mylite_schema_presence){
        .exists = false,
        .is_system = false,
    };
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(stmt, 1, schema_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        *out_presence = (struct mylite_schema_presence){
            .exists = true,
            .is_system = sqlite3_column_int(stmt, 0) != 0,
        };
        sqlite3_finalize(stmt);
        return MYLITE_OK;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

int mylite_catalog_table_exists(mylite_db *database, const char *schema_name,
                                const char *table_name, bool *out_exists)
{
    sqlite3_stmt *stmt = NULL;
    static const char sql[] =
        "SELECT 1 FROM __mylite_table_catalog WHERE table_schema = ? AND table_name = ?";
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);

    *out_exists = false;
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
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

int mylite_catalog_load_table_metadata(mylite_db *database, const char *schema_name,
                                       const char *table_name,
                                       struct mylite_catalog_table_metadata *out_metadata)
{
    sqlite3_stmt *stmt = NULL;
    static const char sql[] =
        "SELECT auto_increment FROM __mylite_table_catalog WHERE table_schema = ? "
        "AND table_name = ?";
    int rc = SQLITE_OK;

    if (out_metadata == NULL) {
        return MYLITE_MISUSE;
    }
    *out_metadata = (struct mylite_catalog_table_metadata){0};
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(stmt, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(stmt, 2, table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        sqlite3_int64 value = sqlite3_column_int64(stmt, 0);

        if (value > 0) {
            out_metadata->auto_increment = (uint64_t)value;
            out_metadata->has_auto_increment = true;
        }
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_ROW) {
        return rc == SQLITE_DONE ? mylite_diagnostics_set_table_doesnt_exist_error(
                                       database, schema_name, table_name)
                                 : mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

int mylite_catalog_load_table_columns(mylite_db *database, const char *schema_name,
                                      const char *table_name,
                                      mylite_catalog_column_callback callback, void *context)
{
    sqlite3_stmt *stmt = NULL;
    static const char sql[] =
        "SELECT column_name, column_default, is_nullable, data_type, extra "
        "FROM __mylite_column_catalog WHERE table_schema = ? AND table_name = ? "
        "ORDER BY ordinal_position";
    int rc = SQLITE_OK;

    if (callback == NULL) {
        return MYLITE_MISUSE;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);
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
        int status = callback(context, &row);

        if (status != MYLITE_OK) {
            sqlite3_finalize(stmt);
            return status;
        }
    }
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

int mylite_catalog_load_unique_index_parts(mylite_db *database, const char *schema_name,
                                           const char *table_name,
                                           mylite_catalog_unique_index_part_callback callback,
                                           void *context)
{
    sqlite3_stmt *stmt = NULL;
    static const char sql[] =
        "SELECT index_name, column_name, sub_part FROM __mylite_index_catalog "
        "WHERE table_schema = ? AND table_name = ? AND non_unique = 0 "
        "ORDER BY rowid";
    int rc = SQLITE_OK;

    if (callback == NULL) {
        return MYLITE_MISUSE;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);
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
        int status = callback(context, &row);

        if (status != MYLITE_OK) {
            sqlite3_finalize(stmt);
            return status;
        }
    }
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

int mylite_catalog_schema_default_by_name(mylite_db *database, const char *schema_name,
                                          struct mylite_schema_default *out_default)
{
    sqlite3_stmt *stmt = NULL;
    static const char sql[] =
        "SELECT default_character_set, default_collation FROM __mylite_schema_catalog "
        "WHERE name = ?";
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);

    *out_default = (struct mylite_schema_default){
        .character_set = mylite_charset_default_name(),
        .collation = mylite_charset_default_collation_name(),
    };
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(stmt, 1, schema_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *character_set = (const char *)sqlite3_column_text(stmt, 0);
        const char *collation = (const char *)sqlite3_column_text(stmt, 1);
        const struct mylite_charset *character_set_entry = mylite_charset_lookup(character_set);
        const struct mylite_collation *collation_entry = mylite_collation_lookup(collation);

        if (character_set_entry == NULL) {
            int status = mylite_diagnostics_set_unknown_charset_error(database, character_set);

            sqlite3_finalize(stmt);
            return status;
        }
        if (collation_entry == NULL) {
            int status = mylite_diagnostics_set_unknown_collation_error(database, collation);

            sqlite3_finalize(stmt);
            return status;
        }
        *out_default = (struct mylite_schema_default){
            .character_set = character_set_entry->name,
            .collation = collation_entry->name,
        };
        sqlite3_finalize(stmt);
        return MYLITE_OK;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    (void)mylite_diagnostics_set_error_message_parts(database, "Unknown database '", schema_name,
                                                     "'");
    return MYLITE_EXEC_ERROR;
}

int mylite_catalog_insert_schema(mylite_db *database, const char *schema_name,
                                 const struct mylite_schema_options *options)
{
    enum { bind_read_only = 5 };
    sqlite3_stmt *stmt = NULL;
    static const char sql[] =
        "INSERT INTO __mylite_schema_catalog("
        "name, default_character_set, default_collation, default_encryption, read_only, is_system)"
        " VALUES(?, ?, ?, ?, ?, 0)";
    const char *character_set =
        options->character_set == NULL ? mylite_charset_default_name() : options->character_set;
    const char *collation =
        options->collation == NULL ? mylite_charset_default_collation_name() : options->collation;
    const char *encryption = options->encryption == NULL ? "N" : options->encryption;
    int read_only = 0;
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    if (options->has_read_only) {
        read_only = options->read_only;
    }

    sqlite3_bind_text(stmt, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(stmt, 2, character_set, -1, sqlite_transient_destructor());
    sqlite3_bind_text(stmt, 3, collation, -1, sqlite_transient_destructor());
    sqlite3_bind_text(stmt, 4, encryption, -1, sqlite_transient_destructor());
    sqlite3_bind_int(stmt, bind_read_only, read_only);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

int mylite_catalog_update_schema(mylite_db *database, const char *schema_name,
                                 const struct mylite_schema_options *options)
{
    enum {
        bind_has_read_only = 4,
        bind_read_only = 5,
        bind_schema_name = 6,
    };
    sqlite3_stmt *stmt = NULL;
    int has_read_only = 0;
    static const char sql[] = "UPDATE __mylite_schema_catalog SET "
                              "default_character_set = COALESCE(?, default_character_set),"
                              "default_collation = COALESCE(?, default_collation),"
                              "default_encryption = COALESCE(?, default_encryption),"
                              "read_only = CASE WHEN ? THEN ? ELSE read_only END "
                              "WHERE name = ?";
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    if (options->has_read_only) {
        has_read_only = 1;
    }

    if (options->character_set == NULL) {
        sqlite3_bind_null(stmt, 1);
    } else {
        sqlite3_bind_text(stmt, 1, options->character_set, -1, sqlite_transient_destructor());
    }
    if (options->collation == NULL) {
        sqlite3_bind_null(stmt, 2);
    } else {
        sqlite3_bind_text(stmt, 2, options->collation, -1, sqlite_transient_destructor());
    }
    if (options->encryption == NULL) {
        sqlite3_bind_null(stmt, 3);
    } else {
        sqlite3_bind_text(stmt, 3, options->encryption, -1, sqlite_transient_destructor());
    }
    sqlite3_bind_int(stmt, bind_has_read_only, has_read_only);
    sqlite3_bind_int(stmt, bind_read_only, options->read_only);
    sqlite3_bind_text(stmt, bind_schema_name, schema_name, -1, sqlite_transient_destructor());

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

int mylite_catalog_delete_schema(mylite_db *database, const char *schema_name)
{
    sqlite3_stmt *stmt = NULL;
    static const char sql[] = "DELETE FROM __mylite_schema_catalog WHERE name = ?";
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(stmt, 1, schema_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

char *mylite_catalog_physical_table_name(const char *schema_name, const char *table_name)
{
    static const char prefix[] = "__mylite_user_";
    static const char separator[] = "__";
    size_t prefix_length = sizeof(prefix) - 1U;
    size_t separator_length = sizeof(separator) - 1U;
    size_t schema_length = 0U;
    size_t table_length = 0U;
    size_t schema_hex_length = 0U;
    size_t table_hex_length = 0U;
    size_t output_length = 0U;
    char *output = NULL;
    char *cursor = NULL;

    if (schema_name == NULL || table_name == NULL || schema_name[0] == '\0' ||
        table_name[0] == '\0') {
        return NULL;
    }
    schema_length = strlen(schema_name);
    table_length = strlen(table_name);

    if (!hex_encoded_text_length(schema_length, &schema_hex_length) ||
        !hex_encoded_text_length(table_length, &table_hex_length)) {
        return NULL;
    }
    if (prefix_length > SIZE_MAX - schema_hex_length ||
        prefix_length + schema_hex_length > SIZE_MAX - separator_length ||
        prefix_length + schema_hex_length + separator_length > SIZE_MAX - table_hex_length) {
        return NULL;
    }
    output_length = prefix_length + schema_hex_length + separator_length + table_hex_length;
    if (output_length == SIZE_MAX) {
        return NULL;
    }
    output = malloc(output_length + 1U);
    if (output == NULL) {
        return NULL;
    }

    cursor = output;
    memcpy(cursor, prefix, prefix_length);
    cursor += prefix_length;
    cursor = append_hex_encoded_text(cursor, schema_name);
    memcpy(cursor, separator, separator_length);
    cursor += separator_length;
    cursor = append_hex_encoded_text(cursor, table_name);
    *cursor = '\0';
    return output;
}

static int seed_system_schema(mylite_db *database, const char *name, const char *character_set,
                              const char *collation)
{
    sqlite3_stmt *stmt = NULL;
    static const char sql[] =
        "INSERT INTO __mylite_schema_catalog("
        "name, default_character_set, default_collation, default_encryption, read_only, is_system)"
        " VALUES(?, ?, ?, 'N', 0, 1) "
        "ON CONFLICT(name) DO UPDATE SET "
        "default_character_set = excluded.default_character_set,"
        "default_collation = excluded.default_collation,"
        "default_encryption = 'N',"
        "is_system = 1";
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, character_set, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, collation, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static sqlite3_destructor_type sqlite_transient_destructor(void)
{
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}

static bool hex_encoded_text_length(size_t text_length, size_t *out_length)
{
    enum {
        hex_encoded_byte_width = 2U,
    };

    if (text_length > SIZE_MAX / hex_encoded_byte_width) {
        return false;
    }
    *out_length = text_length * hex_encoded_byte_width;
    return true;
}

static char *append_hex_encoded_text(char *target, const char *source)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    enum {
        hex_digit_high_index = 0U,
        hex_digit_low_index = 1U,
        hex_encoded_byte_width = 2U,
        hex_high_shift = 4U,
        hex_low_mask = 0x0FU,
    };

    while (*source != '\0') {
        unsigned char byte = (unsigned char)*source;

        target[hex_digit_high_index] = hex_digits[byte >> hex_high_shift];
        target[hex_digit_low_index] = hex_digits[byte & hex_low_mask];
        target += hex_encoded_byte_width;
        ++source;
    }
    return target;
}
