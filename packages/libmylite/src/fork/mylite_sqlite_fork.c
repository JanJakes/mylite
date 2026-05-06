#include "mylite_sqlite_fork.h"

#include "mylite_charset.h"

#include <stdbool.h>
#include <string.h>

enum mylite_sqlite_collation_flags {
    mylite_sqlite_collation_case_insensitive = 1U << 0U,
    mylite_sqlite_collation_pad_space = 1U << 1U,
};

enum mylite_sqlite_utf8_byte_masks {
    mylite_sqlite_utf8_ascii_mask = 0x80U,
    mylite_sqlite_utf8_two_byte_mask = 0xE0U,
    mylite_sqlite_utf8_two_byte_prefix = 0xC0U,
    mylite_sqlite_utf8_three_byte_mask = 0xF0U,
    mylite_sqlite_utf8_three_byte_prefix = 0xE0U,
    mylite_sqlite_utf8_four_byte_mask = 0xF8U,
    mylite_sqlite_utf8_four_byte_prefix = 0xF0U,
};

struct mylite_sqlite_pad_trim_request {
    const void *value;
    int length;
    unsigned int flags;
};

static const unsigned int mylite_sqlite_collation_flag_contexts[] = {
    0U,
    mylite_sqlite_collation_case_insensitive,
    mylite_sqlite_collation_pad_space,
    mylite_sqlite_collation_case_insensitive | mylite_sqlite_collation_pad_space,
};

static int register_mysql_collations(sqlite3 *database);

static int register_mysql_functions(sqlite3 *database);

static int register_scalar_function(
    sqlite3 *database,
    const char *name,
    int argument_count,
    void (*callback)(sqlite3_context *, int, sqlite3_value **)
);

static void mysql_concat(sqlite3_context *context, int argument_count, sqlite3_value **arguments);

static void mysql_length(sqlite3_context *context, int argument_count, sqlite3_value **arguments);

static void mysql_char_length(
    sqlite3_context *context,
    int argument_count,
    sqlite3_value **arguments
);

static sqlite3_int64 count_utf8_characters(const unsigned char *text, int length);

static int compare_mysql_collation(
    void *context,
    int left_length,
    const void *left_value,
    int right_length,
    const void *right_value
);

static int compare_binary_bytes(
    const unsigned char *left,
    int left_length,
    const unsigned char *right,
    int right_length
);

static int compare_ascii_ci_bytes(
    const unsigned char *left,
    int left_length,
    const unsigned char *right,
    int right_length
);

static int trim_pad_space(struct mylite_sqlite_pad_trim_request request);

static bool collation_is_case_insensitive(const char *name);

static bool collation_is_pad_space(const struct mylite_collation *collation);

static bool string_has_suffix(const char *text, const char *suffix);

static unsigned char ascii_lower(unsigned char byte);

static int sqlite_sequence_exists(sqlite3 *database, bool *out_exists);

int mylite_sqlite_fork_configure(sqlite3 *database) {
    int rc = SQLITE_OK;

    if (database == NULL) {
        return SQLITE_MISUSE;
    }

    rc = register_mysql_collations(database);
    if (rc != SQLITE_OK) {
        return rc;
    }
    return register_mysql_functions(database);
}

