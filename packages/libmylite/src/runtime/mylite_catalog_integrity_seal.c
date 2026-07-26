#include "mylite_catalog_internal.h"

#include "mylite_sqlite_registration.h"
#include "sqlite3.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    integrity_trigger_sql_capacity = 2048,
    integrity_trigger_table_capacity = 64,
};

struct catalog_integrity_trigger_spec {
    const char *name;
    const char *table;
    const char *event;
};

static const struct catalog_integrity_trigger_spec integrity_trigger_specs[] = {
    {"_mylite_integrity_state_insert", "_mylite_catalog_state", "INSERT"},
    {"_mylite_integrity_state_update",
     "_mylite_catalog_state",
     "UPDATE OF singleton_id, schema_version, minimum_reader_schema_version, "
     "catalog_generation, created_with_file_format_version"},
    {"_mylite_integrity_schemas_insert", "_mylite_catalog_schemas", "INSERT"},
    {"_mylite_integrity_schemas_update", "_mylite_catalog_schemas", "UPDATE"},
    {"_mylite_integrity_schemas_delete", "_mylite_catalog_schemas", "DELETE"},
    {"_mylite_integrity_tables_insert", "_mylite_catalog_tables", "INSERT"},
    {"_mylite_integrity_tables_update",
     "_mylite_catalog_tables",
     "UPDATE OF table_id, schema_id, name, kind, physical_name, default_charset, "
     "default_collation, comment, row_format_option, key_block_size, pack_keys, checksum, "
     "stats_persistent, stats_auto_recalc, stats_sample_pages, min_rows, max_rows, "
     "avg_row_length, delay_key_write, fulltext_doc_id_initialized, created_time_utc_epoch, "
     "descriptor_version, created_catalog_generation, updated_catalog_generation"},
    {"_mylite_integrity_tables_delete", "_mylite_catalog_tables", "DELETE"},
    {"_mylite_integrity_views_insert", "_mylite_catalog_views", "INSERT"},
    {"_mylite_integrity_views_update", "_mylite_catalog_views", "UPDATE"},
    {"_mylite_integrity_views_delete", "_mylite_catalog_views", "DELETE"},
    {"_mylite_integrity_columns_insert", "_mylite_catalog_columns", "INSERT"},
    {"_mylite_integrity_columns_update", "_mylite_catalog_columns", "UPDATE"},
    {"_mylite_integrity_columns_delete", "_mylite_catalog_columns", "DELETE"},
    {"_mylite_integrity_indexes_insert", "_mylite_catalog_indexes", "INSERT"},
    {"_mylite_integrity_indexes_update", "_mylite_catalog_indexes", "UPDATE"},
    {"_mylite_integrity_indexes_delete", "_mylite_catalog_indexes", "DELETE"},
    {"_mylite_integrity_index_columns_insert", "_mylite_catalog_index_columns", "INSERT"},
    {"_mylite_integrity_index_columns_update", "_mylite_catalog_index_columns", "UPDATE"},
    {"_mylite_integrity_index_columns_delete", "_mylite_catalog_index_columns", "DELETE"},
    {"_mylite_integrity_foreign_keys_insert", "_mylite_catalog_foreign_keys", "INSERT"},
    {"_mylite_integrity_foreign_keys_update", "_mylite_catalog_foreign_keys", "UPDATE"},
    {"_mylite_integrity_foreign_keys_delete", "_mylite_catalog_foreign_keys", "DELETE"},
    {"_mylite_integrity_foreign_key_columns_insert", "_mylite_catalog_foreign_key_columns", "INSERT"
    },
    {"_mylite_integrity_foreign_key_columns_update", "_mylite_catalog_foreign_key_columns", "UPDATE"
    },
    {"_mylite_integrity_foreign_key_columns_delete", "_mylite_catalog_foreign_key_columns", "DELETE"
    },
    {"_mylite_integrity_checks_insert", "_mylite_catalog_check_constraints", "INSERT"},
    {"_mylite_integrity_checks_update", "_mylite_catalog_check_constraints", "UPDATE"},
    {"_mylite_integrity_checks_delete", "_mylite_catalog_check_constraints", "DELETE"},
};

static int build_integrity_trigger_sql(
    const struct catalog_integrity_trigger_spec *spec,
    char *sql,
    size_t sql_capacity
);
static bool normalized_sql_equal(const char *left, const char *right);
static const unsigned char *skip_sql_space(const unsigned char *text);

int mylite_catalog_create_integrity_seal_triggers(sqlite3 *sqlite) {
    char sql[integrity_trigger_sql_capacity];
    int rc = sqlite == NULL ? MYLITE_MISUSE : MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < sizeof(integrity_trigger_specs) /
                                                           sizeof(integrity_trigger_specs[0U]);
         ++index) {
        rc = build_integrity_trigger_sql(&integrity_trigger_specs[index], sql, sizeof(sql));
        if (rc == MYLITE_OK) {
            rc = mylite_catalog_execute_sql(sqlite, sql);
        }
    }
    return rc;
}

