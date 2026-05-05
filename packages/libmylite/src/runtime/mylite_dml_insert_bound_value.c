#include "mylite_dml_insert_bound_value.h"

#include "mylite_diagnostics.h"
#include "mylite_span.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static sqlite3_destructor_type sqlite_transient_destructor(void);

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

static sqlite3_destructor_type sqlite_transient_destructor(void)
{
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
