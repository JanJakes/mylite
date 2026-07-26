#include "mylite_execution_result_rows.h"

#include "mylite_catalog.h"
#include "mylite_execution_scalar.h"
#include "mylite_result.h"
#include "sqlite3.h"

#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    approximate_numeric_text_capacity = DBL_MAX_10_EXP + 34,
    maximum_int64_decimal_digits = 20,
};

static int result_row_storage_reserve(
    struct mylite_execution_result_row_storage *storage,
    size_t column_count
);
static int read_sqlite_result_cell_from_descriptors(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *const *columns,
    size_t descriptor_count,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
);
static int read_sqlite_integer_result_cell(
    sqlite3_stmt *statement,
    size_t column_index,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
);
static bool format_int64_decimal(int64_t value, char *text, size_t text_capacity, size_t *out_size);
static int read_sqlite_float_result_cell(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *column,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
);
static int read_sqlite_text_result_cell(
    sqlite3_stmt *statement,
    size_t column_index,
    struct mylite_result_cell *out_value
);
static int read_sqlite_blob_result_cell(
    sqlite3_stmt *statement,
    size_t column_index,
    struct mylite_result_cell *out_value
);
static bool column_name_exists(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *name
);

int mylite_execution_append_sqlite_result_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    mylite_result *result,
    const struct mylite_catalog_column_descriptor *const *columns,
    size_t descriptor_count,
    struct mylite_execution_result_row_storage *storage
) {
    size_t column_count = mylite_result_column_count(result);
    int rc = mylite_execution_read_sqlite_result_row(
        database,
        statement,
        column_count,
        columns,
        descriptor_count,
        storage
    );

    if (rc == MYLITE_OK) {
        rc = mylite_result_append_bytes_row(result, storage->values);
    }
    return rc;
}

int mylite_execution_read_sqlite_result_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_count,
    const struct mylite_catalog_column_descriptor *const *columns,
    size_t descriptor_count,
    struct mylite_execution_result_row_storage *storage
) {
    int rc = MYLITE_OK;

    if (storage == NULL) {
        return MYLITE_MISUSE;
    }
    rc = result_row_storage_reserve(storage, column_count);
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        char *text = &storage->texts[column_index * storage->text_capacity];

        rc = read_sqlite_result_cell_from_descriptors(
            database,
            statement,
            column_index,
            columns,
            descriptor_count,
            text,
            storage->text_capacity,
            &storage->values[column_index]
        );
    }
    return rc;
}

void mylite_execution_result_row_storage_deinit(struct mylite_execution_result_row_storage *storage
) {
    if (storage == NULL) {
        return;
    }

    free(storage->values);
    free(storage->texts);
    *storage = (struct mylite_execution_result_row_storage){0};
}

int mylite_execution_read_sqlite_result_cell(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *column,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
) {
    int sqlite_type = SQLITE_NULL;

    if (column_index > (size_t)INT_MAX || out_value == NULL) {
        return MYLITE_ERROR;
    }
    *out_value = (struct mylite_result_cell){.bytes = NULL, .size = 0U, .is_null = true};

    sqlite_type = sqlite3_column_type(statement, (int)column_index);
    if (sqlite_type == SQLITE_NULL) {
        return MYLITE_OK;
    }
    if (sqlite_type == SQLITE_INTEGER) {
        return read_sqlite_integer_result_cell(
            statement,
            column_index,
            text,
            text_capacity,
            out_value
        );
    }
    if (sqlite_type == SQLITE_FLOAT) {
        return read_sqlite_float_result_cell(
            database,
            statement,
            column_index,
            column,
            text,
            text_capacity,
            out_value
        );
    }
    if (sqlite_type == SQLITE_TEXT) {
        return read_sqlite_text_result_cell(statement, column_index, out_value);
    }
    if (sqlite_type == SQLITE_BLOB) {
        return read_sqlite_blob_result_cell(statement, column_index, out_value);
    }
    return MYLITE_ERROR;
}