int mylite_catalog_validate_integrity_seal_triggers(sqlite3 *sqlite) {
    sqlite3_stmt *statement = NULL;
    char expected_sql[integrity_trigger_sql_capacity];
    char stored_sql[integrity_trigger_sql_capacity];
    char table[integrity_trigger_table_capacity];
    int sqlite_rc = SQLITE_OK;
    int rc = sqlite == NULL ? MYLITE_MISUSE : MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < sizeof(integrity_trigger_specs) /
                                                           sizeof(integrity_trigger_specs[0U]);
         ++index) {
        const struct catalog_integrity_trigger_spec *spec = &integrity_trigger_specs[index];

        rc = build_integrity_trigger_sql(spec, expected_sql, sizeof(expected_sql));
        if (rc == MYLITE_OK) {
            rc = mylite_catalog_prepare_statement(
                sqlite,
                "SELECT tbl_name, sql FROM sqlite_master WHERE type = 'trigger' AND name = ?1",
                &statement
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_catalog_bind_text(statement, 1, spec->name);
        }
        if (rc == MYLITE_OK) {
            sqlite_rc = mylite_catalog_sqlite3_step(statement);
            if (sqlite_rc != SQLITE_ROW) {
                rc = sqlite_rc == SQLITE_DONE ? MYLITE_ERROR
                                              : mylite_sqlite_status_to_mylite(sqlite_rc);
            }
        }
        if (rc == MYLITE_OK) {
            rc = mylite_catalog_checked_column_text(statement, 0, table, sizeof(table));
        }
        if (rc == MYLITE_OK) {
            rc = mylite_catalog_checked_column_text(statement, 1, stored_sql, sizeof(stored_sql));
        }
        if (rc == MYLITE_OK &&
            (strcmp(table, spec->table) != 0 || !normalized_sql_equal(stored_sql, expected_sql))) {
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            sqlite_rc = mylite_catalog_sqlite3_step(statement);
            if (sqlite_rc != SQLITE_DONE) {
                rc = sqlite_rc == SQLITE_ROW ? MYLITE_ERROR
                                             : mylite_sqlite_status_to_mylite(sqlite_rc);
            }
        }
        rc = mylite_catalog_finalize_statement(statement, rc);
        statement = NULL;
    }
    return rc;
}

int mylite_catalog_read_sqlite_schema_version(sqlite3 *sqlite, uint64_t *out_schema_version) {
    sqlite3_stmt *statement = NULL;
    int64_t schema_version = 0;
    int sqlite_rc = SQLITE_OK;
    int rc = sqlite == NULL || out_schema_version == NULL ? MYLITE_MISUSE : MYLITE_OK;

    if (out_schema_version != NULL) {
        *out_schema_version = 0U;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(sqlite, "PRAGMA main.schema_version", &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = mylite_catalog_sqlite3_step(statement);
        if (sqlite_rc != SQLITE_ROW) {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(statement, 0, &schema_version);
    }
    if (rc == MYLITE_OK && schema_version <= 0) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        *out_schema_version = (uint64_t)schema_version;
        sqlite_rc = mylite_catalog_sqlite3_step(statement);
        if (sqlite_rc != SQLITE_DONE) {
            rc = sqlite_rc == SQLITE_ROW ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_publish_integrity_seal(sqlite3 *sqlite, uint64_t catalog_generation) {
    sqlite3_stmt *statement = NULL;
    uint64_t schema_version = 0U;
    int rc = mylite_catalog_validate_generation(catalog_generation);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_sqlite_schema_version(sqlite, &schema_version);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            sqlite,
            "UPDATE _mylite_catalog_state "
            "SET integrity_catalog_generation = ?1, integrity_sqlite_schema_version = ?2 "
            "WHERE singleton_id = 1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 1, catalog_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 2, schema_version);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(sqlite);
    }
    return mylite_catalog_finalize_statement(statement, rc);
}

static int build_integrity_trigger_sql(
    const struct catalog_integrity_trigger_spec *spec,
    char *sql,
    size_t sql_capacity
) {
    int written = 0;

    if (spec == NULL || sql == NULL || sql_capacity == 0U) {
        return MYLITE_MISUSE;
    }
    written = snprintf(
        sql,
        sql_capacity,
        "CREATE TRIGGER %s AFTER %s ON %s BEGIN "
        "UPDATE _mylite_catalog_state "
        "SET integrity_catalog_generation = 0, integrity_sqlite_schema_version = 0 "
        "WHERE singleton_id = 1; END",
        spec->name,
        spec->event,
        spec->table
    );
    return written < 0 || (size_t)written >= sql_capacity ? MYLITE_ERROR : MYLITE_OK;
}

static bool normalized_sql_equal(const char *left, const char *right) {
    const unsigned char *left_cursor = (const unsigned char *)left;
    const unsigned char *right_cursor = (const unsigned char *)right;

    if (left == NULL || right == NULL) {
        return false;
    }
    for (;;) {
        left_cursor = skip_sql_space(left_cursor);
        right_cursor = skip_sql_space(right_cursor);
        if (tolower(*left_cursor) != tolower(*right_cursor)) {
            return false;
        }
        if (*left_cursor == '\0') {
            return true;
        }
        ++left_cursor;
        ++right_cursor;
    }
}

static const unsigned char *skip_sql_space(const unsigned char *text) {
    while (*text != '\0' && isspace(*text) != 0) {
        ++text;
    }
    return text;
}
