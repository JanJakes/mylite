#include "mylite_string_trim.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
};

static void string_trim_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static bool string_trim_kind_from_sqlite_context(
    sqlite3_context *context,
    enum mylite_string_trim_kind *out_kind
);
static int trimmed_bounds(
    struct mylite_db *database,
    enum mylite_string_trim_kind trim_kind,
    const char *value,
    size_t value_length,
    const char *remove_string,
    size_t remove_string_length,
    size_t *out_begin,
    size_t *out_end
);
static void trim_leading_sequence(
    const char *value,
    size_t value_length,
    const char *remove_string,
    size_t remove_string_length,
    size_t *inout_begin
);
static void trim_trailing_sequence(
    const char *value,
    const char *remove_string,
    size_t remove_string_length,
    size_t begin,
    size_t *inout_end
);
static bool trim_sequence_matches(
    const char *value,
    size_t offset,
    const char *remove_string,
    size_t remove_string_length
);
static void set_string_trim_unsupported_error(struct mylite_db *database, const char *message);

int mylite_string_trim_value(
    struct mylite_db *database,
    enum mylite_string_trim_kind trim_kind,
    const char *value,
    size_t value_length,
    const char *remove_string,
    size_t remove_string_length,
    char **out_text
) {
    char *result = NULL;
    size_t begin = 0U;
    size_t end = 0U;
    size_t result_length = 0U;
    int rc = MYLITE_OK;

    if (value == NULL || remove_string == NULL || out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;

    rc = trimmed_bounds(
        database,
        trim_kind,
        value,
        value_length,
        remove_string,
        remove_string_length,
        &begin,
        &end
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    result_length = end - begin;
    result = (char *)malloc(result_length + 1U);
    if (result == NULL) {
        return MYLITE_NOMEM;
    }
    if (result_length != 0U) {
        memcpy(result, &value[begin], result_length);
    }
    result[result_length] = '\0';
    *out_text = result;
    return MYLITE_OK;
}

int mylite_sqlite_register_string_trim_functions(sqlite3 *sqlite) {
    static const enum mylite_string_trim_kind both_kind = MYLITE_STRING_TRIM_BOTH;
    static const enum mylite_string_trim_kind leading_kind = MYLITE_STRING_TRIM_LEADING;
    static const enum mylite_string_trim_kind trailing_kind = MYLITE_STRING_TRIM_TRAILING;
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_trim",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&both_kind,
            .scalar_callback = string_trim_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_ltrim",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&leading_kind,
            .scalar_callback = string_trim_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_rtrim",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&trailing_kind,
            .scalar_callback = string_trim_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
    };

    return mylite_sqlite_register_functions(
        sqlite,
        registrations,
        sizeof(registrations) / sizeof(registrations[0])
    );
}

static void string_trim_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_db *database = NULL;
    enum mylite_string_trim_kind trim_kind = MYLITE_STRING_TRIM_BOTH;
    const unsigned char *value = NULL;
    const unsigned char *remove_string = NULL;
    int value_length = 0;
    int remove_string_length = 0;
    char *result = NULL;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        sqlite3_result_error(context, "invalid MyLite string trim callback", -1);
        return;
    }
    if (!string_trim_kind_from_sqlite_context(context, &trim_kind)) {
        sqlite3_result_error(context, "invalid MyLite string trim kind", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    database = mylite_sqlite_bootstrap_owner_from_context(context);
    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite string trim owner", -1);
        return;
    }

    value = sqlite3_value_text(argv[0]);
    remove_string = sqlite3_value_text(argv[1]);
    value_length = sqlite3_value_bytes(argv[0]);
    remove_string_length = sqlite3_value_bytes(argv[1]);
    if (value == NULL || remove_string == NULL || value_length < 0 || remove_string_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }

    rc = mylite_string_trim_value(
        database,
        trim_kind,
        (const char *)value,
        (size_t)value_length,
        (const char *)remove_string,
        (size_t)remove_string_length,
        &result
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "MyLite string trim failed", -1);
        }
        free(result);
        return;
    }

    sqlite3_result_text(context, result, -1, SQLITE_TRANSIENT);
    free(result);
}

