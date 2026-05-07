#include "mylite_connection.h"

#include "sqlite3.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static void initialize_session_state(struct mylite_session_state *session);
static void copy_session_text(char *destination, size_t destination_size, const char *source);

int mylite_open_memory(mylite_db **out_db) {
    struct mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int rc = SQLITE_OK;

    if (out_db == NULL) {
        return MYLITE_MISUSE;
    }

    *out_db = NULL;

    database = calloc(1U, sizeof(*database));
    if (database == NULL) {
        return MYLITE_NOMEM;
    }

    mylite_diagnostics_init(&database->diagnostics);
    initialize_session_state(&database->session);

    rc = sqlite3_open(":memory:", &sqlite);
    if (rc != SQLITE_OK) {
        if (sqlite != NULL) {
            (void)sqlite3_close(sqlite);
        }
        mylite_diagnostics_deinit(&database->diagnostics);
        free(database);
        return rc == SQLITE_NOMEM ? MYLITE_NOMEM : MYLITE_ERROR;
    }

    database->sqlite = sqlite;
    *out_db = database;

    return MYLITE_OK;
}

void mylite_close(mylite_db *database) {
    if (database == NULL) {
        return;
    }

    if (database->sqlite != NULL) {
        (void)sqlite3_close(database->sqlite);
    }
    database->sqlite = NULL;
    mylite_diagnostics_deinit(&database->diagnostics);
    free(database);
}

int mylite_errcode(const mylite_db *database) {
    if (database == NULL) {
        return MYLITE_MISUSE;
    }

    return mylite_diagnostics_errcode(&database->diagnostics);
}

const char *mylite_sqlstate(const mylite_db *database) {
    if (database == NULL) {
        return mylite_diagnostics_misuse_sqlstate();
    }

    return mylite_diagnostics_sqlstate(&database->diagnostics);
}

const char *mylite_errmsg(const mylite_db *database) {
    if (database == NULL) {
        return mylite_diagnostics_misuse_message();
    }

    return mylite_diagnostics_errmsg(&database->diagnostics);
}

struct mylite_diagnostics *mylite_connection_diagnostics(struct mylite_db *database) {
    if (database == NULL) {
        return NULL;
    }

    return &database->diagnostics;
}

const struct mylite_session_state *mylite_connection_session_state(
    const struct mylite_db *database
) {
    if (database == NULL) {
        return NULL;
    }

    return &database->session;
}

struct sqlite3 *mylite_connection_sqlite_for_test(struct mylite_db *database) {
    if (database == NULL) {
        return NULL;
    }

    return database->sqlite;
}

static void initialize_session_state(struct mylite_session_state *session) {
    session->has_selected_schema = false;
    session->selected_schema[0] = '\0';
    copy_session_text(
        session->current_user_identity,
        sizeof(session->current_user_identity),
        "root@%"
    );
    copy_session_text(
        session->client_user_identity,
        sizeof(session->client_user_identity),
        "root@%"
    );
    session->sql_mode = 0U;
    session->sql_mode_is_placeholder = true;
    copy_session_text(session->time_zone, sizeof(session->time_zone), "");
    session->time_zone_is_placeholder = true;
    copy_session_text(
        session->character_set_client,
        sizeof(session->character_set_client),
        "utf8mb4"
    );
    copy_session_text(
        session->character_set_connection,
        sizeof(session->character_set_connection),
        "utf8mb4"
    );
    copy_session_text(
        session->character_set_results,
        sizeof(session->character_set_results),
        "utf8mb4"
    );
    copy_session_text(
        session->collation_connection,
        sizeof(session->collation_connection),
        "utf8mb4_0900_ai_ci"
    );
    session->character_set_state_is_placeholder = true;
    session->system_variables_are_placeholder = true;
    session->catalog_generation = 0U;
    session->sqlite_schema_generation = 0U;
}

static void copy_session_text(char *destination, size_t destination_size, const char *source) {
    int written = snprintf(destination, destination_size, "%s", source == NULL ? "" : source);

    if (written < 0) {
        destination[0] = '\0';
    }
}
