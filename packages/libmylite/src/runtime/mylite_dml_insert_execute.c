#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_dml_insert_conflict.h"
#include "mylite_dml_insert_set_row_resolve.h"
#include "mylite_dml_insert_sqlite_bind.h"
#include "sqlite3.h"

#include <stdlib.h>
#include <string.h>

static void record_insert_row_auto_increment_id(const struct mylite_insert_table *table,
                                                const struct mylite_insert_bound_value *values,
                                                struct mylite_insert_execution_state *state);
static char *build_insert_update_physical_sql(mylite_db *database,
                                              const struct mylite_insert_table *table);
static bool insert_bound_values_equal(const struct mylite_insert_bound_value *left,
                                      const struct mylite_insert_bound_value *right);

int mylite_dml_initialize_insert_ignore_warning_state(mylite_db *database,
                                                      const struct mylite_insert_values_plan *plan,
                                                      const struct mylite_insert_table *table,
                                                      struct mylite_insert_execution_state *state)
{
    if (database == NULL || plan == NULL || table == NULL || state == NULL) {
        return MYLITE_MISUSE;
    }
    if (!plan->ignore || table->column_count == 0U) {
        return MYLITE_OK;
    }

    state->warned_omitted_no_default_columns =
        calloc(table->column_count, sizeof(*state->warned_omitted_no_default_columns));
    state->warned_null_columns = calloc(table->column_count, sizeof(*state->warned_null_columns));
    if (state->warned_omitted_no_default_columns == NULL || state->warned_null_columns == NULL) {
        mylite_dml_insert_execution_state_deinit(state);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

void mylite_dml_insert_execution_state_deinit(struct mylite_insert_execution_state *state)
{
    if (state == NULL) {
        return;
    }

    free(state->warned_omitted_no_default_columns);
    state->warned_omitted_no_default_columns = NULL;
    free(state->warned_null_columns);
    state->warned_null_columns = NULL;
}

char *mylite_dml_build_insert_physical_sql(mylite_db *database,
                                           const struct mylite_insert_table *table)
{
    sqlite3_str *sql = NULL;

    if (database == NULL || table == NULL) {
        return NULL;
    }

    sql = sqlite3_str_new(database->sqlite);
    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "INSERT INTO \"%w\"(", table->physical_name);
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "\"%w\"", table->columns[index].name);
    }
    sqlite3_str_append(sql, ") VALUES(", (int)strlen(") VALUES("));
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_append(sql, "?", 1);
    }
    sqlite3_str_append(sql, ")", 1);
    return sqlite3_str_finish(sql);
}

char *mylite_dml_build_replace_delete_sql(mylite_db *database,
                                          const struct mylite_insert_table *table)
{
    sqlite3_str *sql = NULL;

    if (database == NULL || table == NULL) {
        return NULL;
    }

    sql = sqlite3_str_new(database->sqlite);
    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "DELETE FROM \"%w\" WHERE rowid = ?", table->physical_name);
    return sqlite3_str_finish(sql);
}

int mylite_dml_write_insert_candidate_row(mylite_db *database, sqlite3_stmt *insert,
                                          const struct mylite_insert_table *table,
                                          const struct mylite_insert_bound_value *values,
                                          struct mylite_insert_execution_state *state)
{
    int status = MYLITE_OK;
    int rc = SQLITE_OK;

    if (database == NULL || insert == NULL || table == NULL || values == NULL || state == NULL) {
        return MYLITE_MISUSE;
    }

    sqlite3_reset(insert);
    sqlite3_clear_bindings(insert);
    status = mylite_dml_bind_insert_row_values(database, insert, values, table->column_count);
    if (status == MYLITE_OK) {
        rc = sqlite3_step(insert);
        if (rc != SQLITE_DONE) {
            status = mylite_diagnostics_set_sqlite_error(database);
        }
    }
    if (status == MYLITE_OK) {
        ++state->accepted_row_count;
        record_insert_row_auto_increment_id(table, values, state);
        status = mylite_dml_advance_insert_row_auto_increment(table, values, state);
    }
    return status;
}

int mylite_dml_execute_insert_row(mylite_db *database, const struct mylite_insert_values_plan *plan,
                                  sqlite3_stmt *insert, const struct mylite_insert_table *table,
                                  const struct mylite_insert_row_column_indexes *column_indexes,
                                  struct mylite_insert_execution_state *state, size_t row_index)
{
    struct mylite_insert_bound_value *values = NULL;
    bool ignored = false;
    int status = MYLITE_OK;

    if (database == NULL || plan == NULL || insert == NULL || table == NULL ||
        column_indexes == NULL || state == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->column_count == 0U) {
        (void)mylite_diagnostics_set_error_message(database, "INSERT target table has no columns");
        return MYLITE_EXEC_ERROR;
    }

    values = calloc(table->column_count, sizeof(*values));
    if (values == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status =
        mylite_dml_resolve_insert_row_values(database, plan, table, column_indexes->insert_columns,
                                             plan->row_count, state, row_index, values);
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_insert_unique_indexes(database, plan->table_name, plan->ignore,
                                                           table, values, state, &ignored);
    }
    if (status == MYLITE_OK && !ignored) {
        status = mylite_dml_write_insert_candidate_row(database, insert, table, values, state);
    }

