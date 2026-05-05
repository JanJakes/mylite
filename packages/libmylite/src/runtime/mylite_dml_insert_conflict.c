#include "mylite_dml_insert_conflict.h"

#include "mylite_diagnostics.h"
#include "mylite_dml_insert_bound_value.h"
#include "mylite_dml_insert_diagnostics.h"
#include "mylite_dml_insert_sqlite_bind.h"
#include "sqlite3.h"

#include <string.h>

static int insert_unique_index_conflicts(mylite_db *database,
                                         const struct mylite_insert_table *table,
                                         const struct mylite_insert_unique_index *index,
                                         const struct mylite_insert_bound_value *values,
                                         bool *out_conflicts);
static int insert_unique_index_conflict_rowid(mylite_db *database,
                                              const struct mylite_insert_table *table,
                                              const struct mylite_insert_unique_index *index,
                                              const struct mylite_insert_bound_value *values,
                                              sqlite3_int64 excluded_rowid, bool has_excluded_rowid,
                                              sqlite3_int64 *out_rowid, bool *out_conflicts);
static char *build_insert_unique_conflict_sql(mylite_db *database,
                                              const struct mylite_insert_table *table,
                                              const struct mylite_insert_unique_index *index,
                                              bool has_excluded_rowid);
static int bind_insert_unique_conflict_values(mylite_db *database, sqlite3_stmt *check,
                                              const struct mylite_insert_unique_index *index,
                                              const struct mylite_insert_bound_value *values,
                                              sqlite3_int64 excluded_rowid,
                                              bool has_excluded_rowid);
static char *build_insert_unique_check_sql(mylite_db *database,
                                           const struct mylite_insert_table *table,
                                           const struct mylite_insert_unique_index *index);
static int bind_insert_unique_check_values(mylite_db *database, sqlite3_stmt *check,
                                           const struct mylite_insert_unique_index *index,
                                           const struct mylite_insert_bound_value *values);
static char *build_insert_conflict_row_sql(mylite_db *database,
                                           const struct mylite_insert_table *table);

int mylite_dml_validate_insert_unique_indexes(mylite_db *database, const char *table_name,
                                              bool ignore, const struct mylite_insert_table *table,
                                              const struct mylite_insert_bound_value *values,
                                              struct mylite_insert_execution_state *state,
                                              bool *out_ignored)
{
    if (database == NULL || table_name == NULL || table == NULL || values == NULL ||
        state == NULL || out_ignored == NULL) {
        return MYLITE_MISUSE;
    }

    *out_ignored = false;
    for (size_t index = 0U; index < table->unique_index_count; ++index) {
        bool conflicts = false;
        int status = insert_unique_index_conflicts(database, table, &table->unique_indexes[index],
                                                   values, &conflicts);

        if (status != MYLITE_OK) {
            return status;
        }
        if (conflicts) {
            if (ignore) {
                ++state->duplicate_count;
                *out_ignored = true;
                return mylite_dml_insert_append_duplicate_entry_warning(
                    database, table_name, &table->unique_indexes[index], values);
            }
            return mylite_dml_insert_set_duplicate_entry_error(
                database, table_name, &table->unique_indexes[index], values);
        }
    }
    return MYLITE_OK;
}

int mylite_dml_find_insert_unique_conflict(mylite_db *database,
                                           const struct mylite_insert_table *table,
                                           const struct mylite_insert_bound_value *values,
                                           struct mylite_insert_unique_conflict *out_conflict)
{
    if (database == NULL || table == NULL || values == NULL || out_conflict == NULL) {
        return MYLITE_MISUSE;
    }

    *out_conflict = (struct mylite_insert_unique_conflict){0};
    for (size_t index = 0U; index < table->unique_index_count; ++index) {
        bool conflicts = false;
        sqlite3_int64 rowid = 0;
        int status = insert_unique_index_conflict_rowid(
            database, table, &table->unique_indexes[index], values, 0, false, &rowid, &conflicts);

        if (status != MYLITE_OK) {
            return status;
        }
        if (conflicts) {
            *out_conflict = (struct mylite_insert_unique_conflict){
                .index = &table->unique_indexes[index],
                .rowid = rowid,
                .conflicts = true,
            };
            return MYLITE_OK;
        }
    }
    return MYLITE_OK;
}

