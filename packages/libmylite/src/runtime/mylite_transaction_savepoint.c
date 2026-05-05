#include "mylite_transactions.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int set_savepoint_does_not_exist_error(mylite_db *database, const char *name);
static int reserve_user_savepoint_capacity(mylite_db *database, size_t required_capacity);
static char *make_sqlite_savepoint_name(mylite_db *database);
static int exec_sqlite_savepoint_command(mylite_db *database, const char *command,
                                         const char *sqlite_name);
static void remove_user_savepoint_at(mylite_db *database, size_t index);
static void remove_user_savepoints_from(mylite_db *database, size_t first);
static int append_user_savepoint(mylite_db *database, struct mylite_savepoint savepoint);
static void savepoint_deinit(struct mylite_savepoint *savepoint);
static char *copy_normalized_savepoint_name(const char *name);

int mylite_transaction_copy_savepoint_statement(const struct mylite_sql_ast_node *statement,
                                                mylite_stmt *stmt)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(statement, 0U);

    if (name == NULL || name->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_UNSUPPORTED;
    }

    stmt->savepoint.name = mylite_copy_identifier_span(name);
    if (stmt->savepoint.name == NULL) {
        return MYLITE_NOMEM;
    }
    stmt->savepoint.normalized_name = copy_normalized_savepoint_name(stmt->savepoint.name);
    return stmt->savepoint.normalized_name == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

void mylite_transaction_savepoint_plan_deinit(struct mylite_savepoint_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    free(plan->name);
    free(plan->normalized_name);
    *plan = (struct mylite_savepoint_plan){0};
}

int mylite_transaction_execute_savepoint_statement(mylite_stmt *stmt)
{
    int status = MYLITE_OK;

    if (!stmt->database->transaction_active) {
        stmt->affected_rows = 0;
        return MYLITE_OK;
    }

    status = mylite_transaction_create_savepoint(stmt->database, stmt->savepoint.name,
                                                 stmt->savepoint.normalized_name);
    stmt->affected_rows = status == MYLITE_OK ? 0 : -1;
    return status;
}

int mylite_transaction_execute_rollback_to_savepoint_statement(mylite_stmt *stmt)
{
    size_t index =
        mylite_transaction_find_savepoint(stmt->database, stmt->savepoint.normalized_name);
    int status = MYLITE_OK;

    if (index == SIZE_MAX) {
        stmt->affected_rows = -1;
        return set_savepoint_does_not_exist_error(stmt->database, stmt->savepoint.name);
    }

    status = mylite_transaction_rollback_to_savepoint(stmt->database, index);
    stmt->affected_rows = status == MYLITE_OK ? 0 : -1;
    return status;
}

int mylite_transaction_execute_release_savepoint_statement(mylite_stmt *stmt)
{
    size_t index =
        mylite_transaction_find_savepoint(stmt->database, stmt->savepoint.normalized_name);
    int status = MYLITE_OK;

    if (index == SIZE_MAX) {
        stmt->affected_rows = -1;
        return set_savepoint_does_not_exist_error(stmt->database, stmt->savepoint.name);
    }

    status = mylite_transaction_release_savepoint(stmt->database, index);
    stmt->affected_rows = status == MYLITE_OK ? 0 : -1;
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
    return mylite_transaction_reapply_pending_auto_increments(database);
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

static int set_savepoint_does_not_exist_error(mylite_db *database, const char *name)
{
    char *message = sqlite3_mprintf("SAVEPOINT %q does not exist", name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_SP_DOES_NOT_EXIST,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

void mylite_transaction_clear_user_savepoints(mylite_db *database)
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

static char *copy_normalized_savepoint_name(const char *name)
{
    char *copy = mylite_copy_span_text(name, name == NULL ? 0U : strlen(name));

    if (copy == NULL) {
        return NULL;
    }

    for (size_t index = 0U; copy[index] != '\0'; ++index) {
        if (copy[index] >= 'A' && copy[index] <= 'Z') {
            copy[index] = (char)(copy[index] - 'A' + 'a');
        }
    }
    return copy;
}
