#include "mylite_connection.h"

#include "mylite_file_open.h"
#include "sqlite3.h"

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int allocate_database_handle(struct mylite_db **out_database);
static int open_memory_sqlite(struct mylite_db *database);
static int open_file_sqlite(struct mylite_db *database, const char *path);
static int bootstrap_sqlite_connection(struct mylite_db *database);
static int initialize_file_backed_catalog(struct mylite_db *database);
static void destroy_database_handle(struct mylite_db *database);
static int sqlite_status_to_mylite(int sqlite_status);
static void initialize_session_state(struct mylite_session_state *session);
static uint64_t allocate_session_connection_id(void);
static void copy_session_text(char *destination, size_t destination_size, const char *source);

int mylite_open_memory(mylite_db **out_db) {
    struct mylite_db *database = NULL;
    int rc = MYLITE_OK;

    if (out_db == NULL) {
        return MYLITE_MISUSE;
    }

    *out_db = NULL;

    rc = allocate_database_handle(&database);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = open_memory_sqlite(database);
    if (rc != MYLITE_OK) {
        destroy_database_handle(database);
        return rc;
    }

    *out_db = database;

    return MYLITE_OK;
}

int mylite_open(const char *path, mylite_db **out_db) {
    struct mylite_storage_open_state open_state;
    struct mylite_db *database = NULL;
    int rc = MYLITE_OK;

    if (out_db == NULL) {
        return MYLITE_MISUSE;
    }

    *out_db = NULL;
    if (path == NULL || path[0] == '\0') {
        return MYLITE_MISUSE;
    }

    rc = mylite_storage_prepare_mylite_file(path, &open_state);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = allocate_database_handle(&database);
    if (rc == MYLITE_OK) {
        rc = open_file_sqlite(database, path);
    }
    if (rc != MYLITE_OK) {
        destroy_database_handle(database);
        mylite_storage_open_state_deinit(&open_state, path);
        return rc;
    }

    mylite_storage_open_state_mark_published(&open_state);
    mylite_storage_open_state_deinit(&open_state, path);
    *out_db = database;

    return MYLITE_OK;
}

