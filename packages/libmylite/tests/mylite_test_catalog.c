#include "mylite_test_support.h"

#include "sqlite3.h"

#include <stddef.h>
#include <stdio.h>

enum {
    integrity_trigger_capacity = 64,
    integrity_trigger_name_capacity = 128,
    integrity_drop_sql_capacity = 192,
};

static int drop_integrity_triggers(sqlite3 *sqlite);
static int execute_sql(sqlite3 *sqlite, const char *sql);

int mylite_test_remove_catalog_integrity_seal(struct sqlite3 *sqlite) {
    static const char rebuild_state_sql[] =
        "ALTER TABLE _mylite_catalog_state RENAME TO _mylite_catalog_state_v38;"
        "CREATE TABLE _mylite_catalog_state ("
        "singleton_id INTEGER PRIMARY KEY CHECK(singleton_id = 1),"
        "schema_version INTEGER NOT NULL,"
        "minimum_reader_schema_version INTEGER NOT NULL,"
        "catalog_generation INTEGER NOT NULL,"
        "created_with_file_format_version INTEGER NOT NULL);"
        "INSERT INTO _mylite_catalog_state "
        "(singleton_id, schema_version, minimum_reader_schema_version, catalog_generation, "
        "created_with_file_format_version) "
        "SELECT singleton_id, schema_version, minimum_reader_schema_version, catalog_generation, "
        "created_with_file_format_version FROM _mylite_catalog_state_v38;"
        "DROP TABLE _mylite_catalog_state_v38";
    int rc = sqlite == NULL ? 1 : execute_sql(sqlite, "BEGIN IMMEDIATE");

    if (rc == 0) {
        rc = drop_integrity_triggers(sqlite);
    }
    if (rc == 0) {
        rc = execute_sql(sqlite, rebuild_state_sql);
    }
    if (rc == 0) {
        rc = execute_sql(sqlite, "COMMIT");
    }
    if (rc != 0 && sqlite != NULL) {
        (void)execute_sql(sqlite, "ROLLBACK");
    }
    return rc;
}

static int drop_integrity_triggers(sqlite3 *sqlite) {
    sqlite3_stmt *statement = NULL;
    char trigger_names[integrity_trigger_capacity][integrity_trigger_name_capacity];
    char sql[integrity_drop_sql_capacity];
    size_t trigger_count = 0U;
    int step_rc = SQLITE_OK;
    int rc = sqlite3_prepare_v2(
        sqlite,
        "SELECT name FROM sqlite_master "
        "WHERE type = 'trigger' AND name LIKE '_mylite_integrity_%' ORDER BY name",
        -1,
        &statement,
        NULL
    );

    while (rc == SQLITE_OK && (step_rc = sqlite3_step(statement)) == SQLITE_ROW) {
        const unsigned char *name = sqlite3_column_text(statement, 0);
        int written = 0;

        if (name == NULL || trigger_count >= integrity_trigger_capacity) {
            rc = SQLITE_ERROR;
            break;
        }
        written = snprintf(
            trigger_names[trigger_count],
            sizeof(trigger_names[trigger_count]),
            "%s",
            (const char *)name
        );
        if (written < 0 || (size_t)written >= sizeof(trigger_names[trigger_count])) {
            rc = SQLITE_ERROR;
            break;
        }
        ++trigger_count;
    }
    if (rc == SQLITE_OK && step_rc != SQLITE_DONE) {
        rc = step_rc;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK || rc != SQLITE_OK || trigger_count == 0U) {
        return 1;
    }
    for (size_t index = 0U; index < trigger_count; ++index) {
        int written = snprintf(sql, sizeof(sql), "DROP TRIGGER \"%s\"", trigger_names[index]);

        if (written < 0 || (size_t)written >= sizeof(sql) || execute_sql(sqlite, sql) != 0) {
            return 1;
        }
    }
    return 0;
}

static int execute_sql(sqlite3 *sqlite, const char *sql) {
    return sqlite3_exec(sqlite, sql, NULL, NULL, NULL) == SQLITE_OK ? 0 : 1;
}