int mylite_sqlite_fork_truncate_table(sqlite3 *database, const char *table_name) {
    bool has_sqlite_sequence = false;
    char *sql = NULL;
    int rc = SQLITE_OK;

    if (database == NULL || table_name == NULL || table_name[0] == '\0') {
        return SQLITE_MISUSE;
    }

    sql = sqlite3_mprintf("DELETE FROM \"%w\"", table_name);
    if (sql == NULL) {
        return SQLITE_NOMEM;
    }
    rc = sqlite3_exec(database, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return rc;
    }

    rc = sqlite_sequence_exists(database, &has_sqlite_sequence);
    if (rc != SQLITE_OK || !has_sqlite_sequence) {
        return rc;
    }

    sql = sqlite3_mprintf("DELETE FROM sqlite_sequence WHERE name = %Q", table_name);
    if (sql == NULL) {
        return SQLITE_NOMEM;
    }
    rc = sqlite3_exec(database, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    return rc;
}

static int register_mysql_collations(sqlite3 *database) {
    for (size_t index = 0U; index < mylite_collation_count(); ++index) {
        const struct mylite_collation *collation = mylite_collation_at(index);
        unsigned int flags = 0U;
        int rc = SQLITE_OK;

        if (collation == NULL) {
            continue;
        }
        if (collation_is_case_insensitive(collation->name)) {
            flags |= mylite_sqlite_collation_case_insensitive;
        }
        if (collation_is_pad_space(collation)) {
            flags |= mylite_sqlite_collation_pad_space;
        }

        rc = sqlite3_create_collation_v2(
            database,
            collation->name,
            SQLITE_UTF8,
            (void *)&mylite_sqlite_collation_flag_contexts[flags],
            compare_mysql_collation,
            NULL
        );
        if (rc != SQLITE_OK) {
            return rc;
        }
    }

    return SQLITE_OK;
}

static int register_mysql_functions(sqlite3 *database) {
    int rc = register_scalar_function(database, "CONCAT", -1, mysql_concat);

    if (rc == SQLITE_OK) {
        rc = register_scalar_function(database, "LENGTH", 1, mysql_length);
    }
    if (rc == SQLITE_OK) {
        rc = register_scalar_function(database, "OCTET_LENGTH", 1, mysql_length);
    }
    if (rc == SQLITE_OK) {
        rc = register_scalar_function(database, "CHAR_LENGTH", 1, mysql_char_length);
    }
    if (rc == SQLITE_OK) {
        rc = register_scalar_function(database, "CHARACTER_LENGTH", 1, mysql_char_length);
    }
    return rc;
}

static int register_scalar_function(
    sqlite3 *database,
    const char *name,
    int argument_count,
    void (*callback)(sqlite3_context *, int, sqlite3_value **)
) {
    return sqlite3_create_function_v2(
        database,
        name,
        argument_count,
        SQLITE_UTF8 | SQLITE_DETERMINISTIC,
        NULL,
        callback,
        NULL,
        NULL,
        NULL
    );
}

static void mysql_concat(sqlite3_context *context, int argument_count, sqlite3_value **arguments) {
    sqlite3 *database = sqlite3_context_db_handle(context);
    sqlite3_str *result = sqlite3_str_new(database);
    int rc = SQLITE_OK;

    if (result == NULL) {
        sqlite3_result_error_nomem(context);
        return;
    }

    for (int index = 0; index < argument_count; ++index) {
        const unsigned char *text = NULL;
        int bytes = 0;

        if (sqlite3_value_type(arguments[index]) == SQLITE_NULL) {
            sqlite3_str_reset(result);
            sqlite3_free(sqlite3_str_finish(result));
            sqlite3_result_null(context);
            return;
        }

        text = sqlite3_value_text(arguments[index]);
        bytes = sqlite3_value_bytes(arguments[index]);
        if (text == NULL && bytes > 0) {
            sqlite3_free(sqlite3_str_finish(result));
            sqlite3_result_error_nomem(context);
            return;
        }
        sqlite3_str_append(result, (const char *)text, bytes);
    }

    rc = sqlite3_str_errcode(result);
    char *text = sqlite3_str_finish(result);
    if (rc != SQLITE_OK) {
        sqlite3_free(text);
        sqlite3_result_error_code(context, rc);
        return;
    }
    sqlite3_result_text(context, text, -1, sqlite3_free);
}

static void mysql_length(sqlite3_context *context, int argument_count, sqlite3_value **arguments) {
    const unsigned char *text = NULL;
    int bytes = 0;

    (void)argument_count;
    if (sqlite3_value_type(arguments[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    if (sqlite3_value_type(arguments[0]) == SQLITE_BLOB) {
        sqlite3_result_int64(context, sqlite3_value_bytes(arguments[0]));
        return;
    }

    text = sqlite3_value_text(arguments[0]);
    bytes = sqlite3_value_bytes(arguments[0]);
    if (text == NULL && bytes > 0) {
        sqlite3_result_error_nomem(context);
        return;
    }
    sqlite3_result_int64(context, bytes);
}

static void mysql_char_length(
    sqlite3_context *context,
    int argument_count,
    sqlite3_value **arguments
) {
    const unsigned char *text = NULL;
    int bytes = 0;

    (void)argument_count;
    if (sqlite3_value_type(arguments[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    text = sqlite3_value_text(arguments[0]);
    bytes = sqlite3_value_bytes(arguments[0]);
    if (text == NULL && bytes > 0) {
        sqlite3_result_error_nomem(context);
        return;
    }
    sqlite3_result_int64(context, count_utf8_characters(text, bytes));
}

static sqlite3_int64 count_utf8_characters(const unsigned char *text, int length) {
    sqlite3_int64 count = 0;

    for (int offset = 0; offset < length;) {
        unsigned char byte = text[offset];
        int advance = 1;

        if ((byte & mylite_sqlite_utf8_ascii_mask) == 0U) {
            advance = 1;
        } else if (
            (byte & mylite_sqlite_utf8_two_byte_mask) == mylite_sqlite_utf8_two_byte_prefix &&
            offset + 1 < length
        ) {
            advance = 2;
        } else if (
            (byte & mylite_sqlite_utf8_three_byte_mask) == mylite_sqlite_utf8_three_byte_prefix &&
            offset + 2 < length
        ) {
            advance = 3;
        } else if (
            (byte & mylite_sqlite_utf8_four_byte_mask) == mylite_sqlite_utf8_four_byte_prefix &&
            offset + 3 < length
        ) {
            advance = 4;
        }

        offset += advance;
        ++count;
    }
    return count;
}

static int compare_mysql_collation(
    void *context,
    int left_length,
    const void *left_value,
    int right_length,
    const void *right_value
) {
    const unsigned int *context_flags = context;
    unsigned int flags = *context_flags;

    left_length = trim_pad_space((struct mylite_sqlite_pad_trim_request){
        .value = left_value,
        .length = left_length,
        .flags = flags,
    });
    right_length = trim_pad_space((struct mylite_sqlite_pad_trim_request){
        .value = right_value,
        .length = right_length,
        .flags = flags,
    });

    if ((flags & mylite_sqlite_collation_case_insensitive) != 0U) {
        return compare_ascii_ci_bytes(left_value, left_length, right_value, right_length);
    }
    return compare_binary_bytes(left_value, left_length, right_value, right_length);
}

static int compare_binary_bytes(
    const unsigned char *left,
    int left_length,
    const unsigned char *right,
    int right_length
) {
    int common_length = left_length < right_length ? left_length : right_length;
    int result = memcmp(left, right, (size_t)common_length);

    if (result != 0) {
        return result;
    }
    return (left_length > right_length) - (left_length < right_length);
}

static int compare_ascii_ci_bytes(
    const unsigned char *left,
    int left_length,
    const unsigned char *right,
    int right_length
) {
    int common_length = left_length < right_length ? left_length : right_length;

    for (int index = 0; index < common_length; ++index) {
        unsigned char left_byte = ascii_lower(left[index]);
        unsigned char right_byte = ascii_lower(right[index]);

        if (left_byte != right_byte) {
            return (left_byte > right_byte) - (left_byte < right_byte);
        }
    }
    return (left_length > right_length) - (left_length < right_length);
}

static int trim_pad_space(struct mylite_sqlite_pad_trim_request request) {
    const unsigned char *bytes = request.value;
    int length = request.length;

    if ((request.flags & mylite_sqlite_collation_pad_space) == 0U) {
        return length;
    }
    while (length > 0 && bytes[length - 1] == ' ') {
        --length;
    }
    return length;
}

static bool collation_is_case_insensitive(const char *name) {
    if (strcmp(name, "binary") == 0) {
        return false;
    }
    if (string_has_suffix(name, "_bin")) {
        return false;
    }
    return true;
}

static bool collation_is_pad_space(const struct mylite_collation *collation) {
    return strcmp(collation->pad_attribute, "PAD SPACE") == 0;
}

static bool string_has_suffix(const char *text, const char *suffix) {
    size_t text_length = strlen(text);
    size_t suffix_length = strlen(suffix);

    if (text_length < suffix_length) {
        return false;
    }
    if (strcmp(text + text_length - suffix_length, suffix) == 0) {
        return true;
    }
    return false;
}

static unsigned char ascii_lower(unsigned char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (unsigned char)(byte + ('a' - 'A'));
    }
    return byte;
}

static int sqlite_sequence_exists(sqlite3 *database, bool *out_exists) {
    sqlite3_stmt *statement = NULL;
    int rc = SQLITE_OK;

    *out_exists = false;
    rc = sqlite3_prepare_v3(
        database,
        "SELECT 1 FROM sqlite_schema WHERE type = 'table' AND name = 'sqlite_sequence'",
        -1,
        SQLITE_PREPARE_PERSISTENT,
        &statement,
        NULL
    );
    if (rc != SQLITE_OK) {
        return rc;
    }

    rc = sqlite3_step(statement);
    if (rc == SQLITE_ROW) {
        *out_exists = true;
        rc = SQLITE_DONE;
    }
    sqlite3_finalize(statement);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}
