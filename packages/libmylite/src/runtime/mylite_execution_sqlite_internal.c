#include "mylite_execution_sqlite_internal.h"

#include "mylite_connection.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_sqlite_registration.h"

#include <mylite/mylite.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum {
    sqlite_use_nul_terminated_string = -1,
};

static int prepare_uncached_sqlite_statement(
    sqlite3 *sqlite,
    const char *sql,
    unsigned int prepare_flags,
    sqlite3_stmt **out_statement
);
static void prune_stale_cached_sqlite_statements(struct mylite_db *database);
static struct mylite_execution_statement_cache_entry *find_available_cached_sqlite_statement(
    struct mylite_db *database,
    const char *sql
);
static int append_cached_sqlite_statement(
    struct mylite_db *database,
    const char *sql,
    sqlite3_stmt **out_statement
);
static void evict_available_cached_sqlite_statement(struct mylite_db *database);
static char *copy_sqlite_statement_sql(const char *sql);
static struct mylite_execution_statement_cache_entry *find_cached_sqlite_statement_by_handle(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t *out_index
);
static void remove_cached_sqlite_statement(struct mylite_db *database, size_t index);
static void clear_cached_sqlite_statements(struct mylite_db *database, bool include_in_use);

int mylite_execution_execute_sqlite_schema_sql(struct mylite_db *database, const char *sql) {
    int sqlite_rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    int rc = mylite_sqlite_status_to_mylite(sqlite_rc);

    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        mylite_execution_diagnostics_set_physical_sqlite_error(database);
        return MYLITE_ERROR;
    }

    clear_cached_sqlite_statements(database, false);

    return MYLITE_OK;
}

