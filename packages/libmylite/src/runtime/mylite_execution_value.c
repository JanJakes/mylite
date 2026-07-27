#include "mylite_execution_value.h"

#include "mylite_execution_diagnostics.h"
#include "mylite_execution_sqlite_internal.h"
#include "mylite_sqlite_registration.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void planned_value_deinit(struct planned_value *value) {
    if (value == NULL) {
        return;
    }

    if (!value->is_borrowed_text) {
        free(value->text);
    }
    *value = (struct planned_value){0};
}

int copy_planned_value(
    struct mylite_db *database,
    const struct planned_value *source,
    struct planned_value *out_value
) {
    char *text = NULL;

    if (source == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (source == out_value) {
        return MYLITE_OK;
    }
    if ((source->is_text || source->is_blob) && source->text_length != 0U) {
        if (source->text == NULL) {
            return MYLITE_ERROR;
        }
        text = malloc(source->text_length);
        if (text == NULL) {
            mylite_execution_diagnostics_set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memcpy(text, source->text, source->text_length);
    }

    planned_value_deinit(out_value);
    *out_value = *source;
    out_value->text = text;
    out_value->is_borrowed_text = false;
    return MYLITE_OK;
}

int bind_planned_value_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct planned_value *value
) {
    static const char empty_bytes[] = "";
    int sqlite_rc = SQLITE_OK;

    if (statement == NULL || parameter_index <= 0 || value == NULL) {
        return MYLITE_ERROR;
    }
    if (value->is_external_parameter && value->external_binding != NULL) {
        return bind_stmt_binding_parameter(statement, parameter_index, value->external_binding);
    }
    if (value->is_null) {
        sqlite_rc = sqlite3_bind_null(statement, parameter_index);
    } else if (value->is_text || value->is_blob) {
        if (value->text_length > (size_t)INT_MAX ||
            (value->text == NULL && value->text_length != 0U)) {
            return MYLITE_ERROR;
        }
        if (value->is_text) {
            sqlite_rc = sqlite3_bind_text(
                statement,
                parameter_index,
                value->text_length == 0U ? empty_bytes : value->text,
                (int)value->text_length,
                SQLITE_TRANSIENT
            );
        } else {
            sqlite_rc = sqlite3_bind_blob(
                statement,
                parameter_index,
                value->text_length == 0U ? empty_bytes : value->text,
                (int)value->text_length,
                SQLITE_TRANSIENT
            );
        }
    } else if (value->is_real) {
        sqlite_rc = sqlite3_bind_double(statement, parameter_index, value->real);
    } else {
        sqlite_rc = sqlite3_bind_int64(statement, parameter_index, (sqlite3_int64)value->integer);
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

int bind_stmt_binding_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct mylite_stmt_binding *binding
) {
    enum { uint64_text_capacity = 32 };

    static const unsigned char empty_blob[] = {0U};
    static const char empty_text[] = "";
    char uint64_text[uint64_text_capacity];
    int written = 0;
    int sqlite_rc = SQLITE_OK;

    if (statement == NULL || parameter_index <= 0 || binding == NULL) {
        return MYLITE_ERROR;
    }
    switch (binding->type) {
    case MYLITE_STMT_BINDING_NULL:
        sqlite_rc = sqlite3_bind_null(statement, parameter_index);
        break;
    case MYLITE_STMT_BINDING_INT64:
        sqlite_rc = sqlite3_bind_int64(
            statement,
            parameter_index,
            (sqlite3_int64)binding->scalar.int64_value
        );
        break;
    case MYLITE_STMT_BINDING_UINT64:
        if (binding->scalar.uint64_value <= (uint64_t)INT64_MAX) {
            sqlite_rc = sqlite3_bind_int64(
                statement,
                parameter_index,
                (sqlite3_int64)binding->scalar.uint64_value
            );
            break;
        }
        written =
            snprintf(uint64_text, sizeof(uint64_text), "%" PRIu64, binding->scalar.uint64_value);
        if (written <= 0 || (size_t)written >= sizeof(uint64_text)) {
            return MYLITE_ERROR;
        }
        sqlite_rc =
            sqlite3_bind_text(statement, parameter_index, uint64_text, written, SQLITE_TRANSIENT);
        break;
    case MYLITE_STMT_BINDING_DOUBLE:
        sqlite_rc = sqlite3_bind_double(statement, parameter_index, binding->scalar.double_value);
        break;
    case MYLITE_STMT_BINDING_TEXT:
        if (binding->size > (size_t)INT_MAX) {
            return MYLITE_ERROR;
        }
        sqlite_rc = sqlite3_bind_text(
            statement,
            parameter_index,
            binding->size == 0U ? empty_text : (const char *)binding->bytes,
            (int)binding->size,
            SQLITE_TRANSIENT
        );
        break;
    case MYLITE_STMT_BINDING_BLOB:
        if (binding->size > (size_t)INT_MAX) {
            return MYLITE_ERROR;
        }
        sqlite_rc = sqlite3_bind_blob(
            statement,
            parameter_index,
            binding->size == 0U ? empty_blob : binding->bytes,
            (int)binding->size,
            SQLITE_TRANSIENT
        );
        break;
    case MYLITE_STMT_BINDING_UNBOUND:
        return MYLITE_MISUSE;
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

int validate_sqlite_parameter_count(sqlite3_stmt *statement, int bound_parameter_count) {
    if (statement == NULL || bound_parameter_count < 0 ||
        sqlite3_bind_parameter_count(statement) != bound_parameter_count) {
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

int bind_int64_parameter(sqlite3_stmt *statement, int parameter_index, int64_t value) {
    if (statement == NULL || parameter_index <= 0) {
        return MYLITE_ERROR;
    }
    return mylite_sqlite_status_to_mylite(
        sqlite3_bind_int64(statement, parameter_index, (sqlite3_int64)value)
    );
}

int bind_text_parameter(sqlite3_stmt *statement, int parameter_index, const char *value) {
    if (statement == NULL || parameter_index <= 0 || value == NULL) {
        return MYLITE_ERROR;
    }
    return mylite_sqlite_status_to_mylite(
        sqlite3_bind_text(statement, parameter_index, value, -1, SQLITE_TRANSIENT)
    );
}
