#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_dml_insert_conflict.h"
#include "mylite_dml_insert_set_row_resolve.h"
#include "mylite_dml_insert_sqlite_bind.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct mylite_insert_integer_range {
    sqlite3_int64 minimum;
    sqlite3_int64 maximum;
};

static const sqlite3_int64 mylite_tinyint_signed_minimum = -128;
static const sqlite3_int64 mylite_tinyint_signed_maximum = 127;
static const sqlite3_int64 mylite_tinyint_unsigned_maximum = 255;
static const sqlite3_int64 mylite_smallint_signed_minimum = -32768;
static const sqlite3_int64 mylite_smallint_signed_maximum = 32767;
static const sqlite3_int64 mylite_smallint_unsigned_maximum = 65535;
static const sqlite3_int64 mylite_mediumint_signed_minimum = -8388608;
static const sqlite3_int64 mylite_mediumint_signed_maximum = 8388607;
static const sqlite3_int64 mylite_mediumint_unsigned_maximum = 16777215;
static const sqlite3_int64 mylite_int_signed_minimum = -2147483647 - 1;
static const sqlite3_int64 mylite_int_signed_maximum = 2147483647;
static const sqlite3_int64 mylite_int_unsigned_maximum = 4294967295LL;

static void record_insert_row_auto_increment_id(
    const struct mylite_insert_table *table,
    const struct mylite_insert_bound_value *values,
    struct mylite_insert_execution_state *state
);

static char *build_insert_update_physical_sql(
    mylite_db *database,
    const struct mylite_insert_table *table
);

static void append_insert_value_placeholder(
    sqlite3_str *sql,
    const struct mylite_insert_table_column *column
);

static bool insert_column_signed_integer_bounds(
    const struct mylite_insert_table_column *column,
    struct mylite_insert_integer_range *out_range
);

static bool insert_column_unsigned_integer_maximum(
    const struct mylite_insert_table_column *column,
    sqlite3_int64 *out_maximum
);

static bool insert_column_uses_double_coercion(const struct mylite_insert_table_column *column);

static bool insert_column_uses_varchar_coercion(const struct mylite_insert_table_column *column);

static bool insert_bound_values_equal(
    const struct mylite_insert_bound_value *left,
    const struct mylite_insert_bound_value *right
);

int mylite_dml_initialize_insert_ignore_warning_state(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    struct mylite_insert_execution_state *state
) {
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

void mylite_dml_insert_execution_state_deinit(struct mylite_insert_execution_state *state) {
    if (state == NULL) {
        return;
    }

    free(state->warned_omitted_no_default_columns);
    state->warned_omitted_no_default_columns = NULL;
    free(state->warned_null_columns);
    state->warned_null_columns = NULL;
}

char *mylite_dml_build_insert_physical_sql(
    mylite_db *database,
    const struct mylite_insert_table *table
) {
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
        append_insert_value_placeholder(sql, &table->columns[index]);
    }
    sqlite3_str_append(sql, ")", 1);
    return sqlite3_str_finish(sql);
}

char *mylite_dml_build_replace_delete_sql(
    mylite_db *database,
    const struct mylite_insert_table *table
) {
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

int mylite_dml_write_insert_candidate_row(
    mylite_db *database,
    sqlite3_stmt *insert,
    const struct mylite_insert_table *table,
    const struct mylite_insert_bound_value *values,
    struct mylite_insert_execution_state *state
) {
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

int mylite_dml_execute_insert_row(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const char *schema_name,
    sqlite3_stmt *insert,
    const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    struct mylite_insert_execution_state *state,
    size_t row_index,
    const struct mylite_dml_expression_callbacks *callbacks
) {
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

    status = mylite_dml_resolve_insert_row_values(
        database,
        plan,
        schema_name,
        table,
        column_indexes->insert_columns,
        plan->row_count,
        state,
        row_index,
        values,
        callbacks
    );
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_insert_unique_indexes(
            database,
            plan->table_name,
            plan->ignore,
            table,
            values,
            state,
            &ignored
        );
    }
    if (status == MYLITE_OK && !ignored) {
        status = mylite_dml_write_insert_candidate_row(database, insert, table, values, state);
    }

    mylite_dml_insert_bound_values_deinit(values, table->column_count);
    return status;
}

int mylite_dml_execute_insert_set_row(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_set_plan *set_plan,
    sqlite3_stmt *insert,
    const struct mylite_insert_table *table,
    const size_t *column_indexes,
    size_t column_index_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *values,
    struct mylite_insert_set_row_state *row_state,
    const struct mylite_dml_expression_callbacks *callbacks
) {
    bool ignored = false;
    int status = MYLITE_OK;

    if (database == NULL || values_plan == NULL || set_plan == NULL || insert == NULL ||
        table == NULL || state == NULL || values == NULL || row_state == NULL) {
        return MYLITE_MISUSE;
    }

    status = mylite_dml_resolve_insert_set_row_values(
        database,
        schema_name,
        values_plan,
        set_plan,
        table,
        column_indexes,
        column_index_count,
        1U,
        state,
        values,
        row_state,
        callbacks
    );
    if (status == MYLITE_OK) {
        status = mylite_dml_validate_insert_unique_indexes(
            database,
            values_plan->table_name,
            values_plan->ignore,
            table,
            values,
            state,
            &ignored
        );
    }
    if (status != MYLITE_OK || ignored) {
        return status;
    }

    return mylite_dml_write_insert_candidate_row(database, insert, table, values, state);
}

int mylite_dml_write_insert_update_candidate(
    mylite_db *database,
    const struct mylite_insert_table *table,
    sqlite3_int64 rowid,
    const struct mylite_insert_bound_value *values,
    struct mylite_insert_execution_state *state
) {
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

bool mylite_dml_insert_update_row_changed(
    const struct mylite_insert_bound_value *stored,
    const struct mylite_insert_bound_value *candidate,
    size_t value_count
) {
    for (size_t index = 0U; index < value_count; ++index) {
        if (!insert_bound_values_equal(&stored[index], &candidate[index])) {
            return true;
        }
    }
    return false;
}

int mylite_dml_advance_insert_row_auto_increment(
    const struct mylite_insert_table *table,
    const struct mylite_insert_bound_value *values,
    struct mylite_insert_execution_state *state
) {
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

static char *build_insert_update_physical_sql(
    mylite_db *database,
    const struct mylite_insert_table *table
) {
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "UPDATE \"%w\" SET ", table->physical_name);
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "\"%w\" = ", table->columns[index].name);
        append_insert_value_placeholder(sql, &table->columns[index]);
    }
    sqlite3_str_append(sql, " WHERE rowid = ?", (int)strlen(" WHERE rowid = ?"));
    return sqlite3_str_finish(sql);
}