int mylite_dml_validate_insert_update_unique_indexes(mylite_db *database, const char *table_name,
                                                     bool ignore,
                                                     const struct mylite_insert_table *table,
                                                     const struct mylite_insert_bound_value *values,
                                                     sqlite3_int64 rowid, bool *out_conflicts)
{
    if (database == NULL || table_name == NULL || table == NULL || values == NULL ||
        out_conflicts == NULL) {
        return MYLITE_MISUSE;
    }

    *out_conflicts = false;
    for (size_t index = 0U; index < table->unique_index_count; ++index) {
        bool conflicts = false;
        sqlite3_int64 conflict_rowid = 0;
        int status =
            insert_unique_index_conflict_rowid(database, table, &table->unique_indexes[index],
                                               values, rowid, true, &conflict_rowid, &conflicts);

        (void)conflict_rowid;
        if (status != MYLITE_OK) {
            return status;
        }
        if (conflicts) {
            *out_conflicts = true;
            if (ignore) {
                return mylite_dml_insert_append_duplicate_entry_warning(
                    database, table_name, &table->unique_indexes[index], values);
            }
            return mylite_dml_insert_set_duplicate_entry_error(
                database, table_name, &table->unique_indexes[index], values);
        }
    }
    return MYLITE_OK;
}

int mylite_dml_load_insert_conflict_row(mylite_db *database,
                                        const struct mylite_insert_table *table,
                                        sqlite3_int64 rowid,
                                        struct mylite_insert_bound_value *values)
{
    sqlite3_stmt *select = NULL;
    char *sql = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (database == NULL || table == NULL || values == NULL) {
        return MYLITE_MISUSE;
    }

    sql = build_insert_conflict_row_sql(database, table);
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    rc = sqlite3_bind_int64(select, 1, rowid);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(select);
        return mylite_diagnostics_set_sqlite_error(database);
    }

    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW) {
        for (size_t index = 0U; index < table->column_count; ++index) {
            if (mylite_dml_copy_insert_sqlite_column_value(select, (int)index, &values[index]) !=
                0) {
                status = MYLITE_NOMEM;
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                break;
            }
        }
    } else {
        status =
            rc == SQLITE_DONE ? MYLITE_EXEC_ERROR : mylite_diagnostics_set_sqlite_error(database);
        if (status == MYLITE_EXEC_ERROR) {
            (void)mylite_diagnostics_set_error_message(database,
                                                       "Duplicate row disappeared during INSERT");
        }
    }

    sqlite3_finalize(select);
    return status;
}

static int insert_unique_index_conflicts(mylite_db *database,
                                         const struct mylite_insert_table *table,
                                         const struct mylite_insert_unique_index *index,
                                         const struct mylite_insert_bound_value *values,
                                         bool *out_conflicts)
{
    char *sql = NULL;
    sqlite3_stmt *check = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    *out_conflicts = false;
    for (size_t part = 0U; part < index->column_count; ++part) {
        if (values[index->column_indexes[part]].kind == MYLITE_INSERT_BOUND_NULL) {
            return MYLITE_OK;
        }
    }

    sql = build_insert_unique_check_sql(database, table, index);
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &check, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    status = bind_insert_unique_check_values(database, check, index, values);
    if (status == MYLITE_OK) {
        rc = sqlite3_step(check);
        if (rc == SQLITE_ROW) {
            *out_conflicts = true;
        } else if (rc != SQLITE_DONE) {
            status = mylite_diagnostics_set_sqlite_error(database);
        }
    }
    sqlite3_finalize(check);
    return status;
}

static int insert_unique_index_conflict_rowid(mylite_db *database,
                                              const struct mylite_insert_table *table,
                                              const struct mylite_insert_unique_index *index,
                                              const struct mylite_insert_bound_value *values,
                                              sqlite3_int64 excluded_rowid, bool has_excluded_rowid,
                                              sqlite3_int64 *out_rowid, bool *out_conflicts)
{
    char *sql = NULL;
    sqlite3_stmt *check = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    *out_rowid = 0;
    *out_conflicts = false;
    for (size_t part = 0U; part < index->column_count; ++part) {
        if (values[index->column_indexes[part]].kind == MYLITE_INSERT_BOUND_NULL) {
            return MYLITE_OK;
        }
    }

    sql = build_insert_unique_conflict_sql(database, table, index, has_excluded_rowid);
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &check, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    status = bind_insert_unique_conflict_values(database, check, index, values, excluded_rowid,
                                                has_excluded_rowid);
    if (status == MYLITE_OK) {
        rc = sqlite3_step(check);
        if (rc == SQLITE_ROW) {
            *out_conflicts = true;
            *out_rowid = sqlite3_column_int64(check, 0);
        } else if (rc != SQLITE_DONE) {
            status = mylite_diagnostics_set_sqlite_error(database);
        }
    }
    sqlite3_finalize(check);
    return status;
}

