#include "mylite_select_sql.h"

#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_types.h"
#include "sqlite3.h"

#include <stdio.h>
#include <string.h>

char *mylite_select_build_physical_sql(mylite_db *database, const struct mylite_select_plan *plan)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_append(sql, "SELECT ", (int)strlen("SELECT "));
    for (size_t index = 0U; index < plan->output_count; ++index) {
        const struct mylite_select_output_column *output = &plan->outputs[index];
        const struct mylite_select_column *column =
            mylite_select_plan_column_const(plan, output->column_index, NULL);

        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        if (column == NULL) {
            sqlite3_free(sqlite3_str_finish(sql));
            return NULL;
        }
        sqlite3_str_appendf(sql, "\"%w\" AS \"%w\"", column->name, output->label);
    }
    sqlite3_str_appendf(sql, " FROM \"%w\"",
                        mylite_select_plan_table_const(plan, 0U)->physical_name);
    return sqlite3_str_finish(sql);
}

char *mylite_select_build_scan_sql(mylite_db *database, const struct mylite_select_plan *plan)
{
    enum { sqlite_table_alias_size = 32 };
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_append(sql, "SELECT ", (int)strlen("SELECT "));
    for (size_t table_index = 0U; table_index < mylite_select_plan_table_count(plan);
         ++table_index) {
        const struct mylite_select_table *table = mylite_select_plan_table_const(plan, table_index);
        char table_alias[sqlite_table_alias_size];
        int alias_length = snprintf(table_alias, sizeof(table_alias), "_m%zu", table_index);

        if (table == NULL || alias_length < 0 || (size_t)alias_length >= sizeof(table_alias)) {
            sqlite3_free(sqlite3_str_finish(sql));
            return NULL;
        }
        for (size_t column_index = 0U; column_index < table->column_count; ++column_index) {
            const struct mylite_select_column *column = &table->columns[column_index];

            if (table_index != 0U || column_index != 0U) {
                sqlite3_str_append(sql, ",", 1);
            }
            sqlite3_str_appendf(sql, "\"%w\".\"%w\"", table_alias, column->name);
        }
    }
    sqlite3_str_append(sql, " FROM ", (int)strlen(" FROM "));
    for (size_t table_index = 0U; table_index < mylite_select_plan_table_count(plan);
         ++table_index) {
        const struct mylite_select_table *table = mylite_select_plan_table_const(plan, table_index);
        char table_alias[sqlite_table_alias_size];
        int alias_length = snprintf(table_alias, sizeof(table_alias), "_m%zu", table_index);

        if (table == NULL || alias_length < 0 || (size_t)alias_length >= sizeof(table_alias)) {
            sqlite3_free(sqlite3_str_finish(sql));
            return NULL;
        }
        if (table_index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "\"%w\" AS \"%w\"", table->physical_name, table_alias);
    }
    return sqlite3_str_finish(sql);
}

char *mylite_select_build_table_scan_sql(mylite_db *database,
                                         const struct mylite_select_table *table)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_append(sql, "SELECT ", (int)strlen("SELECT "));
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "\"%w\"", table->columns[index].name);
    }
    sqlite3_str_appendf(sql, " FROM \"%w\"", table->physical_name);
    return sqlite3_str_finish(sql);
}

char *mylite_select_build_table_rowid_scan_sql(mylite_db *database,
                                               const struct mylite_select_table *table)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_append(sql, "SELECT rowid", (int)strlen("SELECT rowid"));
    for (size_t index = 0U; index < table->column_count; ++index) {
        sqlite3_str_append(sql, ",", 1);
        sqlite3_str_appendf(sql, "\"%w\"", table->columns[index].name);
    }
    sqlite3_str_appendf(sql, " FROM \"%w\"", table->physical_name);
    return sqlite3_str_finish(sql);
}
