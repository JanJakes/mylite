#include "mylite_dml.h"

#include "mylite_runtime.h"
#include "mylite_select_types.h"
#include "sqlite3.h"

#include <string.h>

char *mylite_dml_build_update_scan_sql(mylite_db *database, const struct mylite_select_table *table)
{
    sqlite3_str *sql = NULL;

    if (database == NULL || table == NULL) {
        return NULL;
    }

    sql = sqlite3_str_new(database->sqlite);
    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_append(sql, "SELECT rowid", (int)strlen("SELECT rowid"));
    for (size_t index = 0U; index < table->column_count; ++index) {
        sqlite3_str_appendf(sql, ",\"%w\"", table->columns[index].name);
    }
    sqlite3_str_appendf(sql, " FROM \"%w\"", table->physical_name);
    return sqlite3_str_finish(sql);
}

char *mylite_dml_build_update_physical_sql(mylite_db *database,
                                           const struct mylite_select_table *table)
{
    sqlite3_str *sql = NULL;

    if (database == NULL || table == NULL) {
        return NULL;
    }

    sql = sqlite3_str_new(database->sqlite);
    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "UPDATE \"%w\" SET ", table->physical_name);
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "\"%w\" = ?", table->columns[index].name);
    }
    sqlite3_str_append(sql, " WHERE rowid = ?", (int)strlen(" WHERE rowid = ?"));
    return sqlite3_str_finish(sql);
}
