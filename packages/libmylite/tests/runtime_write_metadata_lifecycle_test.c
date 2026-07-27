#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "sqlite3.h"

#include <stdint.h>
#include <string.h>

enum {
    first_updated_time = 100,
    second_updated_time = 200,
};

struct write_metadata_trace {
    size_t data_version_count;
    size_t updated_time_count;
    size_t guarded_insert_count;
    size_t parent_lookup_count;
};

static int test_transaction_snapshot_sync_is_reused(void);
static int test_updated_time_cache_is_rollback_aware(void);
static int test_foreign_key_insert_uses_guarded_statement(void);
static int count_write_metadata_statements(
    unsigned int trace_type,
    void *context, // NOLINT(bugprone-easily-swappable-parameters): SQLite trace callback ABI.
    void *statement_pointer,
    void *expanded_sql
);
static int seed_schema(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql);
static int read_items_table_id(sqlite3 *sqlite, int64_t *out_table_id);
static int read_items_updated_time(sqlite3 *sqlite, int64_t *out_updated_time);

int main(void) {
    int failures = 0;

    failures += test_transaction_snapshot_sync_is_reused();
    failures += test_updated_time_cache_is_rollback_aware();
    failures += test_foreign_key_insert_uses_guarded_statement();

    return failures == 0 ? 0 : 1;
}

static int test_transaction_snapshot_sync_is_reused(void) {
    struct write_metadata_trace trace = {0};
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open transaction snapshot database"
    );
    if (database == NULL) {
        return failures;
    }
    failures += seed_schema(database);
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += mylite_test_expect_int(
        sqlite3_trace_v2(sqlite, SQLITE_TRACE_STMT, count_write_metadata_statements, &trace),
        SQLITE_OK,
        "install transaction snapshot trace"
    );

    failures += execute_ok(database, "START TRANSACTION");
    trace.data_version_count = 0U;
    failures += execute_ok(database, "INSERT INTO items VALUES (1, 10)");
    failures += execute_ok(database, "INSERT INTO items VALUES (2, 20)");
    failures += execute_ok(database, "SELECT COUNT(*) FROM items");
    failures += mylite_test_expect_size(
        trace.data_version_count,
        1U,
        "one catalog sync within transaction snapshot"
    );

    failures += execute_ok(database, "COMMIT");
    failures += execute_ok(database, "START TRANSACTION");
    trace.data_version_count = 0U;
    failures += execute_ok(database, "SELECT COUNT(*) FROM items");
    failures += mylite_test_expect_size(
        trace.data_version_count,
        1U,
        "new transaction refreshes catalog snapshot"
    );

    failures += execute_ok(database, "COMMIT AND CHAIN");
    trace.data_version_count = 0U;
    failures += execute_ok(database, "SELECT COUNT(*) FROM items");
    failures += mylite_test_expect_size(
        trace.data_version_count,
        1U,
        "chained transaction refreshes catalog snapshot"
    );
    failures += execute_ok(database, "ROLLBACK");

    failures += mylite_test_expect_int(
        sqlite3_trace_v2(sqlite, 0U, NULL, NULL),
        SQLITE_OK,
        "remove transaction snapshot trace"
    );
    mylite_close(database);
    return failures;
}

static int test_updated_time_cache_is_rollback_aware(void) {
    struct write_metadata_trace trace = {0};
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int64_t table_id = 0;
    int64_t updated_time = 0;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open table status database"
    );
    if (database == NULL) {
        return failures;
    }
    failures += seed_schema(database);
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += read_items_table_id(sqlite, &table_id);
    failures += mylite_test_expect_int(
        sqlite3_trace_v2(sqlite, SQLITE_TRACE_STMT, count_write_metadata_statements, &trace),
        SQLITE_OK,
        "install table status trace"
    );

    failures += mylite_test_expect_int(
        mylite_catalog_update_table_updated_time(database, table_id, first_updated_time),
        MYLITE_OK,
        "write first table timestamp"
    );
    failures += mylite_test_expect_int(
        mylite_catalog_update_table_updated_time(database, table_id, first_updated_time),
        MYLITE_OK,
        "coalesce identical table timestamp"
    );
    failures += mylite_test_expect_size(
        trace.updated_time_count,
        1U,
        "one physical write for identical table timestamp"
    );

    failures += execute_ok(database, "START TRANSACTION");
    failures += mylite_test_expect_int(
        mylite_catalog_update_table_updated_time(database, table_id, second_updated_time),
        MYLITE_OK,
        "write transactional table timestamp"
    );
    failures += mylite_test_expect_int(
        mylite_catalog_update_table_updated_time(database, table_id, second_updated_time),
        MYLITE_OK,
        "coalesce transactional table timestamp"
    );
    failures += execute_ok(database, "ROLLBACK");
    failures += mylite_test_expect_int(
        mylite_catalog_update_table_updated_time(database, table_id, second_updated_time),
        MYLITE_OK,
        "rewrite rolled back table timestamp"
    );
    failures += mylite_test_expect_size(
        trace.updated_time_count,
        3U,
        "rollback invalidates table timestamp cache"
    );
    failures += read_items_updated_time(sqlite, &updated_time);
    failures += mylite_test_expect_int64(
        updated_time,
        second_updated_time,
        "persisted table timestamp after rollback"
    );

    failures += mylite_test_expect_int(
        sqlite3_trace_v2(sqlite, 0U, NULL, NULL),
        SQLITE_OK,
        "remove table status trace"
    );
    mylite_close(database);
    return failures;
}

