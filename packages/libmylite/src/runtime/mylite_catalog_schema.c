#include "mylite_catalog.h"

#include "mylite_catalog_schema.h"
#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "sqlite3.h"

static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_catalog_seed_system_schema(mylite_db *database, const char *name,
                                      const char *character_set, const char *collation)
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

static sqlite3_destructor_type sqlite_transient_destructor(void)
{
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
