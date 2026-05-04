#include "mylite_connection.h"

#include "mylite_catalog.h"
#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_expression.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_transactions.h"
#include "mylite_vfs.h"
#include "sqlite3.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static const uint64_t mylite_embedded_connection_id = 1U;

static int open_sqlite_database(const char *filename, int flags, const char *vfs_name,
                                mylite_db **out_db);

int mylite_open(const char *filename, mylite_db **out_db)
{
    int rc = SQLITE_OK;

    if (filename == NULL || out_db == NULL) {
        return MYLITE_MISUSE;
    }

    *out_db = NULL;
    rc = mylite_vfs_register();
    if (rc != SQLITE_OK) {
        return MYLITE_SQLITE_ERROR;
    }

    return open_sqlite_database(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                                mylite_vfs_name(), out_db);
}

int mylite_open_memory(mylite_db **out_db)
{
    if (out_db == NULL) {
        return MYLITE_MISUSE;
    }

    return open_sqlite_database(
        ":memory:", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY, NULL, out_db);
}

void mylite_close(mylite_db *database)
{
    if (database == NULL) {
        return;
    }

    if (database->transaction_active) {
        (void)mylite_transaction_rollback_explicit(database);
    }
    sqlite3_close(database->sqlite);
    free(database->error_message);
    mylite_expression_warnings_deinit(&database->warnings);
    free(database->selected_schema);
    mylite_transaction_savepoint_state_deinit(&database->savepoints);
    mylite_transaction_clear_pending_auto_increments(database);
    free(database);
}

uint64_t mylite_last_insert_id(const mylite_db *database)
{
    if (database == NULL) {
        return 0U;
    }

    return database->last_insert_id;
}

const char *mylite_connection_character_set_client(const mylite_db *database)
{
    return database == NULL ? NULL : database->character_set_client;
}

const char *mylite_connection_character_set_connection(const mylite_db *database)
{
    return database == NULL ? NULL : database->character_set_connection;
}

const char *mylite_connection_character_set_results(const mylite_db *database)
{
    return database == NULL ? NULL : database->character_set_results;
}

const char *mylite_connection_collation_connection(const mylite_db *database)
{
    return database == NULL ? NULL : database->collation_connection;
}

int mylite_connection_set_default_state(mylite_db *database)
{
    database->character_set_client = mylite_charset_default_name();
    database->character_set_connection = mylite_charset_default_name();
    database->character_set_results = mylite_charset_default_name();
    database->collation_connection = mylite_charset_default_collation_name();
    return MYLITE_OK;
}

int mylite_connection_set_released_error(mylite_db *database)
{
    int status = mylite_diagnostics_set_error_message(
        database, "Connection was released by transaction completion");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_connection_set_selected_schema(mylite_db *database, const char *schema_name)
{
    char *copy = mylite_copy_span_text(schema_name, strlen(schema_name));

    if (copy == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    free(database->selected_schema);
    database->selected_schema = copy;
    return MYLITE_OK;
}

void mylite_connection_clear_selected_schema_if_matches(mylite_db *database,
                                                        const char *schema_name)
{
    if (database->selected_schema != NULL && strcmp(database->selected_schema, schema_name) == 0) {
        free(database->selected_schema);
        database->selected_schema = NULL;
    }
}

int mylite_connection_set_names_state(mylite_db *database, const char *character_set_name,
                                      const char *collation_name)
{
    const struct mylite_charset *character_set = mylite_charset_lookup(character_set_name);
    const struct mylite_collation *collation = NULL;

    if (character_set == NULL) {
        return mylite_diagnostics_set_unknown_charset_error(database, character_set_name);
    }

    if (collation_name == NULL) {
        collation = mylite_collation_lookup(character_set->default_collation);
    } else {
        collation = mylite_collation_lookup(collation_name);
        if (collation == NULL) {
            return mylite_diagnostics_set_unknown_collation_error(database, collation_name);
        }
        if (!mylite_charset_collation_match(character_set, collation)) {
            return mylite_diagnostics_set_collation_charset_error(database, collation->name,
                                                                  character_set->name);
        }
    }

    database->character_set_client = character_set->name;
    database->character_set_connection = character_set->name;
    database->character_set_results = character_set->name;
    database->collation_connection = collation->name;
    return MYLITE_OK;
}

int mylite_connection_set_character_set_state(mylite_db *database, const char *character_set_name)
{
    struct mylite_schema_default schema_default;
    const struct mylite_charset *character_set = mylite_charset_lookup(character_set_name);
    const struct mylite_collation *connection_collation = NULL;
    int status = MYLITE_OK;

    if (character_set == NULL) {
        return mylite_diagnostics_set_unknown_charset_error(database, character_set_name);
    }

    status = mylite_catalog_selected_schema_default(database, &schema_default);
    if (status != MYLITE_OK) {
        return status;
    }

    connection_collation = mylite_collation_lookup(schema_default.collation);
    if (connection_collation == NULL) {
        return mylite_diagnostics_set_unknown_collation_error(database, schema_default.collation);
    }

    database->character_set_client = character_set->name;
    database->character_set_connection = connection_collation->character_set;
    database->character_set_results = character_set->name;
    database->collation_connection = connection_collation->name;
    return MYLITE_OK;
}

static int open_sqlite_database(const char *filename, int flags, const char *vfs_name,
                                mylite_db **out_db)
{
    mylite_db *database = calloc(1U, sizeof(*database));
    int rc = SQLITE_OK;

    *out_db = NULL;
    if (database == NULL) {
        return MYLITE_NOMEM;
    }
    database->status_started_at = time(NULL);
    database->connection_id = mylite_embedded_connection_id;

    rc = sqlite3_open_v2(filename, &database->sqlite, flags, vfs_name);
    if (rc != SQLITE_OK) {
        sqlite3_close(database->sqlite);
        free(database);
        return MYLITE_SQLITE_ERROR;
    }

    rc = mylite_catalog_initialize(database);
    if (rc != MYLITE_OK) {
        sqlite3_close(database->sqlite);
        free(database->error_message);
        free(database);
        return rc;
    }

    (void)mylite_connection_set_default_state(database);
    *out_db = database;
    return MYLITE_OK;
}