int mylite_execution_choose_sqlite_rowid_alias(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *unsupported_message,
    const char **out_alias
) {
    static const char *const rowid_aliases[] = {"rowid", "_rowid_", "oid"};

    if (out_alias == NULL) {
        return MYLITE_MISUSE;
    }
    *out_alias = NULL;
    for (size_t index = 0U; index < sizeof(rowid_aliases) / sizeof(rowid_aliases[0]); ++index) {
        if (!column_name_exists(columns, column_count, rowid_aliases[index])) {
            *out_alias = rowid_aliases[index];
            return MYLITE_OK;
        }
    }

    mylite_execution_set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static int result_row_storage_reserve(
    struct mylite_execution_result_row_storage *storage,
    size_t column_count
) {
    struct mylite_result_cell *values = NULL;
    char *texts = NULL;
    size_t text_capacity = approximate_numeric_text_capacity;

    if (storage == NULL) {
        return MYLITE_MISUSE;
    }
    if (column_count > (size_t)INT_MAX || column_count > SIZE_MAX / text_capacity) {
        return MYLITE_NOMEM;
    }
    if (column_count <= storage->column_capacity) {
        return MYLITE_OK;
    }
    if (column_count > SIZE_MAX / sizeof(*values)) {
        return MYLITE_NOMEM;
    }
    values = realloc(storage->values, column_count * sizeof(*values));
    if (values == NULL && column_count != 0U) {
        return MYLITE_NOMEM;
    }
    storage->values = values;
    texts = realloc(storage->texts, column_count * text_capacity);
    if (texts == NULL && column_count != 0U) {
        return MYLITE_NOMEM;
    }
    storage->texts = texts;
    storage->column_capacity = column_count;
    storage->text_capacity = text_capacity;
    return MYLITE_OK;
}

static int read_sqlite_result_cell_from_descriptors(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *const *columns,
    size_t descriptor_count,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
) {
    const struct mylite_catalog_column_descriptor *column =
        columns != NULL && column_index < descriptor_count ? columns[column_index] : NULL;

    return mylite_execution_read_sqlite_result_cell(
        database,
        statement,
        column_index,
        column,
        text,
        text_capacity,
        out_value
    );
}

static int read_sqlite_integer_result_cell(
    sqlite3_stmt *statement,
    size_t column_index,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
) {
    size_t written = 0U;

    if (!format_int64_decimal(
            (int64_t)sqlite3_column_int64(statement, (int)column_index),
            text,
            text_capacity,
            &written
        )) {
        return MYLITE_ERROR;
    }
    *out_value = (struct mylite_result_cell){
        .bytes = text,
        .size = written,
        .is_null = false,
    };
    return MYLITE_OK;
}

static bool format_int64_decimal(
    int64_t value,
    char *text,
    size_t text_capacity,
    size_t *out_size
) {
    char reversed[maximum_int64_decimal_digits];
    uint64_t magnitude = 0U;
    size_t digit_count = 0U;
    size_t output_size = 0U;
    bool negative = value < 0;

    if (text == NULL || out_size == NULL) {
        return false;
    }
    magnitude = negative ? (uint64_t)(-(value + 1)) + UINT64_C(1) : (uint64_t)value;
    do {
        reversed[digit_count++] = (char)('0' + (magnitude % UINT64_C(10)));
        magnitude /= UINT64_C(10);
    } while (magnitude != 0U);

    output_size = digit_count + (negative ? 1U : 0U);
    if (output_size >= text_capacity) {
        return false;
    }
    if (negative) {
        text[0] = '-';
    }
    for (size_t index = 0U; index < digit_count; ++index) {
        text[output_size - index - 1U] = reversed[index];
    }
    text[output_size] = '\0';
    *out_size = output_size;
    return true;
}

static int read_sqlite_float_result_cell(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *column,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
) {
    int rc = mylite_execution_format_approximate_result_text(
        database,
        column,
        sqlite3_column_double(statement, (int)column_index),
        text,
        text_capacity
    );

    if (rc == MYLITE_OK) {
        *out_value = (struct mylite_result_cell){
            .bytes = text,
            .size = strlen(text),
            .is_null = false,
        };
    }
    return rc;
}

static int read_sqlite_text_result_cell(
    sqlite3_stmt *statement,
    size_t column_index,
    struct mylite_result_cell *out_value
) {
    const unsigned char *sqlite_text = sqlite3_column_text(statement, (int)column_index);
    int byte_count = sqlite3_column_bytes(statement, (int)column_index);
    size_t character_count = 0U;

    if (sqlite_text == NULL || byte_count < 0 ||
        mylite_execution_validate_utf8_text(
            (const char *)sqlite_text,
            (size_t)byte_count,
            &character_count
        ) != MYLITE_OK) {
        return MYLITE_ERROR;
    }

    (void)character_count;
    *out_value = (struct mylite_result_cell){
        .bytes = sqlite_text,
        .size = (size_t)byte_count,
        .is_null = false,
    };
    return MYLITE_OK;
}

static int read_sqlite_blob_result_cell(
    sqlite3_stmt *statement,
    size_t column_index,
    struct mylite_result_cell *out_value
) {
    const void *sqlite_blob = sqlite3_column_blob(statement, (int)column_index);
    int byte_count = sqlite3_column_bytes(statement, (int)column_index);

    if ((sqlite_blob == NULL && byte_count != 0) || byte_count < 0) {
        return MYLITE_ERROR;
    }

    *out_value = (struct mylite_result_cell){
        .bytes = sqlite_blob,
        .size = (size_t)byte_count,
        .is_null = false,
    };
    return MYLITE_OK;
}

static bool column_name_exists(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *name
) {
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        if (mylite_execution_text_equals_ascii_case_insensitive(columns[column_index].name, name)) {
            return true;
        }
    }
    return false;
}
