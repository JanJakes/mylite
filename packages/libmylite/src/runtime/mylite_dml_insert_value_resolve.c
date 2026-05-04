#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int resolve_insert_column_list_row_values(
    mylite_db *database, const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table, const struct mylite_insert_row *row,
    const size_t *column_indexes, uint64_t statement_row_count,
    struct mylite_insert_execution_state *state, struct mylite_insert_bound_value *values);
static const struct mylite_insert_value *
insert_column_list_value_for_column(const struct mylite_insert_values_plan *plan,
                                    const struct mylite_insert_row *row,
                                    const size_t *column_indexes, size_t column);
static int resolve_insert_positional_row_values(mylite_db *database,
                                                const struct mylite_insert_values_plan *plan,
                                                const struct mylite_insert_table *table,
                                                size_t row_index, uint64_t statement_row_count,
                                                struct mylite_insert_execution_state *state,
                                                struct mylite_insert_bound_value *values);
static int resolve_insert_default_row_values(mylite_db *database,
                                             const struct mylite_insert_values_plan *plan,
                                             const struct mylite_insert_table *table,
                                             uint64_t statement_row_count,
                                             struct mylite_insert_execution_state *state,
                                             struct mylite_insert_bound_value *values);
static int initialize_insert_set_row_values(mylite_db *database,
                                            const struct mylite_insert_table *table,
                                            uint64_t statement_row_count,
                                            struct mylite_insert_execution_state *state,
                                            struct mylite_insert_bound_value *values,
                                            struct mylite_insert_set_row_state *row_state);
static int apply_insert_set_assignments(mylite_db *database, const char *schema_name,
                                        const struct mylite_insert_values_plan *values_plan,
                                        const struct mylite_insert_set_plan *set_plan,
                                        const struct mylite_insert_table *table,
                                        const size_t *column_indexes, size_t column_index_count,
                                        struct mylite_insert_bound_value *values,
                                        struct mylite_insert_set_row_state *row_state);
static int finish_insert_set_row_values(mylite_db *database,
                                        const struct mylite_insert_values_plan *plan,
                                        const struct mylite_insert_table *table,
                                        uint64_t statement_row_count,
                                        struct mylite_insert_execution_state *state,
                                        struct mylite_insert_bound_value *values,
                                        const struct mylite_insert_set_row_state *row_state);
static int finish_insert_set_required_omission(mylite_db *database,
                                               const struct mylite_insert_values_plan *plan,
                                               const struct mylite_insert_table_column *column,
                                               struct mylite_insert_execution_state *state,
                                               size_t column_index,
                                               const struct mylite_insert_set_row_state *row_state);
static int finish_insert_set_required_null(mylite_db *database,
                                           const struct mylite_insert_values_plan *plan,
                                           const struct mylite_insert_table_column *column,
                                           struct mylite_insert_bound_value *value);
static int evaluate_insert_set_assignment_value(
    mylite_db *database, const char *schema_name, const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table, size_t target_column,
    const struct mylite_insert_value *value, const struct mylite_insert_bound_value *values,
    bool *out_generate_auto_increment, struct mylite_insert_bound_value *out_value);
static int evaluate_insert_set_expression(mylite_db *database, const char *schema_name,
                                          const struct mylite_insert_values_plan *plan,
                                          const struct mylite_insert_table *table,
                                          const struct mylite_insert_value *value,
                                          const struct mylite_insert_bound_value *values,
                                          struct mylite_insert_bound_value *out_value);
static int evaluate_insert_set_simple_expression(mylite_db *database, const char *schema_name,
                                                 const struct mylite_insert_values_plan *plan,
                                                 const struct mylite_insert_table *table,
                                                 const struct mylite_insert_value *value,
                                                 const struct mylite_insert_bound_value *values,
                                                 struct mylite_insert_bound_value *out_value);
static int evaluate_insert_set_column_reference(mylite_db *database, const char *schema_name,
                                                const struct mylite_insert_values_plan *plan,
                                                const struct mylite_insert_table *table,
                                                const struct mylite_insert_column_reference *ref,
                                                const struct mylite_insert_bound_value *values,
                                                struct mylite_insert_bound_value *out_value);
static int evaluate_insert_set_unary_expression(mylite_db *database, const char *schema_name,
                                                const struct mylite_insert_values_plan *plan,
                                                const struct mylite_insert_table *table,
                                                const struct mylite_insert_value *value,
                                                const struct mylite_insert_bound_value *values,
                                                struct mylite_insert_bound_value *out_value);
static int evaluate_insert_set_binary_expression(mylite_db *database, const char *schema_name,
                                                 const struct mylite_insert_values_plan *plan,
                                                 const struct mylite_insert_table *table,
                                                 const struct mylite_insert_value *value,
                                                 const struct mylite_insert_bound_value *values,
                                                 struct mylite_insert_bound_value *out_value);
static int set_insert_set_candidate_auto_value(struct mylite_insert_bound_value *out_value);
static int
resolve_insert_explicit_value(mylite_db *database, const struct mylite_insert_values_plan *plan,
                              const struct mylite_insert_table_column *column,
                              const struct mylite_insert_value *value, uint64_t statement_row_count,
                              struct mylite_insert_execution_state *state, size_t column_index,
                              struct mylite_insert_bound_value *out_value);
static int resolve_insert_explicit_default_value(mylite_db *database,
                                                 const struct mylite_insert_values_plan *plan,
                                                 const struct mylite_insert_table_column *column,
                                                 uint64_t statement_row_count,
                                                 struct mylite_insert_execution_state *state,
                                                 struct mylite_insert_bound_value *out_value);
static int resolve_insert_omitted_default_value(mylite_db *database,
                                                const struct mylite_insert_values_plan *plan,
                                                const struct mylite_insert_table_column *column,
                                                uint64_t statement_row_count,
                                                struct mylite_insert_execution_state *state,
                                                size_t column_index,
                                                struct mylite_insert_bound_value *out_value);
static int resolve_insert_text_value(mylite_db *database,
                                     const struct mylite_insert_table_column *column,
                                     const char *text, uint64_t statement_row_count,
                                     struct mylite_insert_execution_state *state,
                                     struct mylite_insert_bound_value *out_value);
static int resolve_insert_quoted_text_value(mylite_db *database,
                                            const struct mylite_insert_table_column *column,
                                            const char *text, uint64_t statement_row_count,
                                            struct mylite_insert_execution_state *state,
                                            struct mylite_insert_bound_value *out_value);
static bool insert_column_uses_text_storage(const struct mylite_insert_table_column *column);
static int set_insert_bound_text_value(mylite_db *database, const char *text,
                                       struct mylite_insert_bound_value *out_value);