static void append_insert_value_placeholder(
    sqlite3_str *sql,
    const struct mylite_insert_table_column *column
) {
    struct mylite_insert_integer_range range = {0};
    sqlite3_int64 maximum = 0;

    if (insert_column_unsigned_integer_maximum(column, &maximum)) {
        sqlite3_str_appendf(sql, "_mylite_coerce_unsigned_integer(?,%lld)", (long long)maximum);
        return;
    }
    if (insert_column_signed_integer_bounds(column, &range)) {
        sqlite3_str_appendf(
            sql,
            "_mylite_coerce_signed_integer(?,%lld,%lld)",
            (long long)range.minimum,
            (long long)range.maximum
        );
        return;
    }
    if (insert_column_uses_double_coercion(column)) {
        sqlite3_str_appendall(sql, "_mylite_coerce_double(?)");
        return;
    }
    if (insert_column_uses_varchar_coercion(column)) {
        sqlite3_str_appendf(
            sql,
            "_mylite_coerce_varchar(?,%llu)",
            (unsigned long long)column->character_maximum_length
        );
        return;
    }
    sqlite3_str_append(sql, "?", 1);
}

static bool insert_column_signed_integer_bounds(
    const struct mylite_insert_table_column *column,
    struct mylite_insert_integer_range *out_range
) {
    if (column == NULL || mylite_text_contains_word(column->column_type, "unsigned")) {
        return false;
    }
    if (mylite_ascii_case_equal(column->data_type, "tinyint")) {
        *out_range = (struct mylite_insert_integer_range){
            .minimum = mylite_tinyint_signed_minimum,
            .maximum = mylite_tinyint_signed_maximum,
        };
        return true;
    }
    if (mylite_ascii_case_equal(column->data_type, "smallint")) {
        *out_range = (struct mylite_insert_integer_range){
            .minimum = mylite_smallint_signed_minimum,
            .maximum = mylite_smallint_signed_maximum,
        };
        return true;
    }
    if (mylite_ascii_case_equal(column->data_type, "mediumint")) {
        *out_range = (struct mylite_insert_integer_range){
            .minimum = mylite_mediumint_signed_minimum,
            .maximum = mylite_mediumint_signed_maximum,
        };
        return true;
    }
    if (mylite_ascii_case_equal(column->data_type, "int")) {
        *out_range = (struct mylite_insert_integer_range){
            .minimum = mylite_int_signed_minimum,
            .maximum = mylite_int_signed_maximum,
        };
        return true;
    }
    if (mylite_ascii_case_equal(column->data_type, "bigint")) {
        *out_range = (struct mylite_insert_integer_range){
            .minimum = INT64_MIN,
            .maximum = INT64_MAX,
        };
        return true;
    }
    return false;
}

static bool insert_column_unsigned_integer_maximum(
    const struct mylite_insert_table_column *column,
    sqlite3_int64 *out_maximum
) {
    if (column == NULL || !mylite_text_contains_word(column->column_type, "unsigned")) {
        return false;
    }
    if (mylite_ascii_case_equal(column->data_type, "tinyint")) {
        *out_maximum = mylite_tinyint_unsigned_maximum;
        return true;
    }
    if (mylite_ascii_case_equal(column->data_type, "smallint")) {
        *out_maximum = mylite_smallint_unsigned_maximum;
        return true;
    }
    if (mylite_ascii_case_equal(column->data_type, "mediumint")) {
        *out_maximum = mylite_mediumint_unsigned_maximum;
        return true;
    }
    if (mylite_ascii_case_equal(column->data_type, "int")) {
        *out_maximum = mylite_int_unsigned_maximum;
        return true;
    }
    if (mylite_ascii_case_equal(column->data_type, "bigint")) {
        *out_maximum = INT64_MAX;
        return true;
    }
    return false;
}

static bool insert_column_uses_double_coercion(const struct mylite_insert_table_column *column) {
    if (column == NULL) {
        return false;
    }
    return (mylite_ascii_case_equal(column->data_type, "float") ||
            mylite_ascii_case_equal(column->data_type, "double")) != 0;
}

static bool insert_column_uses_varchar_coercion(const struct mylite_insert_table_column *column) {
    if (column == NULL || !column->has_character_maximum_length) {
        return false;
    }
    return (mylite_ascii_case_equal(column->data_type, "char") ||
            mylite_ascii_case_equal(column->data_type, "varchar")) != 0;
}

static bool insert_bound_values_equal(
    const struct mylite_insert_bound_value *left,
    const struct mylite_insert_bound_value *right
) {
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

static void record_insert_row_auto_increment_id(
    const struct mylite_insert_table *table,
    const struct mylite_insert_bound_value *values,
    struct mylite_insert_execution_state *state
) {
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
