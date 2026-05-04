#include "mylite_transactions.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int reapply_pending_auto_increments(mylite_db *database);
static void clear_user_savepoints(mylite_db *database);
static int reserve_user_savepoint_capacity(mylite_db *database, size_t required_capacity);
static char *make_sqlite_savepoint_name(mylite_db *database);
static int exec_sqlite_savepoint_command(mylite_db *database, const char *command,
                                         const char *sqlite_name);
static void remove_user_savepoint_at(mylite_db *database, size_t index);
static void remove_user_savepoints_from(mylite_db *database, size_t first);
static int append_user_savepoint(mylite_db *database, struct mylite_savepoint savepoint);
static void savepoint_deinit(struct mylite_savepoint *savepoint);

int mylite_transaction_begin_explicit(mylite_db *database,
                                      enum mylite_transaction_access_mode access_mode,
                                      bool consistent_snapshot)
{
    int rc = sqlite3_exec(database->sqlite, "BEGIN DEFERRED", NULL, NULL, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    database->transaction_active = true;
    database->transaction_access_mode = access_mode;
    database->transaction_consistent_snapshot = consistent_snapshot;
    return MYLITE_OK;
}

int mylite_transaction_commit_explicit(mylite_db *database)
{
    int status = mylite_transaction_commit_storage(database);

    if (status != MYLITE_OK) {
        return status;
    }

    database->transaction_active = false;
    database->transaction_access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE;
    database->transaction_consistent_snapshot = false;
    clear_user_savepoints(database);
    mylite_transaction_clear_pending_auto_increments(database);
    return MYLITE_OK;
}

int mylite_transaction_rollback_explicit(mylite_db *database)
{
    int status = MYLITE_OK;
    int rc = sqlite3_exec(database->sqlite, "ROLLBACK", NULL, NULL, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    database->transaction_active = false;
    database->transaction_access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE;
    database->transaction_consistent_snapshot = false;
    status = reapply_pending_auto_increments(database);
    clear_user_savepoints(database);
    mylite_transaction_clear_pending_auto_increments(database);
    return status;
}

int mylite_transaction_create_savepoint(mylite_db *database, const char *name,
                                        const char *normalized_name)
{
    struct mylite_savepoint savepoint = {0};
    int status = MYLITE_OK;

    savepoint.original_name = mylite_copy_span_text(name, strlen(name));
    savepoint.normalized_name = mylite_copy_span_text(normalized_name, strlen(normalized_name));
    savepoint.sqlite_name = make_sqlite_savepoint_name(database);
    savepoint.level = database->savepoints.current_level;
    if (savepoint.original_name == NULL || savepoint.normalized_name == NULL ||
        savepoint.sqlite_name == NULL) {
        savepoint_deinit(&savepoint);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = reserve_user_savepoint_capacity(database, database->savepoints.count + 1U);
    if (status == MYLITE_OK) {
        status = exec_sqlite_savepoint_command(database, "SAVEPOINT", savepoint.sqlite_name);
    }
    if (status == MYLITE_OK) {
        size_t existing = mylite_transaction_find_savepoint(database, normalized_name);

        if (existing != SIZE_MAX) {
            remove_user_savepoint_at(database, existing);
        }
        status = append_user_savepoint(database, savepoint);
    }
    if (status != MYLITE_OK) {
        savepoint_deinit(&savepoint);
    }
    return status;
}

size_t mylite_transaction_find_savepoint(const mylite_db *database, const char *normalized_name)
{
    for (size_t index = database->savepoints.count; index > 0U; --index) {
        const struct mylite_savepoint *savepoint = &database->savepoints.items[index - 1U];

        if (savepoint->level == database->savepoints.current_level &&
            strcmp(savepoint->normalized_name, normalized_name) == 0) {
            return index - 1U;
        }
    }
    return SIZE_MAX;
}

int mylite_transaction_rollback_to_savepoint(mylite_db *database, size_t index)
{
    int status = exec_sqlite_savepoint_command(database, "ROLLBACK TO SAVEPOINT",
                                               database->savepoints.items[index].sqlite_name);

    if (status != MYLITE_OK) {
        return status;
    }

    remove_user_savepoints_from(database, index + 1U);
    return reapply_pending_auto_increments(database);
}

int mylite_transaction_release_savepoint(mylite_db *database, size_t index)
{
    int status = exec_sqlite_savepoint_command(database, "RELEASE SAVEPOINT",
                                               database->savepoints.items[index].sqlite_name);

    if (status == MYLITE_OK) {
        remove_user_savepoints_from(database, index);
    }
    return status;
}

void mylite_transaction_savepoint_state_deinit(struct mylite_savepoint_state *state)
{
    if (state == NULL) {
        return;
    }

    for (size_t index = 0U; index < state->count; ++index) {
        savepoint_deinit(&state->items[index]);
    }
    free(state->items);
    *state = (struct mylite_savepoint_state){0};
}

int mylite_transaction_record_pending_auto_increment(mylite_db *database, const char *schema_name,
                                                     const char *table_name,
                                                     uint64_t next_auto_increment)
{
    struct mylite_pending_auto_increment *items = NULL;
    char *schema_copy = NULL;
    char *table_copy = NULL;

    if (!database->transaction_active) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; index < database->pending_auto_increment_count; ++index) {
        struct mylite_pending_auto_increment *item = &database->pending_auto_increments[index];

        if (strcmp(item->schema_name, schema_name) == 0 &&
            strcmp(item->table_name, table_name) == 0) {
            if (next_auto_increment > item->next_auto_increment) {
                item->next_auto_increment = next_auto_increment;
            }
            return MYLITE_OK;
        }
    }

    if (database->pending_auto_increment_count >=
        SIZE_MAX / sizeof(*database->pending_auto_increments)) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    schema_copy = mylite_copy_span_text(schema_name, strlen(schema_name));
    table_copy = mylite_copy_span_text(table_name, strlen(table_name));
    if (schema_copy == NULL || table_copy == NULL) {
        free(schema_copy);
        free(table_copy);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    items = realloc(database->pending_auto_increments,
                    (database->pending_auto_increment_count + 1U) * sizeof(*items));
    if (items == NULL) {
        free(schema_copy);
        free(table_copy);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    database->pending_auto_increments = items;
    database->pending_auto_increments[database->pending_auto_increment_count] =
        (struct mylite_pending_auto_increment){
            .schema_name = schema_copy,
            .table_name = table_copy,
            .next_auto_increment = next_auto_increment,
        };
    ++database->pending_auto_increment_count;
    return MYLITE_OK;
}

void mylite_transaction_clear_pending_auto_increments(mylite_db *database)
{
    for (size_t index = 0U; index < database->pending_auto_increment_count; ++index) {
        free(database->pending_auto_increments[index].schema_name);
        free(database->pending_auto_increments[index].table_name);
    }
    free(database->pending_auto_increments);
    database->pending_auto_increments = NULL;
    database->pending_auto_increment_count = 0U;
}

int mylite_transaction_begin_statement_atomicity(mylite_db *database,
                                                 struct mylite_statement_atomicity *atomicity)
{
    int rc = SQLITE_OK;

    if (atomicity == NULL) {
        return MYLITE_MISUSE;
    }

    *atomicity = (struct mylite_statement_atomicity){0};
    if (!database->transaction_active) {
        int status = mylite_transaction_begin_storage(database);

        if (status == MYLITE_OK) {
            atomicity->kind = MYLITE_STATEMENT_ATOMICITY_TRANSACTION;
        }
        return status;
    }

    rc = sqlite3_exec(database->sqlite, "SAVEPOINT mylite_statement_atomicity", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    atomicity->kind = MYLITE_STATEMENT_ATOMICITY_SAVEPOINT;
    return MYLITE_OK;
}

int mylite_transaction_commit_statement_atomicity(mylite_db *database,
                                                  struct mylite_statement_atomicity *atomicity)
{
    int rc = SQLITE_OK;

    if (atomicity == NULL) {
        return MYLITE_MISUSE;
    }

    switch (atomicity->kind) {
    case MYLITE_STATEMENT_ATOMICITY_TRANSACTION:
        atomicity->kind = MYLITE_STATEMENT_ATOMICITY_NONE;
        return mylite_transaction_commit_storage(database);
    case MYLITE_STATEMENT_ATOMICITY_SAVEPOINT:
        rc = sqlite3_exec(database->sqlite, "RELEASE SAVEPOINT mylite_statement_atomicity", NULL,
                          NULL, NULL);
        atomicity->kind = MYLITE_STATEMENT_ATOMICITY_NONE;
        return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
    case MYLITE_STATEMENT_ATOMICITY_NONE:
        return MYLITE_OK;
    }

    return MYLITE_MISUSE;
}

void mylite_transaction_rollback_statement_atomicity(
    mylite_db *database, const struct mylite_statement_atomicity *atomicity)
{
    if (atomicity == NULL) {
        return;
    }

    switch (atomicity->kind) {
    case MYLITE_STATEMENT_ATOMICITY_TRANSACTION:
        mylite_transaction_rollback_storage(database);
        break;
    case MYLITE_STATEMENT_ATOMICITY_SAVEPOINT:
        (void)sqlite3_exec(database->sqlite, "ROLLBACK TO SAVEPOINT mylite_statement_atomicity",
                           NULL, NULL, NULL);
        (void)sqlite3_exec(database->sqlite, "RELEASE SAVEPOINT mylite_statement_atomicity", NULL,
                           NULL, NULL);
        break;
    case MYLITE_STATEMENT_ATOMICITY_NONE:
        break;
    }
}

static int reapply_pending_auto_increments(mylite_db *database)
{
    for (size_t index = 0U; index < database->pending_auto_increment_count; ++index) {
        const struct mylite_pending_auto_increment *item =
            &database->pending_auto_increments[index];
        int status = mylite_catalog_update_auto_increment(
            database, item->schema_name, item->table_name, item->next_auto_increment);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_transaction_begin_storage(mylite_db *database)
{
    int rc = sqlite3_exec(database->sqlite, "BEGIN IMMEDIATE", NULL, NULL, NULL);

    return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

int mylite_transaction_commit_storage(mylite_db *database)
{
    int rc = sqlite3_exec(database->sqlite, "COMMIT", NULL, NULL, NULL);

    return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

void mylite_transaction_rollback_storage(mylite_db *database)
{
    (void)sqlite3_exec(database->sqlite, "ROLLBACK", NULL, NULL, NULL);
}

static void clear_user_savepoints(mylite_db *database)
{
    remove_user_savepoints_from(database, 0U);
}

static int reserve_user_savepoint_capacity(mylite_db *database, size_t required_capacity)
{
    struct mylite_savepoint *items = NULL;
    size_t capacity = database->savepoints.capacity == 0U ? 4U : database->savepoints.capacity;

    if (required_capacity > SIZE_MAX / sizeof(*database->savepoints.items)) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (required_capacity > database->savepoints.capacity) {
        while (capacity < required_capacity) {
            if (capacity > SIZE_MAX / 2U) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
            capacity *= 2U;
        }
        items = realloc(database->savepoints.items, capacity * sizeof(*items));
        if (items == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        database->savepoints.items = items;
        database->savepoints.capacity = capacity;
    }
    return MYLITE_OK;
}

static char *make_sqlite_savepoint_name(mylite_db *database)
{
    uint64_t next_id = database->savepoints.next_sqlite_id + 1U;
    int length = 0;
    char *name = NULL;

    if (next_id == 0U) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return NULL;
    }
    length = snprintf(NULL, 0, "mylite_user_savepoint_%llu", (unsigned long long)next_id);
    if (length < 0) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return NULL;
    }

    name = malloc((size_t)length + 1U);
    if (name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return NULL;
    }
    (void)snprintf(name, (size_t)length + 1U, "mylite_user_savepoint_%llu",
                   (unsigned long long)next_id);
    database->savepoints.next_sqlite_id = next_id;
    return name;
}

static int exec_sqlite_savepoint_command(mylite_db *database, const char *command,
                                         const char *sqlite_name)
{
    char *sql = sqlite3_mprintf("%s \"%w\"", command, sqlite_name);
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static void remove_user_savepoint_at(mylite_db *database, size_t index)
{
    if (index >= database->savepoints.count) {
        return;
    }

    savepoint_deinit(&database->savepoints.items[index]);
    for (size_t next = index + 1U; next < database->savepoints.count; ++next) {
        database->savepoints.items[next - 1U] = database->savepoints.items[next];
    }
    --database->savepoints.count;
    database->savepoints.items[database->savepoints.count] = (struct mylite_savepoint){0};
}

static void remove_user_savepoints_from(mylite_db *database, size_t first)
{
    if (first > database->savepoints.count) {
        return;
    }
    for (size_t index = first; index < database->savepoints.count; ++index) {
        savepoint_deinit(&database->savepoints.items[index]);
    }
    database->savepoints.count = first;
}

static int append_user_savepoint(mylite_db *database, struct mylite_savepoint savepoint)
{
    if (database->savepoints.count >= database->savepoints.capacity) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    database->savepoints.items[database->savepoints.count++] = savepoint;
    return MYLITE_OK;
}

static void savepoint_deinit(struct mylite_savepoint *savepoint)
{
    if (savepoint == NULL) {
        return;
    }

    free(savepoint->original_name);
    free(savepoint->normalized_name);
    free(savepoint->sqlite_name);
    *savepoint = (struct mylite_savepoint){0};
}
