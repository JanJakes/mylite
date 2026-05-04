#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "sqlite3.h"

#include <stdlib.h>
#include <string.h>

static void record_insert_row_auto_increment_id(const struct mylite_insert_table *table,
                                                const struct mylite_insert_bound_value *values,
                                                struct mylite_insert_execution_state *state);

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