int mylite_execution_execute_sqlite_control_sql(const struct mylite_db *database, const char *sql) {
    int sqlite_rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

int mylite_execution_execute_cached_sqlite_control_sql(
    struct mylite_db *database,
    const char *sql
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_execution_prepare_cached_sqlite_statement(database, sql, &statement);

    if (rc == MYLITE_OK) {
        int sqlite_rc = sqlite3_step(statement);

        rc = sqlite_rc == SQLITE_DONE ? MYLITE_OK : mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return mylite_execution_finish_cached_sqlite_statement(database, statement, rc);
}

int mylite_execution_prepare_sqlite_statement(
    const struct mylite_db *database,
    const char *sql,
    sqlite3_stmt **out_statement
) {
    if (database == NULL || sql == NULL || out_statement == NULL) {
        return MYLITE_MISUSE;
    }

    return prepare_uncached_sqlite_statement(database->sqlite, sql, 0U, out_statement);
}

int mylite_execution_finalize_sqlite_statement(sqlite3_stmt *statement, int rc) {
    int sqlite_rc = SQLITE_OK;

    if (statement == NULL) {
        return rc;
    }

    sqlite_rc = sqlite3_finalize(statement);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

int mylite_execution_prepare_cached_sqlite_statement(
    struct mylite_db *database,
    const char *sql,
    sqlite3_stmt **out_statement
) {
    struct mylite_execution_statement_cache_entry *entry = NULL;

    if (database == NULL || sql == NULL || out_statement == NULL) {
        return MYLITE_MISUSE;
    }

    *out_statement = NULL;
    prune_stale_cached_sqlite_statements(database);
    entry = find_available_cached_sqlite_statement(database, sql);
    if (entry != NULL) {
        entry->in_use = true;
        *out_statement = entry->statement;
        return MYLITE_OK;
    }
    if (database->execution_statement_cache_count >= MYLITE_EXECUTION_STATEMENT_CACHE_LIMIT) {
        evict_available_cached_sqlite_statement(database);
    }
    if (database->execution_statement_cache_count < MYLITE_EXECUTION_STATEMENT_CACHE_LIMIT) {
        return append_cached_sqlite_statement(database, sql, out_statement);
    }

    return prepare_uncached_sqlite_statement(database->sqlite, sql, 0U, out_statement);
}

int mylite_execution_finish_cached_sqlite_statement(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int rc
) {
    struct mylite_execution_statement_cache_entry *entry = NULL;
    size_t entry_index = 0U;
    int sqlite_rc = SQLITE_OK;

    if (statement == NULL) {
        return rc;
    }
    if (database == NULL) {
        return mylite_execution_finalize_sqlite_statement(statement, rc);
    }

    entry = find_cached_sqlite_statement_by_handle(database, statement, &entry_index);
    if (entry == NULL) {
        return mylite_execution_finalize_sqlite_statement(statement, rc);
    }
    if (rc != MYLITE_OK) {
        remove_cached_sqlite_statement(database, entry_index);
        return rc;
    }

    sqlite_rc = sqlite3_reset(statement);
    if (sqlite_rc == SQLITE_OK) {
        sqlite_rc = sqlite3_clear_bindings(statement);
    }
    if (sqlite_rc != SQLITE_OK) {
        remove_cached_sqlite_statement(database, entry_index);
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    entry->in_use = false;
    return MYLITE_OK;
}

void mylite_execution_statement_cache_deinit(struct mylite_db *database) {
    if (database == NULL) {
        return;
    }

    clear_cached_sqlite_statements(database, true);
}

static int prepare_uncached_sqlite_statement(
    sqlite3 *sqlite,
    const char *sql,
    unsigned int prepare_flags,
    sqlite3_stmt **out_statement
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;

    if (sqlite == NULL || sql == NULL || out_statement == NULL) {
        return MYLITE_MISUSE;
    }

    *out_statement = NULL;
    sqlite_rc = sqlite3_prepare_v3(
        sqlite,
        sql,
        sqlite_use_nul_terminated_string,
        prepare_flags,
        &statement,
        NULL
    );
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    *out_statement = statement;
    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static void prune_stale_cached_sqlite_statements(struct mylite_db *database) {
    size_t index = 0U;

    if (database == NULL) {
        return;
    }

    while (index < database->execution_statement_cache_count) {
        const struct mylite_execution_statement_cache_entry *entry =
            &database->execution_statement_cache[index];

        if (entry->in_use ||
            (entry->catalog_generation == database->session.catalog_generation &&
             entry->sqlite_schema_generation == database->session.sqlite_schema_generation)) {
            ++index;
            continue;
        }

        remove_cached_sqlite_statement(database, index);
    }
}

static struct mylite_execution_statement_cache_entry *find_available_cached_sqlite_statement(
    struct mylite_db *database,
    const char *sql
) {
    if (database == NULL || sql == NULL) {
        return NULL;
    }

    for (size_t index = 0U; index < database->execution_statement_cache_count; ++index) {
        struct mylite_execution_statement_cache_entry *entry =
            &database->execution_statement_cache[index];

        if (!entry->in_use && entry->sql != NULL && strcmp(entry->sql, sql) == 0) {
            return entry;
        }
    }

    return NULL;
}

static int append_cached_sqlite_statement(
    struct mylite_db *database,
    const char *sql,
    sqlite3_stmt **out_statement
) {
    struct mylite_execution_statement_cache_entry *entry = NULL;
    sqlite3_stmt *statement = NULL;
    char *sql_copy = NULL;
    int rc = MYLITE_OK;

    rc = prepare_uncached_sqlite_statement(database->sqlite, sql, 0U, &statement);
    if (rc != MYLITE_OK) {
        return rc;
    }

    sql_copy = copy_sqlite_statement_sql(sql);
    if (sql_copy == NULL) {
        sqlite3_finalize(statement);
        return MYLITE_NOMEM;
    }

    entry = &database->execution_statement_cache[database->execution_statement_cache_count];
    ++database->execution_statement_cache_count;
    *entry = (struct mylite_execution_statement_cache_entry){
        .sql = sql_copy,
        .statement = statement,
        .catalog_generation = database->session.catalog_generation,
        .sqlite_schema_generation = database->session.sqlite_schema_generation,
        .in_use = true,
    };
    *out_statement = statement;

    return MYLITE_OK;
}

static void evict_available_cached_sqlite_statement(struct mylite_db *database) {
    if (database == NULL) {
        return;
    }

    for (size_t index = 0U; index < database->execution_statement_cache_count; ++index) {
        if (!database->execution_statement_cache[index].in_use) {
            remove_cached_sqlite_statement(database, index);
            return;
        }
    }
}

static char *copy_sqlite_statement_sql(const char *sql) {
    size_t sql_size = 0U;
    char *copy = NULL;

    if (sql == NULL) {
        return NULL;
    }

    sql_size = strlen(sql) + 1U;
    copy = malloc(sql_size);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, sql, sql_size);
    return copy;
}

static struct mylite_execution_statement_cache_entry *find_cached_sqlite_statement_by_handle(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t *out_index
) {
    if (database == NULL || statement == NULL) {
        return NULL;
    }

    for (size_t index = 0U; index < database->execution_statement_cache_count; ++index) {
        struct mylite_execution_statement_cache_entry *entry =
            &database->execution_statement_cache[index];

        if (entry->statement == statement) {
            if (out_index != NULL) {
                *out_index = index;
            }
            return entry;
        }
    }

    return NULL;
}

static void remove_cached_sqlite_statement(struct mylite_db *database, size_t index) {
    struct mylite_execution_statement_cache_entry *entry = NULL;

    if (database == NULL || index >= database->execution_statement_cache_count) {
        return;
    }

    entry = &database->execution_statement_cache[index];
    sqlite3_finalize(entry->statement);
    free(entry->sql);

    --database->execution_statement_cache_count;
    if (index != database->execution_statement_cache_count) {
        database->execution_statement_cache[index] =
            database->execution_statement_cache[database->execution_statement_cache_count];
    }
    database->execution_statement_cache[database->execution_statement_cache_count] =
        (struct mylite_execution_statement_cache_entry){0};
}

static void clear_cached_sqlite_statements(struct mylite_db *database, bool include_in_use) {
    size_t index = 0U;

    if (database == NULL) {
        return;
    }

    while (index < database->execution_statement_cache_count) {
        if (!include_in_use && database->execution_statement_cache[index].in_use) {
            ++index;
            continue;
        }

        remove_cached_sqlite_statement(database, index);
    }
}
