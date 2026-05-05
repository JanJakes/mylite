#include "mylite_select_catalog.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_catalog_descriptor.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static int load_select_column_from_catalog_row(mylite_db *database,
                                               struct mylite_select_table *table,
                                               sqlite3_stmt *select);
static bool select_column_extra_is_visible(const char *extra);
static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_select_load_table_columns(mylite_db *database, struct mylite_select_table *table)
{
    sqlite3_stmt *select = NULL;
    static const char sql[] =
        "SELECT column_name, extra, is_nullable, data_type, character_octet_length, "
        "numeric_precision, numeric_scale, datetime_precision, collation_name, column_type, "
        "column_key, column_default FROM __mylite_column_catalog WHERE table_schema = ? "
        "AND table_name = ? ORDER BY ordinal_position";
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(select, 1, table->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, table->table_name, -1, sqlite_transient_destructor());
    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        int status = load_select_column_from_catalog_row(database, table, select);

        if (status != MYLITE_OK) {
            sqlite3_finalize(select);
            if (status == MYLITE_NOMEM) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
            }
            return status;
        }
    }
    sqlite3_finalize(select);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    if (table->column_count == 0U) {
        return mylite_diagnostics_set_table_doesnt_exist_error(database, table->schema_name,
                                                               table->table_name);
    }
    return MYLITE_OK;
}

static int load_select_column_from_catalog_row(mylite_db *database,
                                               struct mylite_select_table *table,
                                               sqlite3_stmt *select)
{
    const char *name = (const char *)sqlite3_column_text(select, 0);
    const char *extra = (const char *)sqlite3_column_text(select, 1);
    struct mylite_select_column column = {
        .visible = select_column_extra_is_visible(extra),
    };
    struct mylite_select_column *columns = NULL;

    column.name = mylite_copy_span_text(name, name == NULL ? 0U : strlen(name));
    if (column.name == NULL) {
        return MYLITE_NOMEM;
    }
    int status = mylite_select_catalog_load_column_descriptor(database, select, &column.descriptor);
    if (status != MYLITE_OK) {
        mylite_select_column_deinit(&column);
        return status;
    }

    columns = realloc(table->columns, (table->column_count + 1U) * sizeof(*table->columns));
    if (columns == NULL) {
        mylite_select_column_deinit(&column);
        return MYLITE_NOMEM;
    }

    table->columns = columns;
    table->columns[table->column_count++] = column;
    return MYLITE_OK;
}

static bool select_column_extra_is_visible(const char *extra)
{
    if (extra == NULL) {
        return true;
    }
    if (strstr(extra, "INVISIBLE") == NULL) {
        return true;
    }
    return false;
}

static sqlite3_destructor_type sqlite_transient_destructor(void)
{
    // SQLite's public macro intentionally uses this sentinel pointer value.
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
