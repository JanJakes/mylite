#include "mylite_uint64_text.h"

#include "mylite_span.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>

enum {
    mylite_uint64_decimal_base = 10,
    mylite_uint64_text_buffer_size = 32,
};

static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_sqlite_bind_uint64(sqlite3_stmt *stmt, int index, uint64_t value) {
    char buffer[mylite_uint64_text_buffer_size];
    size_t length = 0U;
    int status = SQLITE_OK;

    if (value <= (uint64_t)INT64_MAX) {
        return sqlite3_bind_int64(stmt, index, (sqlite3_int64)value);
    }
    status = mylite_format_uint64(value, buffer, sizeof(buffer), &length);
    if (status != SQLITE_OK) {
        return status;
    }
    return sqlite3_bind_text(stmt, index, buffer, (int)length, sqlite_transient_destructor());
}

bool mylite_sqlite_column_uint64(sqlite3_stmt *stmt, int column, uint64_t *out_value) {
    const unsigned char *text = NULL;
    int length = 0;

    if (stmt == NULL || out_value == NULL || sqlite3_column_type(stmt, column) == SQLITE_NULL) {
        return false;
    }
    text = sqlite3_column_text(stmt, column);
    length = sqlite3_column_bytes(stmt, column);
    if (text == NULL || length < 0) {
        return false;
    }
    return mylite_parse_uint64_text((const char *)text, (size_t)length, out_value);
}

char *mylite_copy_uint64_text(uint64_t value) {
    char buffer[mylite_uint64_text_buffer_size];
    size_t length = 0U;

    if (mylite_format_uint64(value, buffer, sizeof(buffer), &length) != SQLITE_OK) {
        return NULL;
    }
    return mylite_copy_span_text(buffer, length);
}

int mylite_format_uint64(uint64_t value, char *buffer, size_t buffer_size, size_t *out_length) {
    int length = 0;

    if (buffer == NULL || buffer_size == 0U || out_length == NULL) {
        return SQLITE_MISUSE;
    }
    length = snprintf(buffer, buffer_size, "%llu", (unsigned long long)value);
    if (length < 0 || (size_t)length >= buffer_size || length > INT_MAX) {
        return SQLITE_NOMEM;
    }
    *out_length = (size_t)length;
    return SQLITE_OK;
}

bool mylite_parse_uint64_text(const char *text, size_t length, uint64_t *out_value) {
    uint64_t value = 0U;
    size_t offset = 0U;
    bool saw_digit = false;

    if (text == NULL || out_value == NULL) {
        return false;
    }
    while (offset < length && isspace((unsigned char)text[offset])) {
        ++offset;
    }
    if (offset < length && text[offset] == '+') {
        ++offset;
    }
    while (offset < length && isdigit((unsigned char)text[offset])) {
        uint64_t digit = (uint64_t)(text[offset] - '0');

        saw_digit = true;
        if (value > (UINT64_MAX - digit) / mylite_uint64_decimal_base) {
            return false;
        }
        value = (value * mylite_uint64_decimal_base) + digit;
        ++offset;
    }
    while (offset < length && isspace((unsigned char)text[offset])) {
        ++offset;
    }
    if (!saw_digit || offset != length) {
        return false;
    }
    *out_value = value;
    return true;
}

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
