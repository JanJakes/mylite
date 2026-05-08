#include "mylite_dml_insert_unique_probe.h"

#include "mylite_diagnostics.h"
#include "mylite_dml_insert_sqlite_bind.h"
#include "mylite_text_compare.h"
#include "sqlite3.h"

#include <string.h>

struct insert_unique_text_compare {
    bool case_insensitive;
    bool pad_space;
};

static char *build_insert_unique_conflict_sql(
    mylite_db *database,
    const struct mylite_insert_table *table,
    const struct mylite_insert_unique_index *index,
    bool has_excluded_rowid
);

static int bind_insert_unique_conflict_values(
    mylite_db *database,
    sqlite3_stmt *check,
    const struct mylite_insert_unique_index *index,
    const struct mylite_insert_bound_value *values,
    sqlite3_int64 excluded_rowid,
    bool has_excluded_rowid
);

static void append_insert_unique_part_comparison(
    sqlite3_str *sql,
    const struct mylite_insert_table_column *column,
    uint64_t prefix_length
);

static void append_insert_unique_stored_part(
    sqlite3_str *sql,
    const struct mylite_insert_table_column *column,
    uint64_t prefix_length,
    struct insert_unique_text_compare compare
);

static void append_insert_unique_bound_part(
    sqlite3_str *sql,
    uint64_t prefix_length,
    struct insert_unique_text_compare compare
);

static struct insert_unique_text_compare insert_unique_column_text_compare(
    const struct mylite_insert_table_column *column
);

static char *build_insert_unique_check_sql(
    mylite_db *database,
    const struct mylite_insert_table *table,
    const struct mylite_insert_unique_index *index
);

static int bind_insert_unique_check_values(
    mylite_db *database,
    sqlite3_stmt *check,
    const struct mylite_insert_unique_index *index,
    const struct mylite_insert_bound_value *values
);

int mylite_dml_insert_unique_index_conflicts(
    mylite_db *database,
    const struct mylite_insert_table *table,
    const struct mylite_insert_unique_index *index,
    const struct mylite_insert_bound_value *values,
    bool *out_conflicts
) {
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

int mylite_dml_insert_unique_index_conflict_rowid(
    mylite_db *database,
    const struct mylite_insert_table *table,
    const struct mylite_insert_unique_index *index,
    const struct mylite_insert_bound_value *values,
    sqlite3_int64 excluded_rowid,
    bool has_excluded_rowid,
    sqlite3_int64 *out_rowid,
    bool *out_conflicts
) {
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

    status = bind_insert_unique_conflict_values(
        database,
        check,
        index,
        values,
        excluded_rowid,
        has_excluded_rowid
    );
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

static char *build_insert_unique_conflict_sql(
    mylite_db *database,
    const struct mylite_insert_table *table,
    const struct mylite_insert_unique_index *index,
    bool has_excluded_rowid
) {
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
        append_insert_unique_part_comparison(
            sql,
            &table->columns[column_index],
            index->prefix_lengths[part]
        );
    }
    if (has_excluded_rowid) {
        sqlite3_str_append(sql, " AND rowid <> ?", (int)strlen(" AND rowid <> ?"));
    }
    sqlite3_str_append(sql, " LIMIT 1", (int)strlen(" LIMIT 1"));
    return sqlite3_str_finish(sql);
}

static void append_insert_unique_part_comparison(
    sqlite3_str *sql,
    const struct mylite_insert_table_column *column,
    uint64_t prefix_length
) {
    struct insert_unique_text_compare compare = insert_unique_column_text_compare(column);

    append_insert_unique_stored_part(sql, column, prefix_length, compare);
    sqlite3_str_append(sql, " = ", (int)strlen(" = "));
    append_insert_unique_bound_part(sql, prefix_length, compare);
}

static void append_insert_unique_stored_part(
    sqlite3_str *sql,
    const struct mylite_insert_table_column *column,
    uint64_t prefix_length,
    struct insert_unique_text_compare compare
) {
    if (compare.case_insensitive) {
        sqlite3_str_append(sql, "lower(", (int)strlen("lower("));
    }
    if (compare.pad_space) {
        sqlite3_str_append(sql, "rtrim(", (int)strlen("rtrim("));
    }
    if (prefix_length != 0U) {
        sqlite3_str_appendf(
            sql,
            "substr(\"%w\",1,%llu)",
            column->name,
            (unsigned long long)prefix_length
        );
    } else {
        sqlite3_str_appendf(sql, "\"%w\"", column->name);
    }
    if (compare.pad_space) {
        sqlite3_str_append(sql, ",' ')", (int)strlen(",' ')"));
    }
    if (compare.case_insensitive) {
        sqlite3_str_append(sql, ")", 1);
    }
}

static void append_insert_unique_bound_part(
    sqlite3_str *sql,
    uint64_t prefix_length,
    struct insert_unique_text_compare compare
) {
    if (compare.case_insensitive) {
        sqlite3_str_append(sql, "lower(", (int)strlen("lower("));
    }
    if (compare.pad_space) {
        sqlite3_str_append(sql, "rtrim(", (int)strlen("rtrim("));
    }
    if (prefix_length != 0U) {
        sqlite3_str_appendf(sql, "substr(?,1,%llu)", (unsigned long long)prefix_length);
    } else {
        sqlite3_str_append(sql, "?", 1);
    }
    if (compare.pad_space) {
        sqlite3_str_append(sql, ",' ')", (int)strlen(",' ')"));
    }
    if (compare.case_insensitive) {
        sqlite3_str_append(sql, ")", 1);
    }
}

static struct insert_unique_text_compare insert_unique_column_text_compare(
    const struct mylite_insert_table_column *column
) {
    if (column == NULL) {
        return (struct insert_unique_text_compare){0};
    }
    return (struct insert_unique_text_compare){
        .case_insensitive = mylite_column_definition_uses_case_insensitive_text_compare(
            column->data_type,
            column->character_set_name,
            column->collation_name
        ),
        .pad_space = mylite_column_definition_uses_pad_space_text_compare(
            column->data_type,
            column->character_set_name,
            column->collation_name
        ),
    };
}

static int bind_insert_unique_conflict_values(
    mylite_db *database,
    sqlite3_stmt *check,
    const struct mylite_insert_unique_index *index,
    const struct mylite_insert_bound_value *values,
    sqlite3_int64 excluded_rowid,
    bool has_excluded_rowid
) {
    for (size_t part = 0U; part < index->column_count; ++part) {
        int rc = mylite_dml_bind_insert_bound_value(
            check,
            (int)part + 1,
            &values[index->column_indexes[part]]
        );

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

static char *build_insert_unique_check_sql(
    mylite_db *database,
    const struct mylite_insert_table *table,
    const struct mylite_insert_unique_index *index
) {
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
        append_insert_unique_part_comparison(
            sql,
            &table->columns[column_index],
            index->prefix_lengths[part]
        );
    }
    sqlite3_str_append(sql, " LIMIT 1", (int)strlen(" LIMIT 1"));
    return sqlite3_str_finish(sql);
}

static int bind_insert_unique_check_values(
    mylite_db *database,
    sqlite3_stmt *check,
    const struct mylite_insert_unique_index *index,
    const struct mylite_insert_bound_value *values
) {
    for (size_t part = 0U; part < index->column_count; ++part) {
        int rc = mylite_dml_bind_insert_bound_value(
            check,
            (int)part + 1,
            &values[index->column_indexes[part]]
        );

        if (rc != SQLITE_OK) {
            return mylite_diagnostics_set_sqlite_error(database);
        }
    }
    return MYLITE_OK;
}