void mylite_close(mylite_db *database) {
    if (database == NULL) {
        return;
    }

    destroy_database_handle(database);
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

const struct mylite_sqlite_bootstrap_state *mylite_connection_sqlite_bootstrap_state_for_test(
    const struct mylite_db *database
) {
    if (database == NULL) {
        return NULL;
    }

    return &database->sqlite_bootstrap;
}

const struct mylite_catalog *mylite_connection_catalog_for_test(const struct mylite_db *database) {
    if (database == NULL) {
        return NULL;
    }

    return &database->catalog;
}

static int allocate_database_handle(struct mylite_db **out_database) {
    struct mylite_db *database = calloc(1U, sizeof(*database));

    if (database == NULL) {
        return MYLITE_NOMEM;
    }

    mylite_diagnostics_init(&database->diagnostics);
    mylite_diagnostics_init(&database->previous_diagnostics);
    initialize_session_state(&database->session);
    mylite_catalog_init(&database->catalog);
    *out_database = database;

    return MYLITE_OK;
}

static int open_memory_sqlite(struct mylite_db *database) {
    sqlite3 *sqlite = NULL;
    int rc = sqlite3_open(":memory:", &sqlite);

    if (rc != SQLITE_OK) {
        if (sqlite != NULL) {
            (void)sqlite3_close(sqlite);
        }
        return sqlite_status_to_mylite(rc);
    }

    database->sqlite = sqlite;

    return bootstrap_sqlite_connection(database);
}

static int open_file_sqlite(struct mylite_db *database, const char *path) {
    sqlite3 *sqlite = NULL;
    int rc = MYLITE_OK;

    rc = mylite_storage_vfs_ensure_registered();
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = sqlite3_open_v2(
        path,
        &sqlite,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
        mylite_storage_vfs_name()
    );
    if (rc != SQLITE_OK) {
        if (sqlite != NULL) {
            (void)sqlite3_close(sqlite);
        }
        return sqlite_status_to_mylite(rc);
    }

    database->sqlite = sqlite;

    rc = bootstrap_sqlite_connection(database);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_storage_configure_sqlite_payload(database->sqlite);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return initialize_file_backed_catalog(database);
}

static int bootstrap_sqlite_connection(struct mylite_db *database) {
    return mylite_sqlite_bootstrap_connection(
        database->sqlite,
        database,
        &database->sqlite_bootstrap
    );
}

static int initialize_file_backed_catalog(struct mylite_db *database) {
    return mylite_catalog_initialize_file_backed(database);
}

static void destroy_database_handle(struct mylite_db *database) {
    if (database == NULL) {
        return;
    }

    if (database->sqlite != NULL && database->session.user_transaction_active) {
        (void)sqlite3_exec(database->sqlite, "ROLLBACK", NULL, NULL, NULL);
        database->session.user_transaction_active = false;
        database->session.active_transaction_read_only = false;
    }
    free(database->session.savepoints);
    database->session.savepoints = NULL;
    database->session.savepoint_count = 0U;
    database->session.savepoint_capacity = 0U;
    free(database->session.table_locks);
    database->session.table_locks = NULL;
    database->session.table_lock_count = 0U;
    database->session.table_lock_capacity = 0U;
    for (size_t index = 0U; index < database->session.user_variable_count; ++index) {
        free(database->session.user_variables[index].value);
    }
    free(database->session.user_variables);
    database->session.user_variables = NULL;
    database->session.user_variable_count = 0U;
    database->session.user_variable_capacity = 0U;
    for (size_t index = 0U; index < database->session.prepared_statement_count; ++index) {
        free(database->session.prepared_statements[index].sql);
    }
    free(database->session.prepared_statements);
    database->session.prepared_statements = NULL;
    database->session.prepared_statement_count = 0U;
    database->session.prepared_statement_capacity = 0U;
    mylite_temporary_catalog_deinit(&database->session.temporary_catalog);
    mylite_catalog_deinit(&database->catalog);
    mylite_sqlite_bootstrap_deinit(database->sqlite, &database->sqlite_bootstrap);
    if (database->sqlite != NULL) {
        (void)sqlite3_close(database->sqlite);
    }
    database->sqlite = NULL;
    mylite_diagnostics_deinit(&database->previous_diagnostics);
    mylite_diagnostics_deinit(&database->diagnostics);
    free(database);
}

static int sqlite_status_to_mylite(int sqlite_status) {
    if (sqlite_status == SQLITE_OK) {
        return MYLITE_OK;
    }
    if (sqlite_status == SQLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    if (sqlite_status == SQLITE_MISUSE) {
        return MYLITE_MISUSE;
    }

    return MYLITE_ERROR;
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
    session->sql_mode = MYLITE_SESSION_SQL_MODE_DEFAULT_BITS;
    copy_session_text(
        session->sql_mode_text,
        sizeof(session->sql_mode_text),
        MYLITE_SESSION_SQL_MODE_DEFAULT_TEXT
    );
    session->sql_mode_is_placeholder = false;
    copy_session_text(session->time_zone, sizeof(session->time_zone), "SYSTEM");
    session->time_zone_offset_minutes = 0;
    session->time_zone_is_placeholder = false;
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
    session->foreign_key_checks_enabled = true;
    mylite_temporary_catalog_init(&session->temporary_catalog);
    session->connection_id = allocate_session_connection_id();
    session->previous_row_count = -1;
    session->found_rows = 1U;
    session->last_insert_id = 0U;
    session->auto_increment_increment = 1U;
    session->auto_increment_offset = 1U;
    session->sql_select_limit = UINT64_MAX;
    session->group_concat_max_len = MYLITE_SESSION_GROUP_CONCAT_MAX_LEN_DEFAULT_VALUE;
    session->group_concat_value_ordinal = 0U;
    session->wait_timeout = MYLITE_SESSION_TIMEOUT_DEFAULT_VALUE;
    session->interactive_timeout = MYLITE_SESSION_TIMEOUT_DEFAULT_VALUE;
    session->catalog_generation = 0U;
    session->sqlite_schema_generation = 0U;
    session->user_transaction_active = false;
    session->session_transaction_isolation = MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ;
    session->session_transaction_access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE;
    session->has_next_transaction_isolation = false;
    session->next_transaction_isolation = MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ;
    session->has_next_transaction_access_mode = false;
    session->next_transaction_access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE;
    session->next_transaction_isolation_from_system_variable = false;
    session->next_transaction_access_mode_from_system_variable = false;
    session->active_transaction_read_only = false;
    session->savepoints = NULL;
    session->savepoint_count = 0U;
    session->savepoint_capacity = 0U;
    session->next_savepoint_id = 1U;
    session->table_locks = NULL;
    session->table_lock_count = 0U;
    session->table_lock_capacity = 0U;
    session->user_variables = NULL;
    session->user_variable_count = 0U;
    session->user_variable_capacity = 0U;
    session->prepared_statements = NULL;
    session->prepared_statement_count = 0U;
    session->prepared_statement_capacity = 0U;
    session->has_timestamp_override = false;
    session->timestamp_override = 0;
    session->active_statement_time = 0;
}

static uint64_t allocate_session_connection_id(void) {
    static atomic_uint_fast64_t next_connection_id = 1U;

    return (uint64_t)atomic_fetch_add_explicit(&next_connection_id, 1U, memory_order_relaxed);
}

static void copy_session_text(char *destination, size_t destination_size, const char *source) {
    int written = snprintf(destination, destination_size, "%s", source == NULL ? "" : source);

    if (written < 0) {
        destination[0] = '\0';
    }
}
