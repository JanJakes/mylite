#include "mylite_show_create_table_indexes.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_show_create_common.h"
#include "mylite_show_create_table_body.h"
#include "mylite_show_create_table_target.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <stdbool.h>

static int append_show_create_table_index(sqlite3_str *create_sql, sqlite3_stmt *select);
static int append_show_create_table_key_parts(mylite_db *database, sqlite3_str *create_sql,
                                              sqlite3_stmt *select, const char *index_name,
                                              bool temporary);

int mylite_show_create_table_append_indexes(mylite_db *database, sqlite3_str *create_sql,
                                            const struct mylite_show_create_table_target *target,
                                            bool *first_line)
{
    sqlite3_stmt *select = NULL;
    char *sql = sqlite3_mprintf(
        "WITH first_parts AS ("
        " SELECT index_name, MIN(rowid) AS first_rowid FROM %s "
        " WHERE table_schema = ? AND table_name = ? GROUP BY index_name)"
        "SELECT i.index_name, i.non_unique, i.index_comment, i.is_visible, "
        "i.table_schema, i.table_name "
        "FROM %s i "
        "JOIN first_parts f ON f.index_name = i.index_name "
        "WHERE i.table_schema = ? AND i.table_name = ? AND i.seq_in_index = 1 "
        "ORDER BY CASE WHEN i.index_name = 'PRIMARY' THEN 0 WHEN i.non_unique = 0 THEN 1 "
        "ELSE 2 END, f.first_rowid",
        mylite_catalog_index_catalog_name(target->temporary),
        mylite_catalog_index_catalog_name(target->temporary));
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(select, 1, target->schema_name, -1,
                      mylite_show_sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, target->table_name, -1, mylite_show_sqlite_transient_destructor());
    sqlite3_bind_text(select, 3, target->schema_name, -1,
                      mylite_show_sqlite_transient_destructor());
    sqlite3_bind_text(select, 4, target->table_name, -1, mylite_show_sqlite_transient_destructor());

    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        int status = MYLITE_OK;

        mylite_show_create_table_append_line_prefix(create_sql, first_line);
        status = append_show_create_table_index(create_sql, select);
        if (status == MYLITE_OK) {
            status = append_show_create_table_key_parts(
                database, create_sql, select, (const char *)sqlite3_column_text(select, 0),
                target->temporary);
        }
        if (status != MYLITE_OK) {
            sqlite3_finalize(select);
            return status;
        }
    }

    sqlite3_finalize(select);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int append_show_create_table_index(sqlite3_str *create_sql, sqlite3_stmt *select)
{
    enum {
        index_name_column = 0,
        non_unique_column = 1,
    };
    const char *index_name = (const char *)sqlite3_column_text(select, index_name_column);
    int non_unique = sqlite3_column_int(select, non_unique_column);

    if (mylite_ascii_case_equal(index_name, "PRIMARY")) {
        sqlite3_str_appendall(create_sql, "PRIMARY KEY (");
    } else {
        if (non_unique == 0) {
            sqlite3_str_appendall(create_sql, "UNIQUE ");
        }
        sqlite3_str_appendall(create_sql, "KEY ");
        mylite_show_create_append_identifier(create_sql, index_name);
        sqlite3_str_appendall(create_sql, " (");
    }
    return sqlite3_str_errcode(create_sql) == SQLITE_OK ? MYLITE_OK : MYLITE_NOMEM;
}

static int append_show_create_table_key_parts(mylite_db *database, sqlite3_str *create_sql,
                                              sqlite3_stmt *select, const char *index_name,
                                              bool temporary)
{
    enum {
        index_comment_column = 2,
        is_visible_column = 3,
        table_schema_column = 4,
        table_name_column = 5,
    };
    sqlite3_stmt *parts = NULL;
    sqlite3 *sqlite = sqlite3_db_handle(select);
    char *sql = sqlite3_mprintf("SELECT column_name, collation, sub_part FROM %s "
                                "WHERE table_schema = ? AND table_name = ? AND index_name = ? "
                                "ORDER BY seq_in_index",
                                mylite_catalog_index_catalog_name(temporary));
    const char *schema_name = (const char *)sqlite3_column_text(select, table_schema_column);
    const char *table_name = (const char *)sqlite3_column_text(select, table_name_column);
    const char *index_comment = (const char *)sqlite3_column_text(select, index_comment_column);
    const char *is_visible = (const char *)sqlite3_column_text(select, is_visible_column);
    bool first_part = true;
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &parts, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(parts, 1, schema_name, -1, mylite_show_sqlite_transient_destructor());
    sqlite3_bind_text(parts, 2, table_name, -1, mylite_show_sqlite_transient_destructor());
    sqlite3_bind_text(parts, 3, index_name, -1, mylite_show_sqlite_transient_destructor());

    while ((rc = sqlite3_step(parts)) == SQLITE_ROW) {
        enum {
            column_name_column = 0,
            collation_column = 1,
            sub_part_column = 2,
        };
        const char *column_name = (const char *)sqlite3_column_text(parts, column_name_column);
        const char *collation = (const char *)sqlite3_column_text(parts, collation_column);

        if (!first_part) {
            sqlite3_str_appendall(create_sql, ",");
        }
        first_part = false;
        mylite_show_create_append_identifier(create_sql, column_name);
        if (sqlite3_column_type(parts, sub_part_column) != SQLITE_NULL) {
            sqlite3_str_appendf(create_sql, "(%lld)", sqlite3_column_int64(parts, sub_part_column));
        }
        if (mylite_ascii_case_equal(collation, "D")) {
            sqlite3_str_appendall(create_sql, " DESC");
        }
    }
    sqlite3_finalize(parts);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_str_appendall(create_sql, ")");
    if (index_comment != NULL && index_comment[0] != '\0') {
        sqlite3_str_appendall(create_sql, " COMMENT ");
        mylite_show_create_append_string_literal(create_sql, index_comment);
    }
    if (!mylite_ascii_case_equal(index_name, "PRIMARY") &&
        mylite_ascii_case_equal(is_visible, "NO")) {
        sqlite3_str_appendall(create_sql, " /*!80000 INVISIBLE */");
    }
    return sqlite3_str_errcode(create_sql) == SQLITE_OK ? MYLITE_OK : MYLITE_NOMEM;
}