static int test_foreign_key_insert_uses_guarded_statement(void) {
    struct write_metadata_trace trace = {0};
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open guarded foreign-key database"
    );
    if (database == NULL) {
        return failures;
    }
    failures += seed_schema(database);
    failures += execute_ok(database, "CREATE TABLE parents (id BIGINT PRIMARY KEY)");
    failures += execute_ok(
        database,
        "CREATE TABLE children ("
        "id BIGINT PRIMARY KEY, parent_id BIGINT NOT NULL,"
        "FOREIGN KEY (parent_id) REFERENCES parents (id))"
    );
    failures += execute_ok(database, "INSERT INTO parents VALUES (1)");
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += mylite_test_expect_int(
        sqlite3_trace_v2(sqlite, SQLITE_TRACE_STMT, count_write_metadata_statements, &trace),
        SQLITE_OK,
        "install guarded foreign-key trace"
    );

    failures += execute_ok(database, "INSERT INTO children VALUES (1, 1)");
    failures += mylite_test_expect_size(
        trace.guarded_insert_count,
        1U,
        "foreign-key insert executes one guarded statement"
    );
    failures += mylite_test_expect_size(
        trace.parent_lookup_count,
        0U,
        "foreign-key insert avoids standalone parent lookup"
    );

    failures += mylite_test_expect_int(
        sqlite3_trace_v2(sqlite, 0U, NULL, NULL),
        SQLITE_OK,
        "remove guarded foreign-key trace"
    );
    mylite_close(database);
    return failures;
}

static int count_write_metadata_statements(
    unsigned int trace_type,
    void *context, // NOLINT(bugprone-easily-swappable-parameters): SQLite trace callback ABI.
    void *statement_pointer,
    void *expanded_sql
) {
    static const char updated_time_sql[] =
        "UPDATE _mylite_catalog_tables SET updated_time_utc_epoch";
    struct write_metadata_trace *trace = (struct write_metadata_trace *)context;
    sqlite3_stmt *statement = (sqlite3_stmt *)statement_pointer;
    const char *sql = statement == NULL ? NULL : sqlite3_sql(statement);

    (void)expanded_sql;
    if (trace_type != SQLITE_TRACE_STMT || trace == NULL || sql == NULL) {
        return 0;
    }
    if (strcmp(sql, "PRAGMA main.data_version") == 0) {
        ++trace->data_version_count;
    } else if (strncmp(sql, updated_time_sql, sizeof(updated_time_sql) - 1U) == 0) {
        ++trace->updated_time_count;
    } else if (strncmp(sql, "INSERT INTO ", sizeof("INSERT INTO ") - 1U) == 0 &&
               strstr(sql, " EXISTS (SELECT 1 FROM ") != NULL) {
        ++trace->guarded_insert_count;
    } else if (strncmp(sql, "SELECT 1 FROM ", sizeof("SELECT 1 FROM ") - 1U) == 0) {
        ++trace->parent_lookup_count;
    }
    return 0;
}

static int seed_schema(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE items (id BIGINT PRIMARY KEY, value INT)");
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = mylite_test_expect_int(rc, MYLITE_OK, sql);

    mylite_result_free(result);
    return failures;
}

static int read_items_table_id(sqlite3 *sqlite, int64_t *out_table_id) {
    static const char sql[] =
        "SELECT tables.table_id "
        "FROM _mylite_catalog_tables AS tables "
        "JOIN _mylite_catalog_schemas AS schemas ON schemas.schema_id = tables.schema_id "
        "WHERE schemas.name = 'app' AND tables.name = 'items'";
    sqlite3_stmt *statement = NULL;
    int failures = 0;
    int sqlite_rc = sqlite3_prepare_v2(sqlite, sql, -1, &statement, NULL);

    failures += mylite_test_expect_int(sqlite_rc, SQLITE_OK, "prepare table id query");
    if (sqlite_rc == SQLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        failures += mylite_test_expect_int(sqlite_rc, SQLITE_ROW, "step table id query");
        if (sqlite_rc == SQLITE_ROW) {
            *out_table_id = sqlite3_column_int64(statement, 0);
        }
    }
    failures +=
        mylite_test_expect_int(sqlite3_finalize(statement), SQLITE_OK, "finalize table id query");
    return failures;
}

static int read_items_updated_time(sqlite3 *sqlite, int64_t *out_updated_time) {
    static const char sql[] =
        "SELECT tables.updated_time_utc_epoch "
        "FROM _mylite_catalog_tables AS tables "
        "JOIN _mylite_catalog_schemas AS schemas ON schemas.schema_id = tables.schema_id "
        "WHERE schemas.name = 'app' AND tables.name = 'items'";
    sqlite3_stmt *statement = NULL;
    int failures = 0;
    int sqlite_rc = sqlite3_prepare_v2(sqlite, sql, -1, &statement, NULL);

    failures += mylite_test_expect_int(sqlite_rc, SQLITE_OK, "prepare updated time query");
    if (sqlite_rc == SQLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        failures += mylite_test_expect_int(sqlite_rc, SQLITE_ROW, "step updated time query");
        if (sqlite_rc == SQLITE_ROW) {
            *out_updated_time = sqlite3_column_int64(statement, 0);
        }
    }
    failures += mylite_test_expect_int(
        sqlite3_finalize(statement),
        SQLITE_OK,
        "finalize updated time query"
    );
    return failures;
}