    mylite_dml_insert_bound_values_deinit(values, table->column_count);
    return status;
}

int mylite_dml_execute_insert_set_row(mylite_db *database, const char *schema_name,
                                      const struct mylite_insert_values_plan *values_plan,
                                      const struct mylite_insert_set_plan *set_plan,
                                      sqlite3_stmt *insert, const struct mylite_insert_table *table,
                                      const size_t *column_indexes, size_t column_index_count,
                                      struct mylite_insert_execution_state *state,
                                      struct mylite_insert_bound_value *values,
                                      struct mylite_insert_set_row_state *row_state)
{
    bool ignored = false;
    int status = MYLITE_OK;

    if (database == NULL || values_plan == NULL || set_plan == NULL || insert == NULL ||
        table == NULL || state == NULL || values == NULL || row_state == NULL) {
        return MYLITE_MISUSE;
    }

    status = mylite_dml_resolve_insert_set_row_values(database, schema_name, values_plan, set_plan,
                                                      table, column_indexes, column_index_count, 1U,
                                                      state, values, row_state);
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_insert_unique_indexes(
            database, values_plan->table_name, values_plan->ignore, table, values, state, &ignored);
    }
    if (status != MYLITE_OK || ignored) {
        return status;
    }

    return mylite_dml_write_insert_candidate_row(database, insert, table, values, state);
}

int mylite_dml_write_insert_update_candidate(mylite_db *database,
                                             const struct mylite_insert_table *table,
                                             sqlite3_int64 rowid,
                                             const struct mylite_insert_bound_value *values,
                                             struct mylite_insert_execution_state *state)
{
    sqlite3_stmt *update = NULL;
    char *sql = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (database == NULL || table == NULL || values == NULL || state == NULL) {
        return MYLITE_MISUSE;
    }

    sql = build_insert_update_physical_sql(database, table);
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    status = mylite_dml_bind_insert_row_values(database, update, values, table->column_count);
    if (status == MYLITE_OK) {
        rc = sqlite3_bind_int64(update, (int)table->column_count + 1, rowid);
        if (rc != SQLITE_OK) {
            status = mylite_diagnostics_set_sqlite_error(database);
        }
    }
    if (status == MYLITE_OK) {
        rc = sqlite3_step(update);
        if (rc != SQLITE_DONE) {
            status = mylite_diagnostics_set_sqlite_error(database);
        }
    }
    sqlite3_finalize(update);
    if (status == MYLITE_OK) {
        status = mylite_dml_advance_insert_row_auto_increment(table, values, state);
    }
    return status;
}

bool mylite_dml_insert_update_row_changed(const struct mylite_insert_bound_value *stored,
                                          const struct mylite_insert_bound_value *candidate,
                                          size_t value_count)
{
    for (size_t index = 0U; index < value_count; ++index) {
        if (!insert_bound_values_equal(&stored[index], &candidate[index])) {
            return true;
        }
    }
    return false;
}

int mylite_dml_advance_insert_row_auto_increment(const struct mylite_insert_table *table,
                                                 const struct mylite_insert_bound_value *values,
                                                 struct mylite_insert_execution_state *state)
{
    const struct mylite_insert_bound_value *auto_value = NULL;

    if (table == NULL || values == NULL || state == NULL) {
        return MYLITE_MISUSE;
    }
    if (!table->has_auto_increment) {
        return MYLITE_OK;
    }

    auto_value = &values[table->auto_increment_column_index];
    if (auto_value->kind == MYLITE_INSERT_BOUND_INTEGER && auto_value->integer_value > 0 &&
        (uint64_t)auto_value->integer_value >= state->next_auto_increment) {
        state->next_auto_increment = (uint64_t)auto_value->integer_value + 1U;
    }
    return MYLITE_OK;
}

static char *build_insert_update_physical_sql(mylite_db *database,
                                              const struct mylite_insert_table *table)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

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

static bool insert_bound_values_equal(const struct mylite_insert_bound_value *left,
                                      const struct mylite_insert_bound_value *right)
{
    if (left->kind != right->kind) {
        return false;
    }
    switch (left->kind) {
    case MYLITE_INSERT_BOUND_NULL:
        return true;
    case MYLITE_INSERT_BOUND_INTEGER:
        return left->integer_value == right->integer_value;
    case MYLITE_INSERT_BOUND_REAL:
        return left->real_value == right->real_value;
    case MYLITE_INSERT_BOUND_TEXT:
        if (left->text_value == NULL || right->text_value == NULL) {
            return left->text_value == right->text_value;
        }
        return strcmp(left->text_value, right->text_value) == 0;
    }
    return false;
}

static void record_insert_row_auto_increment_id(const struct mylite_insert_table *table,
                                                const struct mylite_insert_bound_value *values,
                                                struct mylite_insert_execution_state *state)
{
    const struct mylite_insert_bound_value *auto_value = NULL;

    if (!table->has_auto_increment || state->generated_insert_id) {
        return;
    }

    auto_value = &values[table->auto_increment_column_index];
    if (auto_value->generated_auto_increment && auto_value->kind == MYLITE_INSERT_BOUND_INTEGER &&
        auto_value->integer_value > 0) {
        state->first_insert_id = (uint64_t)auto_value->integer_value;
        state->generated_insert_id = true;
    }
}
