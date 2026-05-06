#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select_types.h"
#include "sql/mylite_expression.h"
#include "sqlite3.h"

#include <stdint.h>
#include <string.h>

static int bind_update_value(
    sqlite3_stmt *stmt,
    int index,
    const struct mylite_expression_value *value
);

static sqlite3_destructor_type sqlite_transient_destructor(void);

char *mylite_dml_build_update_scan_sql(
    mylite_db *database,
    const struct mylite_select_table *table
) {
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

char *mylite_dml_build_update_physical_sql(
    mylite_db *database,
    const struct mylite_select_table *table
) {
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

char *mylite_dml_build_update_unique_check_sql(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_insert_table *write_table,
    const struct mylite_insert_unique_index *index
) {
    sqlite3_str *sql = NULL;

    if (database == NULL || table == NULL || write_table == NULL || index == NULL) {
        return NULL;
    }

    sql = sqlite3_str_new(database->sqlite);
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
            sqlite3_str_appendf(
                sql,
                "substr(\"%w\",1,%llu) = substr(?,1,%llu)",
                write_table->columns[column_index].name,
                (unsigned long long)index->prefix_lengths[part],
                (unsigned long long)index->prefix_lengths[part]
            );
        } else {
            sqlite3_str_appendf(sql, "\"%w\" = ?", write_table->columns[column_index].name);
        }
    }
    sqlite3_str_append(sql, " AND rowid <> ? LIMIT 1", (int)strlen(" AND rowid <> ? LIMIT 1"));
    return sqlite3_str_finish(sql);
}

int mylite_dml_bind_update_unique_check_values(
    mylite_db *database,
    sqlite3_stmt *check,
    const struct mylite_insert_unique_index *index,
    const struct mylite_update_row *candidate
) {
    if (database == NULL || check == NULL || index == NULL || candidate == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t part = 0U; part < index->column_count; ++part) {
        int rc = bind_update_value(
            check,
            (int)part + 1,
            &candidate->values[index->column_indexes[part]]
        );

        if (rc != SQLITE_OK) {
            return mylite_diagnostics_set_sqlite_error(database);
        }
    }

    {
        int rc = sqlite3_bind_int64(check, (int)index->column_count + 1, candidate->rowid);

        if (rc != SQLITE_OK) {
            return mylite_diagnostics_set_sqlite_error(database);
        }
    }
    return MYLITE_OK;
}

int mylite_dml_bind_update_row_values(
    mylite_db *database,
    sqlite3_stmt *update,
    const struct mylite_update_row *candidate
) {
    if (database == NULL || update == NULL || candidate == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < candidate->value_count; ++index) {
        int rc = bind_update_value(update, (int)index + 1, &candidate->values[index]);

        if (rc != SQLITE_OK) {
            return mylite_diagnostics_set_sqlite_error(database);
        }
    }
    return MYLITE_OK;
}

static int bind_update_value(
    sqlite3_stmt *stmt,
    int index,
    const struct mylite_expression_value *value
) {
    if (stmt == NULL || value == NULL) {
        return SQLITE_MISUSE;
    }

    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_NULL:
        return sqlite3_bind_null(stmt, index);
    case MYLITE_EXPRESSION_VALUE_INT64:
        return sqlite3_bind_int64(stmt, index, value->int64_value);
    case MYLITE_EXPRESSION_VALUE_UINT64:
        if (value->uint64_value > (uint64_t)INT64_MAX) {
            return SQLITE_RANGE;
        }
        return sqlite3_bind_int64(stmt, index, (sqlite3_int64)value->uint64_value);
    case MYLITE_EXPRESSION_VALUE_REAL:
        return sqlite3_bind_double(stmt, index, value->real_value);
    case MYLITE_EXPRESSION_VALUE_TEXT:
        return sqlite3_bind_text(
            stmt,
            index,
            value->text_value,
            (int)value->text_length,
            sqlite_transient_destructor()
        );
    }
    return SQLITE_MISUSE;
}

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