static int allocate_insert_auto_increment(mylite_db *database, uint64_t statement_row_count,
                                          struct mylite_insert_execution_state *state,
                                          struct mylite_insert_bound_value *out_value);
static int reserve_insert_auto_increment(mylite_db *database, uint64_t statement_row_count,
                                         struct mylite_insert_execution_state *state,
                                         uint64_t first_value);
static size_t insert_table_column_index(const struct mylite_insert_table *table,
                                        const char *column_name);
static size_t
insert_table_column_reference_index(const struct mylite_insert_table *table,
                                    const char *schema_name, const char *table_name,
                                    const struct mylite_insert_column_reference *reference);
static bool
insert_column_reference_qualifiers_match(const struct mylite_insert_column_reference *reference,
                                         const char *schema_name, const char *table_name);
static bool
insert_column_uses_numeric_implicit_default(const struct mylite_insert_table_column *column);
static bool insert_row_uses_all_defaults(const struct mylite_insert_values_plan *plan,
                                         size_t row_index);
static size_t insert_row_target_column_count(const struct mylite_insert_values_plan *plan,
                                             const struct mylite_insert_table *table,
                                             size_t row_index);
static int set_insert_wrong_value_count_error(mylite_db *database, size_t row_index);
static int set_insert_no_default_error(mylite_db *database, const char *column_name);
static int set_insert_unsupported_generated_default_error(mylite_db *database,
                                                          const char *column_name);
static int set_insert_unsupported_expression_error(mylite_db *database);
static int append_insert_no_default_warning(mylite_db *database, const char *column_name);
static int append_insert_no_default_warning_once(mylite_db *database,
                                                 const struct mylite_insert_table_column *column,
                                                 struct mylite_insert_execution_state *state,
                                                 size_t column_index);
static int append_insert_null_warning(mylite_db *database, const char *column_name);
static int append_insert_null_warning_once(mylite_db *database,
                                           const struct mylite_insert_table_column *column,
                                           struct mylite_insert_execution_state *state,
                                           size_t column_index);
static char *insert_current_timestamp_text(void);
static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_dml_resolve_insert_row_values(mylite_db *database,
                                         const struct mylite_insert_values_plan *plan,
                                         const struct mylite_insert_table *table,
                                         const size_t *column_indexes, uint64_t statement_row_count,
                                         struct mylite_insert_execution_state *state,
                                         size_t row_index,
                                         struct mylite_insert_bound_value *out_values)
{
    const struct mylite_insert_row *row = NULL;
    size_t expected_count = 0U;

    if (database == NULL || plan == NULL || table == NULL || state == NULL || out_values == NULL ||
        row_index >= plan->row_count) {
        return MYLITE_MISUSE;
    }

    row = &plan->rows[row_index];
    expected_count = insert_row_target_column_count(plan, table, row_index);
    if (row->value_count != expected_count) {
        return set_insert_wrong_value_count_error(database, row_index);
    }
    if (plan->has_column_list) {
        return resolve_insert_column_list_row_values(database, plan, table, row, column_indexes,
                                                     statement_row_count, state, out_values);
    }
    return resolve_insert_positional_row_values(database, plan, table, row_index,
                                                statement_row_count, state, out_values);
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
int mylite_dml_resolve_insert_set_row_values(
    mylite_db *database, const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_set_plan *set_plan, const struct mylite_insert_table *table,
    const size_t *column_indexes, size_t column_index_count, uint64_t statement_row_count,
    struct mylite_insert_execution_state *state, struct mylite_insert_bound_value *values,
    struct mylite_insert_set_row_state *row_state)
{
    int status = MYLITE_OK;

    if (database == NULL || schema_name == NULL || values_plan == NULL || set_plan == NULL ||
        table == NULL || state == NULL || values == NULL || row_state == NULL) {
        return MYLITE_MISUSE;
    }

    status = initialize_insert_set_row_values(database, table, statement_row_count, state, values,
                                              row_state);
    if (status == MYLITE_OK) {
        status =
            apply_insert_set_assignments(database, schema_name, values_plan, set_plan, table,
                                         column_indexes, column_index_count, values, row_state);
    }
    if (status == MYLITE_OK) {
        status = finish_insert_set_row_values(database, values_plan, table, statement_row_count,
                                              state, values, row_state);
    }
    return status;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

int mylite_dml_resolve_insert_default_bound_value(mylite_db *database,
                                                  const struct mylite_insert_table_column *column,
                                                  uint64_t statement_row_count,
                                                  struct mylite_insert_execution_state *state,
                                                  struct mylite_insert_bound_value *out_value)
{
    if (database == NULL || column == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    if (column->auto_increment && state == NULL) {
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = 0,
        };
        return MYLITE_OK;
    }
    if (column->auto_increment) {
        return allocate_insert_auto_increment(database, statement_row_count, state, out_value);
    }
    if (column->default_text == NULL) {
        if (column->nullable) {
            *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
            return MYLITE_OK;
        }
        return set_insert_no_default_error(database, column->name);
    }
    if (mylite_column_default_is_current_timestamp(column->default_text)) {
        char *timestamp = insert_current_timestamp_text();

        if (timestamp == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_TEXT,
            .text_value = timestamp,
        };
        return MYLITE_OK;
    }
    if (column->generated_default) {
        return set_insert_unsupported_generated_default_error(database, column->name);
    }
    return resolve_insert_text_value(database, column, column->default_text, statement_row_count,
                                     state, out_value);
}

int mylite_dml_resolve_insert_implicit_expression_default(
    mylite_db *database, const struct mylite_insert_table_column *column,
    struct mylite_insert_bound_value *out_value)
{
    const char *text_default = "";

    if (database == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    if (insert_column_uses_numeric_implicit_default(column)) {
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = 0,
        };
        return MYLITE_OK;
    }
    if (column != NULL && column->data_type != NULL) {
        if (mylite_ascii_case_equal(column->data_type, "date")) {
            text_default = "0000-00-00";
        } else if (mylite_ascii_case_equal(column->data_type, "time")) {
            text_default = "00:00:00";
        } else if (mylite_ascii_case_equal(column->data_type, "datetime") ||
                   mylite_ascii_case_equal(column->data_type, "timestamp")) {
            text_default = "0000-00-00 00:00:00";
        }
    }

