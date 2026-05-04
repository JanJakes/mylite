#include "mylite_select_row_loader.h"

#include "mylite_diagnostics.h"
#include "mylite_field_descriptor.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_rowset.h"
#include "mylite_select_sql.h"
#include "mylite_sqlite_value.h"
#include "sqlite3.h"

#include <stdlib.h>

static int load_table_select_join_rowset(mylite_stmt *stmt, size_t table_index,
                                         struct mylite_table_select_table_rowset *rowset);
static int append_table_select_join_scan_row(mylite_stmt *stmt, sqlite3_stmt *scan,
                                             const struct mylite_select_table *table,
                                             struct mylite_table_select_table_rowset *rowset);

int mylite_select_load_join_rowsets(mylite_stmt *stmt,
                                    struct mylite_table_select_table_rowset *rowsets)
{
    size_t table_count = mylite_select_plan_table_count(&stmt->select_plan);

    for (size_t table_index = 0U; table_index < table_count; ++table_index) {
        int status = load_table_select_join_rowset(stmt, table_index, &rowsets[table_index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_select_copy_sqlite_row(mylite_stmt *stmt, struct mylite_table_select_row *out_row)
{
    size_t column_count = mylite_select_plan_column_count(&stmt->select_plan);

    if (column_count == 0U) {
        *out_row = (struct mylite_table_select_row){0};
        return MYLITE_OK;
    }
    out_row->values = calloc(column_count, sizeof(*out_row->values));
    if (out_row->values == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_row->value_count = column_count;

    for (size_t index = 0U; index < column_count; ++index) {
        int status =
            mylite_select_copy_current_sqlite_column_value(stmt, index, &out_row->values[index]);

        if (status != 0) {
            mylite_select_row_deinit(out_row);
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

int mylite_select_copy_current_sqlite_column_value(mylite_stmt *stmt, size_t column_index,
                                                   struct mylite_expression_value *out_value)
{
    const struct mylite_select_column *column = NULL;
    int status = 0;

    if (stmt == NULL || stmt->sqlite_stmt == NULL ||
        column_index >= mylite_select_plan_column_count(&stmt->select_plan)) {
        return -1;
    }

    status = mylite_sqlite_copy_column_value(stmt->sqlite_stmt, column_index, out_value);
    if (status == 0) {
        column = mylite_select_plan_column_const(&stmt->select_plan, column_index, NULL);
        out_value->preserve_temporal_fraction_digits =
            mylite_field_descriptor_preserves_temporal_fraction_digits(
                column == NULL ? NULL : &column->descriptor);
        out_value->temporal_type = mylite_field_descriptor_expression_temporal_type(
            column == NULL ? NULL : &column->descriptor);
    }
    return status;
}

static int load_table_select_join_rowset(mylite_stmt *stmt, size_t table_index,
                                         struct mylite_table_select_table_rowset *rowset)
{
    const struct mylite_select_table *table =
        mylite_select_plan_table_const(&stmt->select_plan, table_index);
    sqlite3_stmt *scan = NULL;
    char *scan_sql = NULL;
    int rc = SQLITE_OK;

    if (table == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    scan_sql = mylite_select_build_table_scan_sql(stmt->database, table);
    if (scan_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(stmt->database->sqlite, scan_sql, -1, SQLITE_PREPARE_PERSISTENT, &scan,
                            NULL);
    sqlite3_free(scan_sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(stmt->database);
    }

    while ((rc = sqlite3_step(scan)) == SQLITE_ROW) {
        int status = append_table_select_join_scan_row(stmt, scan, table, rowset);

        if (status != MYLITE_OK) {
            sqlite3_finalize(scan);
            return status;
        }
    }
    sqlite3_finalize(scan);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(stmt->database);
}

static int append_table_select_join_scan_row(mylite_stmt *stmt, sqlite3_stmt *scan,
                                             const struct mylite_select_table *table,
                                             struct mylite_table_select_table_rowset *rowset)
{
    struct mylite_table_select_row row = {
        .value_count = table->column_count,
    };
    struct mylite_table_select_row *rows = NULL;

    if (row.value_count != 0U) {
        row.values = calloc(row.value_count, sizeof(*row.values));
        if (row.values == NULL) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }

    for (size_t index = 0U; index < row.value_count; ++index) {
        if (mylite_sqlite_copy_column_value(scan, index, &row.values[index]) != 0) {
            mylite_select_row_deinit(&row);
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }

    rows = realloc(rowset->rows, (rowset->row_count + 1U) * sizeof(*rowset->rows));
    if (rows == NULL) {
        mylite_select_row_deinit(&row);
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    rowset->rows = rows;
    rowset->rows[rowset->row_count++] = row;
    return MYLITE_OK;
}