static bool string_trim_kind_from_sqlite_context(
    sqlite3_context *context,
    enum mylite_string_trim_kind *out_kind
) {
    const enum mylite_string_trim_kind *trim_kind = NULL;

    if (context == NULL || out_kind == NULL) {
        return false;
    }
    trim_kind = (const enum mylite_string_trim_kind *)sqlite3_user_data(context);
    if (trim_kind == NULL ||
        (*trim_kind != MYLITE_STRING_TRIM_BOTH && *trim_kind != MYLITE_STRING_TRIM_LEADING &&
         *trim_kind != MYLITE_STRING_TRIM_TRAILING)) {
        return false;
    }
    *out_kind = *trim_kind;
    return true;
}

static int trimmed_bounds(
    struct mylite_db *database,
    enum mylite_string_trim_kind trim_kind,
    const char *value,
    size_t value_length,
    const char *remove_string,
    size_t remove_string_length,
    size_t *out_begin,
    size_t *out_end
) {
    size_t begin = 0U;
    size_t end = value_length;

    if (value == NULL || remove_string == NULL || out_begin == NULL || out_end == NULL) {
        return MYLITE_MISUSE;
    }
    if (memchr(value, '\0', value_length) != NULL ||
        memchr(remove_string, '\0', remove_string_length) != NULL) {
        set_string_trim_unsupported_error(database, "trim functions do not support NUL bytes");
        return MYLITE_ERROR;
    }
    if (trim_kind != MYLITE_STRING_TRIM_BOTH && trim_kind != MYLITE_STRING_TRIM_LEADING &&
        trim_kind != MYLITE_STRING_TRIM_TRAILING) {
        return MYLITE_MISUSE;
    }
    if (remove_string_length == 0U) {
        *out_begin = begin;
        *out_end = end;
        return MYLITE_OK;
    }

    if (trim_kind == MYLITE_STRING_TRIM_BOTH || trim_kind == MYLITE_STRING_TRIM_LEADING) {
        trim_leading_sequence(value, value_length, remove_string, remove_string_length, &begin);
    }
    if (trim_kind == MYLITE_STRING_TRIM_BOTH || trim_kind == MYLITE_STRING_TRIM_TRAILING) {
        trim_trailing_sequence(value, remove_string, remove_string_length, begin, &end);
    }

    *out_begin = begin;
    *out_end = end;
    return MYLITE_OK;
}

static void trim_leading_sequence(
    const char *value,
    size_t value_length,
    const char *remove_string,
    size_t remove_string_length,
    size_t *inout_begin
) {
    size_t begin = inout_begin == NULL ? 0U : *inout_begin;

    if (value == NULL || remove_string == NULL || remove_string_length == 0U ||
        inout_begin == NULL) {
        return;
    }
    while (value_length - begin >= remove_string_length &&
           trim_sequence_matches(value, begin, remove_string, remove_string_length)) {
        begin += remove_string_length;
    }
    *inout_begin = begin;
}

static void trim_trailing_sequence(
    const char *value,
    const char *remove_string,
    size_t remove_string_length,
    size_t begin,
    size_t *inout_end
) {
    size_t end = inout_end == NULL ? 0U : *inout_end;

    if (value == NULL || remove_string == NULL || remove_string_length == 0U || inout_end == NULL) {
        return;
    }
    while (end >= begin && end - begin >= remove_string_length &&
           trim_sequence_matches(
               value,
               end - remove_string_length,
               remove_string,
               remove_string_length
           )) {
        end -= remove_string_length;
    }
    *inout_end = end;
}

static bool trim_sequence_matches(
    const char *value,
    size_t offset,
    const char *remove_string,
    size_t remove_string_length
) {
    return memcmp(&value[offset], remove_string, remove_string_length) == 0;
}

static void set_string_trim_unsupported_error(struct mylite_db *database, const char *message) {
    if (database == NULL || message == NULL) {
        return;
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        message
    );
}
