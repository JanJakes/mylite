#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select_types.h"
#include "sql/mylite_expression.h"
#include "sqlite3.h"

#include <string.h>

static int update_unique_index_conflicts(mylite_db *database,
                                         const struct mylite_select_table *table,
                                         const struct mylite_insert_table *write_table,
                                         const struct mylite_insert_unique_index *index,
                                         const struct mylite_update_row *candidate,
                                         bool *out_conflicts);
static int set_update_duplicate_entry_error(mylite_db *database, const char *table_name,
                                            const struct mylite_insert_unique_index *index,
                                            const struct mylite_update_row *candidate);
static char *copy_update_duplicate_entry_value(const struct mylite_insert_unique_index *index,
                                               const struct mylite_update_row *candidate);

int mylite_dml_validate_update_unique_indexes(mylite_db *database,
                                              const struct mylite_select_table *table,
                                              const struct mylite_insert_table *write_table,
                                              const struct mylite_update_row *candidate)
{
    if (database == NULL || table == NULL || write_table == NULL || candidate == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < write_table->unique_index_count; ++index) {
        bool conflicts = false;
        int status = update_unique_index_conflicts(database, table, write_table,
                                                   &write_table->unique_indexes[index], candidate,
                                                   &conflicts);

        if (status != MYLITE_OK) {
            return status;
        }
        if (conflicts) {
            return set_update_duplicate_entry_error(database, table->table_name,
                                                    &write_table->unique_indexes[index], candidate);
        }
    }
    return MYLITE_OK;
}

static int update_unique_index_conflicts(mylite_db *database,
                                         const struct mylite_select_table *table,
                                         const struct mylite_insert_table *write_table,
                                         const struct mylite_insert_unique_index *index,
                                         const struct mylite_update_row *candidate,
                                         bool *out_conflicts)
{
    char *sql = NULL;
    sqlite3_stmt *check = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    *out_conflicts = false;
    for (size_t part = 0U; part < index->column_count; ++part) {
        if (candidate->values[index->column_indexes[part]].kind == MYLITE_EXPRESSION_VALUE_NULL) {
            return MYLITE_OK;
        }
    }

    sql = mylite_dml_build_update_unique_check_sql(database, table, write_table, index);
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &check, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    status = mylite_dml_bind_update_unique_check_values(database, check, index, candidate);
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

static int set_update_duplicate_entry_error(mylite_db *database, const char *table_name,
                                            const struct mylite_insert_unique_index *index,
                                            const struct mylite_update_row *candidate)
{
    char *entry = copy_update_duplicate_entry_value(index, candidate);
    char *message = NULL;
    int status = MYLITE_OK;

    if (entry == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    message =
        sqlite3_mprintf("Duplicate entry '%q' for key '%q.%q'", entry, table_name, index->name);
    sqlite3_free(entry);
    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static char *copy_update_duplicate_entry_value(const struct mylite_insert_unique_index *index,
                                               const struct mylite_update_row *candidate)
{
    sqlite3_str *text = sqlite3_str_new(NULL);

    if (text == NULL) {
        return NULL;
    }

    for (size_t part = 0U; part < index->column_count; ++part) {
        const struct mylite_expression_value *value =
            &candidate->values[index->column_indexes[part]];

        if (part != 0U) {
            sqlite3_str_append(text, "-", 1);
        }
        switch (value->kind) {
        case MYLITE_EXPRESSION_VALUE_NULL:
            sqlite3_str_append(text, "NULL", (int)strlen("NULL"));
            break;
        case MYLITE_EXPRESSION_VALUE_INT64:
            sqlite3_str_appendf(text, "%lld", (long long)value->int64_value);
            break;
        case MYLITE_EXPRESSION_VALUE_UINT64:
            sqlite3_str_appendf(text, "%llu", (unsigned long long)value->uint64_value);
            break;
        case MYLITE_EXPRESSION_VALUE_REAL:
            sqlite3_str_appendf(text, "%.15g", value->real_value);
            break;
        case MYLITE_EXPRESSION_VALUE_TEXT:
            sqlite3_str_append(text, value->text_value == NULL ? "" : value->text_value,
                               value->text_value == NULL ? 0 : (int)value->text_length);
            break;
        }
    }
    return sqlite3_str_finish(text);
}