    out_value->text_value = mylite_copy_span_text(text_default, strlen(text_default));
    if (out_value->text_value == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_value->kind = MYLITE_INSERT_BOUND_TEXT;
    return MYLITE_OK;
}

int mylite_dml_resolve_insert_current_timestamp_bound_value(
    mylite_db *database, struct mylite_insert_bound_value *out_value)
{
    char *timestamp = NULL;

    if (database == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    timestamp = insert_current_timestamp_text();
    if (timestamp == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *out_value = (struct mylite_insert_bound_value){
        .kind = MYLITE_INSERT_BOUND_TEXT,
        .text_value = timestamp,
    };
    return MYLITE_OK;
}

uint64_t
mylite_dml_insert_auto_increment_next_value(const struct mylite_insert_execution_state *state)
{
    if (state == NULL) {
        return 0U;
    }
    if (state->reserved_auto_increment_end > state->next_auto_increment) {
        return state->reserved_auto_increment_end;
    }
    return state->next_auto_increment;
}

int mylite_dml_bind_insert_row_values(mylite_db *database, sqlite3_stmt *insert,
                                      const struct mylite_insert_bound_value *values,
                                      size_t value_count)
{
    if (database == NULL || insert == NULL || values == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < value_count; ++index) {
        int rc = mylite_dml_bind_insert_bound_value(insert, (int)index + 1, &values[index]);

        if (rc != SQLITE_OK) {
            return mylite_diagnostics_set_sqlite_error(database);
        }
    }
    return MYLITE_OK;
}

int mylite_dml_bind_insert_bound_value(sqlite3_stmt *stmt, int index,
                                       const struct mylite_insert_bound_value *value)
{
    if (stmt == NULL || value == NULL) {
        return SQLITE_MISUSE;
    }

    switch (value->kind) {
    case MYLITE_INSERT_BOUND_NULL:
        return sqlite3_bind_null(stmt, index);
    case MYLITE_INSERT_BOUND_INTEGER:
        return sqlite3_bind_int64(stmt, index, (sqlite3_int64)value->integer_value);
    case MYLITE_INSERT_BOUND_REAL:
        return sqlite3_bind_double(stmt, index, value->real_value);
    case MYLITE_INSERT_BOUND_TEXT:
        return sqlite3_bind_text(stmt, index, value->text_value, -1, sqlite_transient_destructor());
    }

    return SQLITE_MISUSE;
}

int mylite_dml_copy_insert_sqlite_column_value(sqlite3_stmt *scan, int column,
                                               struct mylite_insert_bound_value *out_value)
{
    int sqlite_type = SQLITE_NULL;

    if (scan == NULL || out_value == NULL) {
        return -1;
    }

    sqlite_type = sqlite3_column_type(scan, column);
    switch (sqlite_type) {
    case SQLITE_NULL:
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        return 0;
    case SQLITE_INTEGER:
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = sqlite3_column_int64(scan, column),
        };
        return 0;
    case SQLITE_FLOAT:
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_REAL,
            .real_value = sqlite3_column_double(scan, column),
        };
        return 0;
    case SQLITE_TEXT:
    case SQLITE_BLOB: {
        const unsigned char *text = sqlite3_column_text(scan, column);
        int bytes = sqlite3_column_bytes(scan, column);

        out_value->kind = MYLITE_INSERT_BOUND_TEXT;
        out_value->text_value =
            mylite_copy_span_text((const char *)text, bytes < 0 ? 0U : (size_t)bytes);
        return out_value->text_value == NULL ? -1 : 0;
    }
    default:
        break;
    }
    return -1;
}

int mylite_dml_copy_insert_bound_value(const struct mylite_insert_bound_value *value,
                                       struct mylite_insert_bound_value *out_value)
{
    if (value == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    *out_value = *value;
    out_value->text_value = NULL;
    if (value->kind == MYLITE_INSERT_BOUND_TEXT && value->text_value != NULL) {
        out_value->text_value = mylite_copy_span_text(value->text_value, strlen(value->text_value));
        if (out_value->text_value == NULL) {
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

int mylite_dml_copy_insert_bound_values(mylite_db *database,
                                        const struct mylite_insert_bound_value *values,
                                        size_t value_count,
                                        struct mylite_insert_bound_value **out_values)
{
    struct mylite_insert_bound_value *copy = NULL;

    if (database == NULL || out_values == NULL || (values == NULL && value_count != 0U)) {
        return MYLITE_MISUSE;
    }

    *out_values = NULL;
    if (value_count == 0U) {
        return MYLITE_OK;
    }

    copy = calloc(value_count, sizeof(*copy));
    if (copy == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < value_count; ++index) {
        int status = mylite_dml_copy_insert_bound_value(&values[index], &copy[index]);

        if (status != MYLITE_OK) {
            mylite_dml_insert_bound_values_deinit(copy, value_count);
            if (status == MYLITE_NOMEM) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
            }
            return status;
        }
    }

    *out_values = copy;
    return MYLITE_OK;
}

bool mylite_dml_insert_bound_value_is_numeric(const struct mylite_insert_bound_value *value,
                                              double *out_value, bool *out_is_integer)
{
    int64_t integer_value = 0;

    if (out_value == NULL || out_is_integer == NULL) {
        return false;
    }

    *out_value = 0.0;
    *out_is_integer = false;
    if (value == NULL) {
        return false;
    }
    if (value->kind == MYLITE_INSERT_BOUND_INTEGER) {
        *out_value = (double)value->integer_value;
        *out_is_integer = true;
        return true;
    }
    if (value->kind == MYLITE_INSERT_BOUND_REAL) {
        *out_value = value->real_value;
        return true;
    }
    if (value->kind == MYLITE_INSERT_BOUND_TEXT &&
        mylite_dml_parse_insert_integer_text(value->text_value, &integer_value)) {
        *out_value = (double)integer_value;
        *out_is_integer = true;
        return true;
    }
    if (value->kind == MYLITE_INSERT_BOUND_TEXT &&
        mylite_dml_parse_insert_real_text(value->text_value, out_value)) {
        return true;
    }
    return false;
}

bool mylite_dml_parse_insert_integer_text(const char *text, int64_t *out_value)
{
    enum { decimal_base = 10 };
    char *end = NULL;
    long long value = 0;

    if (text == NULL || text[0] == '\0' || out_value == NULL) {
        return false;
    }
    errno = 0;
    value = strtoll(text, &end, decimal_base);
    if (errno != 0 || end == text) {
        return false;
    }
    while (*end != '\0' && isspace((unsigned char)*end)) {
        ++end;
    }
    if (*end != '\0') {
        return false;
    }
    *out_value = (int64_t)value;
    return true;
}

bool mylite_dml_parse_insert_real_text(const char *text, double *out_value)
{
    char *end = NULL;
    double value = 0.0;

    if (text == NULL || text[0] == '\0' || out_value == NULL) {
        return false;
    }
    errno = 0;
    value = strtod(text, &end);
    if (errno != 0 || end == text) {
        return false;
    }
    while (*end != '\0' && isspace((unsigned char)*end)) {
        ++end;
    }
    if (*end != '\0') {
        return false;
    }
    *out_value = value;
    return true;
}

static int resolve_insert_column_list_row_values(
    mylite_db *database, const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table, const struct mylite_insert_row *row,
    const size_t *column_indexes, uint64_t statement_row_count,
    struct mylite_insert_execution_state *state, struct mylite_insert_bound_value *values)
{
    if (plan->column_count == 0U) {
        return resolve_insert_default_row_values(database, plan, table, statement_row_count, state,
                                                 values);
    }
    if (column_indexes == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    for (size_t column = 0U; column < table->column_count; ++column) {
        const struct mylite_insert_value *explicit_value =
            insert_column_list_value_for_column(plan, row, column_indexes, column);
        int status = explicit_value == NULL
                         ? resolve_insert_omitted_default_value(
                               database, plan, &table->columns[column], statement_row_count, state,
                               column, &values[column])
                         : resolve_insert_explicit_value(database, plan, &table->columns[column],
                                                         explicit_value, statement_row_count, state,
                                                         column, &values[column]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static const struct mylite_insert_value *
insert_column_list_value_for_column(const struct mylite_insert_values_plan *plan,
                                    const struct mylite_insert_row *row,
                                    const size_t *column_indexes, size_t column)
{
    for (size_t target = 0U; target < plan->column_count; ++target) {
        if (column_indexes[target] == column) {
            return &row->values[target];
        }
    }
    return NULL;
}

static int resolve_insert_positional_row_values(mylite_db *database,
                                                const struct mylite_insert_values_plan *plan,
                                                const struct mylite_insert_table *table,
                                                size_t row_index, uint64_t statement_row_count,
                                                struct mylite_insert_execution_state *state,
                                                struct mylite_insert_bound_value *values)
{
    if (insert_row_uses_all_defaults(plan, row_index)) {
        return resolve_insert_default_row_values(database, plan, table, statement_row_count, state,
                                                 values);
    }

    for (size_t column = 0U; column < table->column_count; ++column) {
        int status = resolve_insert_explicit_value(
            database, plan, &table->columns[column], &plan->rows[row_index].values[column],
            statement_row_count, state, column, &values[column]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int resolve_insert_default_row_values(mylite_db *database,
                                             const struct mylite_insert_values_plan *plan,
                                             const struct mylite_insert_table *table,
                                             uint64_t statement_row_count,
                                             struct mylite_insert_execution_state *state,
                                             struct mylite_insert_bound_value *values)
{
    for (size_t column = 0U; column < table->column_count; ++column) {
        int status = resolve_insert_omitted_default_value(database, plan, &table->columns[column],
                                                          statement_row_count, state, column,
                                                          &values[column]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int initialize_insert_set_row_values(mylite_db *database,
                                            const struct mylite_insert_table *table,
                                            uint64_t statement_row_count,
                                            struct mylite_insert_execution_state *state,
                                            struct mylite_insert_bound_value *values,
                                            struct mylite_insert_set_row_state *row_state)
{
    for (size_t column = 0U; column < table->column_count; ++column) {
        int status = MYLITE_OK;

        if (table->columns[column].auto_increment) {
            status = set_insert_set_candidate_auto_value(&values[column]);
            row_state->generate_auto_increment[column] = true;
        } else if (table->columns[column].default_text != NULL || table->columns[column].nullable) {
            status = mylite_dml_resolve_insert_default_bound_value(
                database, &table->columns[column], statement_row_count, state, &values[column]);
        } else {
            status = mylite_dml_resolve_insert_implicit_expression_default(
                database, &table->columns[column], &values[column]);
        }
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int apply_insert_set_assignments(mylite_db *database, const char *schema_name,
                                        const struct mylite_insert_values_plan *values_plan,
                                        const struct mylite_insert_set_plan *set_plan,
                                        const struct mylite_insert_table *table,
                                        const size_t *column_indexes, size_t column_index_count,
                                        struct mylite_insert_bound_value *values,
                                        struct mylite_insert_set_row_state *row_state)
{
    if (column_indexes == NULL || column_index_count != set_plan->assignment_count) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < column_index_count; ++index) {
        size_t column_index = column_indexes[index];
        struct mylite_insert_bound_value value = {0};
        bool generate_auto = false;
        int status = evaluate_insert_set_assignment_value(
            database, schema_name, values_plan, table, column_index,
            &set_plan->assignments[index].value, values, &generate_auto, &value);

        if (status != MYLITE_OK) {
            mylite_dml_insert_bound_value_deinit(&value);
            return status;
        }

        mylite_dml_insert_bound_value_deinit(&values[column_index]);
        values[column_index] = value;
        row_state->generate_auto_increment[column_index] = generate_auto;
        row_state->assigned_columns[column_index] = true;
    }
    return MYLITE_OK;
}

static int finish_insert_set_row_values(mylite_db *database,
                                        const struct mylite_insert_values_plan *plan,
                                        const struct mylite_insert_table *table,
                                        uint64_t statement_row_count,
                                        struct mylite_insert_execution_state *state,
                                        struct mylite_insert_bound_value *values,
                                        const struct mylite_insert_set_row_state *row_state)
{
    for (size_t column = 0U; column < table->column_count; ++column) {
        const struct mylite_insert_table_column *table_column = &table->columns[column];
        int status = finish_insert_set_required_omission(database, plan, table_column, state,
                                                         column, row_state);

        if (status != MYLITE_OK) {
            return status;
        }
        if (table_column->auto_increment && row_state->generate_auto_increment[column]) {
            mylite_dml_insert_bound_value_deinit(&values[column]);
            status = allocate_insert_auto_increment(database, statement_row_count, state,
                                                    &values[column]);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        status = finish_insert_set_required_null(database, plan, table_column, &values[column]);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int finish_insert_set_required_omission(mylite_db *database,
                                               const struct mylite_insert_values_plan *plan,
                                               const struct mylite_insert_table_column *column,
                                               struct mylite_insert_execution_state *state,
                                               size_t column_index,
                                               const struct mylite_insert_set_row_state *row_state)
{
    if (column->auto_increment || column->nullable || column->default_text != NULL ||
        row_state->assigned_columns[column_index]) {
        return MYLITE_OK;
    }
    if (!plan->ignore) {
        return set_insert_no_default_error(database, column->name);
    }
    return append_insert_no_default_warning_once(database, column, state, column_index);
}

static int finish_insert_set_required_null(mylite_db *database,
                                           const struct mylite_insert_values_plan *plan,
                                           const struct mylite_insert_table_column *column,
                                           struct mylite_insert_bound_value *value)
{
    if (column->auto_increment || column->nullable || value->kind != MYLITE_INSERT_BOUND_NULL) {
        return MYLITE_OK;
    }
    if (!plan->ignore) {
        return mylite_dml_set_not_null_column_error(database, column->name);
    }

    int status = append_insert_null_warning(database, column->name);

    if (status != MYLITE_OK) {
        return status;
    }
    mylite_dml_insert_bound_value_deinit(value);
    return mylite_dml_resolve_insert_implicit_expression_default(database, column, value);
}

static int evaluate_insert_set_assignment_value(
    mylite_db *database, const char *schema_name, const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table, size_t target_column,
    const struct mylite_insert_value *value, const struct mylite_insert_bound_value *values,
    bool *out_generate_auto_increment, struct mylite_insert_bound_value *out_value)
{
    const struct mylite_insert_table_column *column = &table->columns[target_column];
    int status = MYLITE_OK;

    *out_generate_auto_increment = false;
    if (value->kind == MYLITE_INSERT_VALUE_DEFAULT) {
        if (column->auto_increment) {
            *out_generate_auto_increment = true;
            return set_insert_set_candidate_auto_value(out_value);
        }
        return resolve_insert_explicit_default_value(database, plan, column, 1U, NULL, out_value);
    }

    status = evaluate_insert_set_expression(database, schema_name, plan, table, value, values,
                                            out_value);
    if (status != MYLITE_OK) {
        return status;
    }

    if (column->auto_increment) {
        if (out_value->kind == MYLITE_INSERT_BOUND_NULL ||
            (out_value->kind == MYLITE_INSERT_BOUND_INTEGER && out_value->integer_value == 0)) {
            mylite_dml_insert_bound_value_deinit(out_value);
            *out_generate_auto_increment = true;
            return set_insert_set_candidate_auto_value(out_value);
        }
        if (out_value->kind != MYLITE_INSERT_BOUND_INTEGER || out_value->integer_value < 0) {
            mylite_dml_insert_bound_value_deinit(out_value);
            return set_insert_unsupported_expression_error(database);
        }
    } else if (!column->nullable && out_value->kind == MYLITE_INSERT_BOUND_NULL) {
        if (plan->ignore) {
            int warning_status = append_insert_null_warning(database, column->name);

            if (warning_status != MYLITE_OK) {
                mylite_dml_insert_bound_value_deinit(out_value);
                return warning_status;
            }
            mylite_dml_insert_bound_value_deinit(out_value);
            return mylite_dml_resolve_insert_implicit_expression_default(database, column,
                                                                         out_value);
        }
        mylite_dml_insert_bound_value_deinit(out_value);
        return mylite_dml_set_not_null_column_error(database, column->name);
    }
    return MYLITE_OK;
}

static int evaluate_insert_set_expression(mylite_db *database, const char *schema_name,
                                          const struct mylite_insert_values_plan *plan,
                                          const struct mylite_insert_table *table,
                                          const struct mylite_insert_value *value,
                                          const struct mylite_insert_bound_value *values,
                                          struct mylite_insert_bound_value *out_value)
{
    if (value->kind == MYLITE_INSERT_VALUE_UNARY_EXPRESSION) {
        return evaluate_insert_set_unary_expression(database, schema_name, plan, table, value,
                                                    values, out_value);
    }
    if (value->kind == MYLITE_INSERT_VALUE_BINARY_EXPRESSION) {
        return evaluate_insert_set_binary_expression(database, schema_name, plan, table, value,
                                                     values, out_value);
    }
    return evaluate_insert_set_simple_expression(database, schema_name, plan, table, value, values,
                                                 out_value);
}

static int evaluate_insert_set_unary_expression(mylite_db *database, const char *schema_name,
                                                const struct mylite_insert_values_plan *plan,
                                                const struct mylite_insert_table *table,
                                                const struct mylite_insert_value *value,
                                                const struct mylite_insert_bound_value *values,
                                                struct mylite_insert_bound_value *out_value)
{
    struct mylite_insert_bound_value operand = {0};
    double numeric_value = 0.0;
    bool is_integer = false;
    int status = evaluate_insert_set_simple_expression(database, schema_name, plan, table,
                                                       value->left, values, &operand);

    if (status != MYLITE_OK) {
        mylite_dml_insert_bound_value_deinit(&operand);
        return status;
    }
    if (operand.kind == MYLITE_INSERT_BOUND_NULL) {
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        mylite_dml_insert_bound_value_deinit(&operand);
        return MYLITE_OK;
    }
    if (!mylite_dml_insert_bound_value_is_numeric(&operand, &numeric_value, &is_integer)) {
        mylite_dml_insert_bound_value_deinit(&operand);
        return set_insert_unsupported_expression_error(database);
    }

    if (is_integer) {
        int64_t integer_value = operand.kind == MYLITE_INSERT_BOUND_INTEGER
                                    ? operand.integer_value
                                    : (int64_t)numeric_value;

        out_value->kind = MYLITE_INSERT_BOUND_INTEGER;
        out_value->integer_value = value->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE
                                       ? -integer_value
                                       : integer_value;
    } else {
        out_value->kind = MYLITE_INSERT_BOUND_REAL;
        out_value->real_value = value->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE
                                    ? -numeric_value
                                    : numeric_value;
    }
    mylite_dml_insert_bound_value_deinit(&operand);
    return MYLITE_OK;
}

static int evaluate_insert_set_binary_expression(mylite_db *database, const char *schema_name,
                                                 const struct mylite_insert_values_plan *plan,
                                                 const struct mylite_insert_table *table,
                                                 const struct mylite_insert_value *value,
                                                 const struct mylite_insert_bound_value *values,
                                                 struct mylite_insert_bound_value *out_value)
{
    struct mylite_insert_bound_value left = {0};
    struct mylite_insert_bound_value right = {0};
    double left_number = 0.0;
    double right_number = 0.0;
    bool left_is_integer = false;
    bool right_is_integer = false;
    int status = evaluate_insert_set_simple_expression(database, schema_name, plan, table,
                                                       value->left, values, &left);

    if (status == MYLITE_OK) {
        status = evaluate_insert_set_simple_expression(database, schema_name, plan, table,
                                                       value->right, values, &right);
    }
    if (status != MYLITE_OK) {
        mylite_dml_insert_bound_value_deinit(&left);
        mylite_dml_insert_bound_value_deinit(&right);
        return status;
    }
    if (left.kind == MYLITE_INSERT_BOUND_NULL || right.kind == MYLITE_INSERT_BOUND_NULL) {
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        mylite_dml_insert_bound_value_deinit(&left);
        mylite_dml_insert_bound_value_deinit(&right);
        return MYLITE_OK;
    }
    if (!mylite_dml_insert_bound_value_is_numeric(&left, &left_number, &left_is_integer) ||
        !mylite_dml_insert_bound_value_is_numeric(&right, &right_number, &right_is_integer)) {
        mylite_dml_insert_bound_value_deinit(&left);
        mylite_dml_insert_bound_value_deinit(&right);
        return set_insert_unsupported_expression_error(database);
    }

    if (left_is_integer && right_is_integer &&
        value->operator_kind != MYLITE_SQL_AST_OPERATOR_DIVIDE) {
        int64_t left_int =
            left.kind == MYLITE_INSERT_BOUND_INTEGER ? left.integer_value : (int64_t)left_number;
        int64_t right_int =
            right.kind == MYLITE_INSERT_BOUND_INTEGER ? right.integer_value : (int64_t)right_number;

        out_value->kind = MYLITE_INSERT_BOUND_INTEGER;
        switch (value->operator_kind) {
        case MYLITE_SQL_AST_OPERATOR_ADD:
            out_value->integer_value = left_int + right_int;
            break;
        case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
            out_value->integer_value = left_int - right_int;
            break;
        case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
            out_value->integer_value = left_int * right_int;
            break;
        case MYLITE_SQL_AST_OPERATOR_DIVIDE:
        case MYLITE_SQL_AST_OPERATOR_NONE:
        case MYLITE_SQL_AST_OPERATOR_POSITIVE:
        case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        default:
            mylite_dml_insert_bound_value_deinit(&left);
            mylite_dml_insert_bound_value_deinit(&right);
            return set_insert_unsupported_expression_error(database);
        }
    } else {
        if (value->operator_kind == MYLITE_SQL_AST_OPERATOR_DIVIDE && right_number == 0.0) {
            mylite_dml_insert_bound_value_deinit(&left);
            mylite_dml_insert_bound_value_deinit(&right);
            return set_insert_unsupported_expression_error(database);
        }
        out_value->kind = MYLITE_INSERT_BOUND_REAL;
        switch (value->operator_kind) {
        case MYLITE_SQL_AST_OPERATOR_ADD:
            out_value->real_value = left_number + right_number;
            break;
        case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
            out_value->real_value = left_number - right_number;
            break;
        case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
            out_value->real_value = left_number * right_number;
            break;
        case MYLITE_SQL_AST_OPERATOR_DIVIDE:
            out_value->real_value = left_number / right_number;
            break;
        case MYLITE_SQL_AST_OPERATOR_NONE:
        case MYLITE_SQL_AST_OPERATOR_POSITIVE:
        case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        default:
            mylite_dml_insert_bound_value_deinit(&left);
            mylite_dml_insert_bound_value_deinit(&right);
            return set_insert_unsupported_expression_error(database);
        }
    }

    mylite_dml_insert_bound_value_deinit(&left);
    mylite_dml_insert_bound_value_deinit(&right);
    return MYLITE_OK;
}

static int evaluate_insert_set_simple_expression(mylite_db *database, const char *schema_name,
                                                 const struct mylite_insert_values_plan *plan,
                                                 const struct mylite_insert_table *table,
                                                 const struct mylite_insert_value *value,
                                                 const struct mylite_insert_bound_value *values,
                                                 struct mylite_insert_bound_value *out_value)
{
    int64_t integer_value = 0;
    double real_value = 0.0;
    char *timestamp = NULL;

    switch (value->kind) {
    case MYLITE_INSERT_VALUE_NULL:
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_INTEGER:
        if (!mylite_dml_parse_insert_integer_text(value->text, &integer_value)) {
            return set_insert_unsupported_expression_error(database);
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = integer_value,
        };
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_REAL:
        if (!mylite_dml_parse_insert_real_text(value->text, &real_value)) {
            return set_insert_unsupported_expression_error(database);
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_REAL,
            .real_value = real_value,
        };
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_TEXT:
        out_value->text_value =
            mylite_copy_span_text(value->text, value->text == NULL ? 0U : strlen(value->text));
        if (out_value->text_value == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        out_value->kind = MYLITE_INSERT_BOUND_TEXT;
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_CURRENT_TIMESTAMP:
        timestamp = insert_current_timestamp_text();
        if (timestamp == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_TEXT,
            .text_value = timestamp,
        };
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_COLUMN_REFERENCE:
        return evaluate_insert_set_column_reference(database, schema_name, plan, table,
                                                    &value->column_reference, values, out_value);
    case MYLITE_INSERT_VALUE_DEFAULT:
    case MYLITE_INSERT_VALUE_UNSUPPORTED:
    case MYLITE_INSERT_VALUE_VALUES_FUNCTION:
    case MYLITE_INSERT_VALUE_UNARY_EXPRESSION:
    case MYLITE_INSERT_VALUE_BINARY_EXPRESSION:
        return set_insert_unsupported_expression_error(database);
    }

    return set_insert_unsupported_expression_error(database);
}

static int evaluate_insert_set_column_reference(mylite_db *database, const char *schema_name,
                                                const struct mylite_insert_values_plan *plan,
                                                const struct mylite_insert_table *table,
                                                const struct mylite_insert_column_reference *ref,
                                                const struct mylite_insert_bound_value *values,
                                                struct mylite_insert_bound_value *out_value)
{
    size_t column_index =
        insert_table_column_reference_index(table, schema_name, plan->table_name, ref);

    if (column_index == table->column_count) {
        int status = mylite_diagnostics_set_error_message_parts(
            database, "Unknown column '", ref->column_name, "' in 'field list'");

        return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
    }
    int status = mylite_dml_copy_insert_bound_value(&values[column_index], out_value);

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    return status;
}

static int set_insert_set_candidate_auto_value(struct mylite_insert_bound_value *out_value)
{
    *out_value = (struct mylite_insert_bound_value){
        .kind = MYLITE_INSERT_BOUND_INTEGER,
        .integer_value = 0,
    };
    return MYLITE_OK;
}

static int
resolve_insert_explicit_value(mylite_db *database, const struct mylite_insert_values_plan *plan,
                              const struct mylite_insert_table_column *column,
                              const struct mylite_insert_value *value, uint64_t statement_row_count,
                              struct mylite_insert_execution_state *state, size_t column_index,
                              struct mylite_insert_bound_value *out_value)
{
    char *timestamp = NULL;

    switch (value->kind) {
    case MYLITE_INSERT_VALUE_DEFAULT:
        return resolve_insert_explicit_default_value(database, plan, column, statement_row_count,
                                                     state, out_value);
    case MYLITE_INSERT_VALUE_NULL:
        if (column->auto_increment) {
            return allocate_insert_auto_increment(database, statement_row_count, state, out_value);
        }
        if (!column->nullable) {
            if (plan->ignore) {
                int status = append_insert_null_warning_once(database, column, state, column_index);

                if (status != MYLITE_OK) {
                    return status;
                }
                return mylite_dml_resolve_insert_implicit_expression_default(database, column,
                                                                             out_value);
            }
            return mylite_dml_set_not_null_column_error(database, column->name);
        }
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_INTEGER:
        return resolve_insert_text_value(database, column, value->text, statement_row_count, state,
                                         out_value);
    case MYLITE_INSERT_VALUE_REAL:
        if (column->auto_increment) {
            return set_insert_unsupported_expression_error(database);
        }
        return resolve_insert_text_value(database, column, value->text, statement_row_count, state,
                                         out_value);
    case MYLITE_INSERT_VALUE_TEXT:
        return resolve_insert_quoted_text_value(database, column, value->text, statement_row_count,
                                                state, out_value);
    case MYLITE_INSERT_VALUE_CURRENT_TIMESTAMP:
        if (column->auto_increment) {
            return set_insert_unsupported_expression_error(database);
        }
        timestamp = insert_current_timestamp_text();
        if (timestamp == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_TEXT,
            .text_value = timestamp,
        };
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_UNSUPPORTED:
    case MYLITE_INSERT_VALUE_COLUMN_REFERENCE:
    case MYLITE_INSERT_VALUE_VALUES_FUNCTION:
    case MYLITE_INSERT_VALUE_UNARY_EXPRESSION:
    case MYLITE_INSERT_VALUE_BINARY_EXPRESSION:
        return set_insert_unsupported_expression_error(database);
    }

    return set_insert_unsupported_expression_error(database);
}

static int resolve_insert_explicit_default_value(mylite_db *database,
                                                 const struct mylite_insert_values_plan *plan,
                                                 const struct mylite_insert_table_column *column,
                                                 uint64_t statement_row_count,
                                                 struct mylite_insert_execution_state *state,
                                                 struct mylite_insert_bound_value *out_value)
{
    if (plan->ignore && !column->auto_increment && !column->nullable &&
        column->default_text == NULL) {
        int status = append_insert_no_default_warning(database, column->name);

        if (status != MYLITE_OK) {
            return status;
        }
        return mylite_dml_resolve_insert_implicit_expression_default(database, column, out_value);
    }
    return mylite_dml_resolve_insert_default_bound_value(database, column, statement_row_count,
                                                         state, out_value);
}

static int resolve_insert_omitted_default_value(mylite_db *database,
                                                const struct mylite_insert_values_plan *plan,
                                                const struct mylite_insert_table_column *column,
                                                uint64_t statement_row_count,
                                                struct mylite_insert_execution_state *state,
                                                size_t column_index,
                                                struct mylite_insert_bound_value *out_value)
{
    if (plan->ignore && !column->auto_increment && !column->nullable &&
        column->default_text == NULL) {
        int status = append_insert_no_default_warning_once(database, column, state, column_index);

        if (status != MYLITE_OK) {
            return status;
        }
        return mylite_dml_resolve_insert_implicit_expression_default(database, column, out_value);
    }
    return mylite_dml_resolve_insert_default_bound_value(database, column, statement_row_count,
                                                         state, out_value);
}

static int resolve_insert_text_value(mylite_db *database,
                                     const struct mylite_insert_table_column *column,
                                     const char *text, uint64_t statement_row_count,
                                     struct mylite_insert_execution_state *state,
                                     struct mylite_insert_bound_value *out_value)
{
    int64_t integer_value = 0;
    double real_value = 0.0;

    if (text == NULL) {
        if (!column->nullable) {
            return mylite_dml_set_not_null_column_error(database, column->name);
        }
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        return MYLITE_OK;
    }
    if (mylite_dml_parse_insert_integer_text(text, &integer_value)) {
        if (column->auto_increment && integer_value == 0) {
            return allocate_insert_auto_increment(database, statement_row_count, state, out_value);
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = integer_value,
        };
        return MYLITE_OK;
    }
    if (column->auto_increment) {
        return set_insert_unsupported_expression_error(database);
    }
    if (mylite_dml_parse_insert_real_text(text, &real_value)) {
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_REAL,
            .real_value = real_value,
        };
        return MYLITE_OK;
    }

    return set_insert_bound_text_value(database, text, out_value);
}

static int resolve_insert_quoted_text_value(mylite_db *database,
                                            const struct mylite_insert_table_column *column,
                                            const char *text, uint64_t statement_row_count,
                                            struct mylite_insert_execution_state *state,
                                            struct mylite_insert_bound_value *out_value)
{
    if (text == NULL || !insert_column_uses_text_storage(column)) {
        return resolve_insert_text_value(database, column, text, statement_row_count, state,
                                         out_value);
    }
    return set_insert_bound_text_value(database, text, out_value);
}

static bool insert_column_uses_text_storage(const struct mylite_insert_table_column *column)
{
    static const char *const text_types[] = {
        "char",   "varchar",   "tinytext", "text", "mediumtext", "longtext",
        "binary", "varbinary", "tinyblob", "blob", "mediumblob", "longblob",
    };

    if (column == NULL || column->data_type == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(text_types) / sizeof(text_types[0]); ++index) {
        if (mylite_ascii_case_equal(column->data_type, text_types[index])) {
            return true;
        }
    }
    return false;
}

static int set_insert_bound_text_value(mylite_db *database, const char *text,
                                       struct mylite_insert_bound_value *out_value)
{
    out_value->text_value = mylite_copy_span_text(text, strlen(text));
    if (out_value->text_value == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_value->kind = MYLITE_INSERT_BOUND_TEXT;
    return MYLITE_OK;
}

static int allocate_insert_auto_increment(mylite_db *database, uint64_t statement_row_count,
                                          struct mylite_insert_execution_state *state,
                                          struct mylite_insert_bound_value *out_value)
{
    uint64_t value = 0U;
    int status = MYLITE_OK;

    if (state == NULL) {
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = 0,
        };
        return MYLITE_OK;
    }

    value = state->next_auto_increment == 0U ? 1U : state->next_auto_increment;
    if (value > (uint64_t)INT64_MAX) {
        (void)mylite_diagnostics_set_error_message(database,
                                                   "AUTO_INCREMENT value is out of range");
        return MYLITE_EXEC_ERROR;
    }
    status = reserve_insert_auto_increment(database, statement_row_count, state, value);
    if (status != MYLITE_OK) {
        return status;
    }
    state->next_auto_increment = value + 1U;
    *out_value = (struct mylite_insert_bound_value){
        .kind = MYLITE_INSERT_BOUND_INTEGER,
        .integer_value = (int64_t)value,
        .generated_auto_increment = true,
    };
    return MYLITE_OK;
}

static int reserve_insert_auto_increment(mylite_db *database, uint64_t statement_row_count,
                                         struct mylite_insert_execution_state *state,
                                         uint64_t first_value)
{
    if (state->reserved_auto_increment_end != 0U) {
        return MYLITE_OK;
    }
    if (statement_row_count > (uint64_t)INT64_MAX - first_value) {
        (void)mylite_diagnostics_set_error_message(database,
                                                   "AUTO_INCREMENT value is out of range");
        return MYLITE_EXEC_ERROR;
    }
    state->reserved_auto_increment_end = first_value + statement_row_count;
    return MYLITE_OK;
}

static size_t insert_table_column_index(const struct mylite_insert_table *table,
                                        const char *column_name)
{
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (mylite_ascii_case_equal(table->columns[index].name, column_name)) {
            return index;
        }
    }
    return table->column_count;
}

static size_t
insert_table_column_reference_index(const struct mylite_insert_table *table,
                                    const char *schema_name, const char *table_name,
                                    const struct mylite_insert_column_reference *reference)
{
    if (!insert_column_reference_qualifiers_match(reference, schema_name, table_name)) {
        return table->column_count;
    }
    return insert_table_column_index(table, reference->column_name);
}

static bool
insert_column_reference_qualifiers_match(const struct mylite_insert_column_reference *reference,
                                         const char *schema_name, const char *table_name)
{
    if (reference->schema_name != NULL &&
        !mylite_ascii_case_equal(reference->schema_name, schema_name)) {
        return false;
    }
    if (reference->table_name != NULL &&
        !mylite_ascii_case_equal(reference->table_name, table_name)) {
        return false;
    }
    return true;
}

static bool
insert_column_uses_numeric_implicit_default(const struct mylite_insert_table_column *column)
{
    static const char *const numeric_types[] = {
        "tinyint", "smallint", "mediumint", "int",     "bigint", "decimal",
        "float",   "double",   "bool",      "boolean", "year",
    };

    if (column == NULL || column->data_type == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(numeric_types) / sizeof(numeric_types[0]); ++index) {
        if (mylite_ascii_case_equal(column->data_type, numeric_types[index])) {
            return true;
        }
    }
    return false;
}

static bool insert_row_uses_all_defaults(const struct mylite_insert_values_plan *plan,
                                         size_t row_index)
{
    return plan->rows[row_index].value_count == 0U;
}

static size_t insert_row_target_column_count(const struct mylite_insert_values_plan *plan,
                                             const struct mylite_insert_table *table,
                                             size_t row_index)
{
    if (plan->has_column_list) {
        return plan->column_count;
    }
    if (plan->rows[row_index].value_count == 0U) {
        return 0U;
    }
    return table->column_count;
}

static int set_insert_wrong_value_count_error(mylite_db *database, size_t row_index)
{
    enum { row_number_buffer_size = 64 };
    char buffer[row_number_buffer_size];

    (void)snprintf(buffer, sizeof(buffer), "%zu", row_index + 1U);
    if (mylite_diagnostics_set_error_message_parts(database,
                                                   "Column count doesn't match value count at row ",
                                                   buffer, "") == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}

static int set_insert_no_default_error(mylite_db *database, const char *column_name)
{
    int status = mylite_diagnostics_set_error_message_parts(database, "Field '", column_name,
                                                            "' doesn't have a default value");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_insert_unsupported_generated_default_error(mylite_db *database,
                                                          const char *column_name)
{
    int status = mylite_diagnostics_set_error_message_parts(
        database, "Unsupported generated default expression for '", column_name, "'");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_insert_unsupported_expression_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(database, "Unsupported INSERT value expression") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}

static int append_insert_no_default_warning(mylite_db *database, const char *column_name)
{
    char *message = sqlite3_mprintf("Field '%q' doesn't have a default value", column_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status =
        mylite_diagnostics_append_warning(database, MYLITE_MYSQL_ER_NO_DEFAULT_FOR_FIELD, message);
    sqlite3_free(message);
    return status;
}

static int append_insert_no_default_warning_once(mylite_db *database,
                                                 const struct mylite_insert_table_column *column,
                                                 struct mylite_insert_execution_state *state,
                                                 size_t column_index)
{
    if (state != NULL && state->warned_omitted_no_default_columns != NULL &&
        state->warned_omitted_no_default_columns[column_index]) {
        return MYLITE_OK;
    }

    int status = append_insert_no_default_warning(database, column->name);

    if (status != MYLITE_OK) {
        return status;
    }
    if (state != NULL && state->warned_omitted_no_default_columns != NULL) {
        state->warned_omitted_no_default_columns[column_index] = true;
    }
    return MYLITE_OK;
}

static int append_insert_null_warning(mylite_db *database, const char *column_name)
{
    char *message = sqlite3_mprintf("Column '%q' cannot be null", column_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_append_warning(database, MYLITE_MYSQL_ER_BAD_NULL_ERROR, message);
    sqlite3_free(message);
    return status;
}

static int append_insert_null_warning_once(mylite_db *database,
                                           const struct mylite_insert_table_column *column,
                                           struct mylite_insert_execution_state *state,
                                           size_t column_index)
{
    if (state != NULL && state->warned_null_columns != NULL &&
        state->warned_null_columns[column_index]) {
        return MYLITE_OK;
    }

    int status = append_insert_null_warning(database, column->name);

    if (status != MYLITE_OK) {
        return status;
    }
    if (state != NULL && state->warned_null_columns != NULL) {
        state->warned_null_columns[column_index] = true;
    }
    return MYLITE_OK;
}

static char *insert_current_timestamp_text(void)
{
    enum { timestamp_length = 19U };
    time_t now = time(NULL);
    struct tm tm_value;
    char *timestamp = malloc(timestamp_length + 1U);

    if (timestamp == NULL) {
        return NULL;
    }
#ifdef _WIN32
    if (gmtime_s(&tm_value, &now) != 0) {
        free(timestamp);
        return NULL;
    }
#else
    if (gmtime_r(&now, &tm_value) == NULL) {
        free(timestamp);
        return NULL;
    }
#endif
    if (strftime(timestamp, timestamp_length + 1U, "%Y-%m-%d %H:%M:%S", &tm_value) == 0U) {
        free(timestamp);
        return NULL;
    }
    return timestamp;
}

static sqlite3_destructor_type sqlite_transient_destructor(void)
{
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