static char *build_insert_unique_conflict_sql(mylite_db *database,
                                              const struct mylite_insert_table *table,
                                              const struct mylite_insert_unique_index *index,
                                              bool has_excluded_rowid)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "SELECT rowid FROM \"%w\" WHERE ", table->physical_name);
    for (size_t part = 0U; part < index->column_count; ++part) {
        size_t column_index = index->column_indexes[part];

        if (part != 0U) {
            sqlite3_str_append(sql, " AND ", (int)strlen(" AND "));
        }
        if (index->prefix_lengths[part] != 0U) {
            sqlite3_str_appendf(sql, "substr(\"%w\",1,%llu) = substr(?,1,%llu)",
                                table->columns[column_index].name,
                                (unsigned long long)index->prefix_lengths[part],
                                (unsigned long long)index->prefix_lengths[part]);
        } else {
            sqlite3_str_appendf(sql, "\"%w\" = ?", table->columns[column_index].name);
        }
    }
    if (has_excluded_rowid) {
        sqlite3_str_append(sql, " AND rowid <> ?", (int)strlen(" AND rowid <> ?"));
    }
    sqlite3_str_append(sql, " LIMIT 1", (int)strlen(" LIMIT 1"));
    return sqlite3_str_finish(sql);
}

static int bind_insert_unique_conflict_values(mylite_db *database, sqlite3_stmt *check,
                                              const struct mylite_insert_unique_index *index,
                                              const struct mylite_insert_bound_value *values,
                                              sqlite3_int64 excluded_rowid, bool has_excluded_rowid)
{
    for (size_t part = 0U; part < index->column_count; ++part) {
        int rc = mylite_dml_bind_insert_bound_value(check, (int)part + 1,
                                                    &values[index->column_indexes[part]]);

        if (rc != SQLITE_OK) {
            return mylite_diagnostics_set_sqlite_error(database);
        }
    }
    if (has_excluded_rowid) {
        int rc = sqlite3_bind_int64(check, (int)index->column_count + 1, excluded_rowid);

        if (rc != SQLITE_OK) {
            return mylite_diagnostics_set_sqlite_error(database);
        }
    }
    return MYLITE_OK;
}

static char *build_insert_unique_check_sql(mylite_db *database,
                                           const struct mylite_insert_table *table,
                                           const struct mylite_insert_unique_index *index)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "SELECT 1 FROM \"%w\" WHERE ", table->physical_name);
    for (size_t part = 0U; part < index->column_count; ++part) {
        size_t column_index = index->column_indexes[part];

        if (part != 0U) {
            sqlite3_str_append(sql, " AND ", (int)strlen(" AND "));
        }
        if (index->prefix_lengths[part] != 0U) {
            sqlite3_str_appendf(sql, "substr(\"%w\",1,%llu) = substr(?,1,%llu)",
                                table->columns[column_index].name,
                                (unsigned long long)index->prefix_lengths[part],
                                (unsigned long long)index->prefix_lengths[part]);
        } else {
            sqlite3_str_appendf(sql, "\"%w\" = ?", table->columns[column_index].name);
        }
    }
    sqlite3_str_append(sql, " LIMIT 1", (int)strlen(" LIMIT 1"));
    return sqlite3_str_finish(sql);
}

static int bind_insert_unique_check_values(mylite_db *database, sqlite3_stmt *check,
                                           const struct mylite_insert_unique_index *index,
                                           const struct mylite_insert_bound_value *values)
{
    for (size_t part = 0U; part < index->column_count; ++part) {
        int rc = mylite_dml_bind_insert_bound_value(check, (int)part + 1,
                                                    &values[index->column_indexes[part]]);

        if (rc != SQLITE_OK) {
            return mylite_diagnostics_set_sqlite_error(database);
        }
    }
    return MYLITE_OK;
}

static char *build_insert_conflict_row_sql(mylite_db *database,
                                           const struct mylite_insert_table *table)
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
    sqlite3_str_appendf(sql, " FROM \"%w\" WHERE rowid = ?", table->physical_name);
    return sqlite3_str_finish(sql);
}
