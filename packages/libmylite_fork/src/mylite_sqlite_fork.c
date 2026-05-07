#include <mylite_fork/mylite_sqlite_fork.h>

#include "mylite_fork_charset.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
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

enum mylite_sqlite_byte_units {
    mylite_sqlite_bits_per_byte = 8,
};

enum mylite_sqlite_mysql_conditions {
    mylite_sqlite_mysql_truncated_wrong_value = 1292,
};

struct mylite_sqlite_pad_trim_request {
    const void *value;
    int length;
    unsigned int flags;
};

struct mylite_sqlite_int64_range {
    sqlite3_int64 minimum;
    sqlite3_int64 maximum;
};

static const double mylite_sqlite_integer_round_half = 0.5;

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

static void mysql_concat_ws(
    sqlite3_context *context,
    int argument_count,
    sqlite3_value **arguments
);

static void mysql_if(sqlite3_context *context, int argument_count, sqlite3_value **arguments);

static void mysql_bit_length(
    sqlite3_context *context,
    int argument_count,
    sqlite3_value **arguments
);

static void mysql_isnull(sqlite3_context *context, int argument_count, sqlite3_value **arguments);

static void mysql_nullsafe_eq(
    sqlite3_context *context,
    int argument_count,
    sqlite3_value **arguments
);

static void mysql_length(sqlite3_context *context, int argument_count, sqlite3_value **arguments);

static void mysql_char_length(
    sqlite3_context *context,
    int argument_count,
    sqlite3_value **arguments
);

static void mysql_coerce_signed_integer(
    sqlite3_context *context,
    int argument_count,
    sqlite3_value **arguments
);

static void mysql_coerce_unsigned_integer(
    sqlite3_context *context,
    int argument_count,
    sqlite3_value **arguments
);

static void mysql_coerce_double(
    sqlite3_context *context,
    int argument_count,
    sqlite3_value **arguments
);

static void mysql_coerce_varchar(
    sqlite3_context *context,
    int argument_count,
    sqlite3_value **arguments
);

static sqlite3_int64 count_utf8_characters(const unsigned char *text, int length);

static bool coerce_value_to_rounded_int64(
    sqlite3_value *value,
    struct mylite_sqlite_int64_range range,
    sqlite3_int64 *out_value
);

static bool double_to_rounded_int64(
    double value,
    struct mylite_sqlite_int64_range range,
    sqlite3_int64 *out_value
);

static bool coerce_value_to_double(sqlite3_value *value, double *out_value);

static bool coerce_value_to_mysql_bool(sqlite3_context *context, sqlite3_value *value);

static bool coerce_value_to_mysql_comparison_double(
    sqlite3_context *context,
    sqlite3_value *value,
    double *out_value
);

static bool value_is_sql_numeric(sqlite3_value *value);

static bool compare_values_as_mysql_text(
    sqlite3_value *left,
    sqlite3_value *right,
    bool *out_equal
);

static bool compare_values_as_mysql_binary(
    sqlite3_value *left,
    sqlite3_value *right,
    bool *out_equal
);

static void publish_truncated_wrong_value_warning(sqlite3_context *context);

static bool parse_complete_double(sqlite3_value *value, double *out_value);

static bool text_has_non_space_tail(const char *cursor, const char *end);

static void result_coercion_error(sqlite3_context *context, const char *message);

static sqlite3_destructor_type sqlite_transient_destructor(void);

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

static bool collation_is_pad_space(const struct mylite_fork_collation *collation);

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
    rc = sqlite3_db_config(database, SQLITE_DBCONFIG_ENABLE_FKEY, 1, NULL);
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
    for (size_t index = 0U; index < mylite_fork_collation_count(); ++index) {
        const struct mylite_fork_collation *collation = mylite_fork_collation_at(index);
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
        rc = register_scalar_function(database, "CONCAT_WS", -1, mysql_concat_ws);
    }
    if (rc == SQLITE_OK) {
        rc = register_scalar_function(database, "IF", 3, mysql_if);
    }
    if (rc == SQLITE_OK) {
        rc = register_scalar_function(database, "BIT_LENGTH", 1, mysql_bit_length);
    }
    if (rc == SQLITE_OK) {
        rc = register_scalar_function(database, "ISNULL", 1, mysql_isnull);
    }
    if (rc == SQLITE_OK) {
        rc = register_scalar_function(database, "_mylite_nullsafe_eq", 2, mysql_nullsafe_eq);
    }
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
    if (rc == SQLITE_OK) {
        rc = register_scalar_function(
            database,
            "_mylite_coerce_signed_integer",
            3,
            mysql_coerce_signed_integer
        );
    }
    if (rc == SQLITE_OK) {
        rc = register_scalar_function(
            database,
            "_mylite_coerce_unsigned_integer",
            2,
            mysql_coerce_unsigned_integer
        );
    }
    if (rc == SQLITE_OK) {
        rc = register_scalar_function(database, "_mylite_coerce_double", 1, mysql_coerce_double);
    }
    if (rc == SQLITE_OK) {
        rc = register_scalar_function(database, "_mylite_coerce_varchar", 2, mysql_coerce_varchar);
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

static void mysql_concat_ws(
    sqlite3_context *context,
    int argument_count,
    sqlite3_value **arguments
) {
    sqlite3 *database = sqlite3_context_db_handle(context);
    sqlite3_str *result = NULL;
    const unsigned char *separator = NULL;
    int separator_bytes = 0;
    bool appended = false;
    int rc = SQLITE_OK;
    char *text = NULL;

    if (argument_count < 2) {
        sqlite3_result_error(context, "CONCAT_WS requires at least 2 arguments", -1);
        return;
    }
    if (sqlite3_value_type(arguments[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    separator = sqlite3_value_text(arguments[0]);
    separator_bytes = sqlite3_value_bytes(arguments[0]);
    if (separator == NULL && separator_bytes > 0) {
        sqlite3_result_error_nomem(context);
        return;
    }

    result = sqlite3_str_new(database);
    if (result == NULL) {
        sqlite3_result_error_nomem(context);
        return;
    }

    for (int index = 1; index < argument_count; ++index) {
        const unsigned char *value = NULL;
        int value_bytes = 0;

        if (sqlite3_value_type(arguments[index]) == SQLITE_NULL) {
            continue;
        }

        value = sqlite3_value_text(arguments[index]);
        value_bytes = sqlite3_value_bytes(arguments[index]);
        if (value == NULL && value_bytes > 0) {
            sqlite3_free(sqlite3_str_finish(result));
            sqlite3_result_error_nomem(context);
            return;
        }
        if (appended) {
            sqlite3_str_append(result, (const char *)separator, separator_bytes);
        }
        sqlite3_str_append(result, (const char *)value, value_bytes);
        appended = true;
    }

    rc = sqlite3_str_errcode(result);
    text = sqlite3_str_finish(result);
    if (rc != SQLITE_OK) {
        sqlite3_free(text);
        sqlite3_result_error_code(context, rc);
        return;
    }
    sqlite3_result_text(context, text, -1, sqlite3_free);
}

static void mysql_if(sqlite3_context *context, int argument_count, sqlite3_value **arguments) {
    int result_index = 2;

    (void)argument_count;
    if (coerce_value_to_mysql_bool(context, arguments[0])) {
        result_index = 1;
    }
    sqlite3_result_value(context, arguments[result_index]);
}

static void mysql_bit_length(
    sqlite3_context *context,
    int argument_count,
    sqlite3_value **arguments
) {
    const void *value = NULL;
    int bytes = 0;

    (void)argument_count;
    if (sqlite3_value_type(arguments[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    value = sqlite3_value_blob(arguments[0]);
    bytes = sqlite3_value_bytes(arguments[0]);
    if (value == NULL && bytes > 0) {
        sqlite3_result_error_nomem(context);
        return;
    }
    sqlite3_result_int64(context, (sqlite3_int64)bytes * mylite_sqlite_bits_per_byte);
}

static void mysql_isnull(sqlite3_context *context, int argument_count, sqlite3_value **arguments) {
    (void)argument_count;
    sqlite3_result_int(context, sqlite3_value_type(arguments[0]) == SQLITE_NULL);
}

static void mysql_nullsafe_eq(
    sqlite3_context *context,
    int argument_count,
    sqlite3_value **arguments
) {
    int left_type = sqlite3_value_type(arguments[0]);
    int right_type = sqlite3_value_type(arguments[1]);
    bool equal = false;

    (void)argument_count;
    if (left_type == SQLITE_NULL || right_type == SQLITE_NULL) {
        sqlite3_result_int(context, left_type == SQLITE_NULL && right_type == SQLITE_NULL);
        return;
    }
    if (value_is_sql_numeric(arguments[0]) || value_is_sql_numeric(arguments[1])) {
        double left = 0.0;
        double right = 0.0;

        if (!coerce_value_to_mysql_comparison_double(context, arguments[0], &left) ||
            !coerce_value_to_mysql_comparison_double(context, arguments[1], &right)) {
            return;
        }
        sqlite3_result_int(context, left == right);
        return;
    }
    if (left_type == SQLITE_BLOB || right_type == SQLITE_BLOB) {
        int result = 0;

        if (!compare_values_as_mysql_binary(arguments[0], arguments[1], &equal)) {
            sqlite3_result_error_nomem(context);
            return;
        }
        if (equal) {
            result = 1;
        }
        sqlite3_result_int(context, result);
        return;
    }
    if (!compare_values_as_mysql_text(arguments[0], arguments[1], &equal)) {
        sqlite3_result_error_nomem(context);
        return;
    }
    {
        int result = 0;

        if (equal) {
            result = 1;
        }
        sqlite3_result_int(context, result);
    }
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

static void mysql_coerce_signed_integer(
    sqlite3_context *context,
    int argument_count,
    sqlite3_value **arguments
) {
    sqlite3_int64 value = 0;
    sqlite3_int64 minimum = 0;
    sqlite3_int64 maximum = 0;

    (void)argument_count;
    if (sqlite3_value_type(arguments[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    minimum = sqlite3_value_int64(arguments[1]);
    maximum = sqlite3_value_int64(arguments[2]);
    if (minimum > maximum) {
        result_coercion_error(context, "invalid integer range");
        return;
    }
    if (!coerce_value_to_rounded_int64(
            arguments[0],
            (struct mylite_sqlite_int64_range){.minimum = minimum, .maximum = maximum},
            &value
        )) {
        result_coercion_error(context, "integer value is out of range");
        return;
    }
    sqlite3_result_int64(context, value);
}

static void mysql_coerce_unsigned_integer(
    sqlite3_context *context,
    int argument_count,
    sqlite3_value **arguments
) {
    sqlite3_int64 value = 0;
    sqlite3_int64 maximum = 0;

    (void)argument_count;
    if (sqlite3_value_type(arguments[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    maximum = sqlite3_value_int64(arguments[1]);
    if (maximum < 0) {
        result_coercion_error(context, "invalid unsigned integer range");
        return;
    }
    if (!coerce_value_to_rounded_int64(
            arguments[0],
            (struct mylite_sqlite_int64_range){.minimum = 0, .maximum = maximum},
            &value
        )) {
        result_coercion_error(context, "unsigned integer value is out of range");
        return;
    }
    sqlite3_result_int64(context, value);
}

static void mysql_coerce_double(
    sqlite3_context *context,
    int argument_count,
    sqlite3_value **arguments
) {
    double value = 0.0;

    (void)argument_count;
    if (sqlite3_value_type(arguments[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    if (!coerce_value_to_double(arguments[0], &value)) {
        result_coercion_error(context, "invalid double value");
        return;
    }
    sqlite3_result_double(context, value);
}

static void mysql_coerce_varchar(
    sqlite3_context *context,
    int argument_count,
    sqlite3_value **arguments
) {
    const unsigned char *text = NULL;
    sqlite3_int64 maximum_length = 0;
    int bytes = 0;

    (void)argument_count;
    if (sqlite3_value_type(arguments[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    maximum_length = sqlite3_value_int64(arguments[1]);
    if (maximum_length < 0) {
        result_coercion_error(context, "invalid varchar length");
        return;
    }

    text = sqlite3_value_text(arguments[0]);
    bytes = sqlite3_value_bytes(arguments[0]);
    if (text == NULL && bytes > 0) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (count_utf8_characters(text, bytes) > maximum_length) {
        result_coercion_error(context, "varchar value is too long");
        return;
    }
    sqlite3_result_text(context, (const char *)text, bytes, sqlite_transient_destructor());
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

static bool coerce_value_to_rounded_int64(
    sqlite3_value *value,
    struct mylite_sqlite_int64_range range,
    sqlite3_int64 *out_value
) {
    if (sqlite3_value_type(value) == SQLITE_INTEGER) {
        sqlite3_int64 integer = sqlite3_value_int64(value);

        if (integer < range.minimum || integer > range.maximum) {
            return false;
        }
        *out_value = integer;
        return true;
    }

    {
        double number = 0.0;

        if (!coerce_value_to_double(value, &number)) {
            return false;
        }
        return double_to_rounded_int64(number, range, out_value);
    }
}

static bool double_to_rounded_int64(
    double value,
    struct mylite_sqlite_int64_range range,
    sqlite3_int64 *out_value
) {
    double lower = (double)range.minimum - mylite_sqlite_integer_round_half;
    double upper = (double)range.maximum + mylite_sqlite_integer_round_half;
    double adjusted = 0.0;
    sqlite3_int64 integer = 0;

    if (!isfinite(value) || value <= lower || value >= upper) {
        return false;
    }

    adjusted = value < 0.0 ? value - mylite_sqlite_integer_round_half
                           : value + mylite_sqlite_integer_round_half;
    if (adjusted < (double)INT64_MIN || adjusted > (double)INT64_MAX) {
        return false;
    }

    integer = (sqlite3_int64)adjusted;
    if (integer < range.minimum || integer > range.maximum) {
        return false;
    }
    *out_value = integer;
    return true;
}

static bool coerce_value_to_double(sqlite3_value *value, double *out_value) {
    int type = sqlite3_value_type(value);
    double number = 0.0;

    if (type == SQLITE_INTEGER || type == SQLITE_FLOAT) {
        number = sqlite3_value_double(value);
        if (!isfinite(number)) {
            return false;
        }
        *out_value = number;
        return true;
    }
    return parse_complete_double(value, out_value);
}

static bool coerce_value_to_mysql_bool(sqlite3_context *context, sqlite3_value *value) {
    int type = sqlite3_value_type(value);
    const unsigned char *text = NULL;
    int bytes = 0;
    const char *start = NULL;
    const char *end = NULL;
    char *cursor = NULL;
    double number = 0.0;
    bool truncated = false;

    if (type == SQLITE_NULL) {
        return false;
    }
    if (type == SQLITE_INTEGER || type == SQLITE_FLOAT) {
        return sqlite3_value_double(value) != 0.0;
    }

    text = sqlite3_value_text(value);
    bytes = sqlite3_value_bytes(value);
    if (text == NULL) {
        return false;
    }

    start = (const char *)text;
    end = start + bytes;
    while (start < end && isspace((unsigned char)*start)) {
        ++start;
    }

    errno = 0;
    number = strtod(start, &cursor);
    truncated = cursor == start;
    if (errno == ERANGE || text_has_non_space_tail(cursor, end)) {
        truncated = true;
    }
    if (truncated) {
        publish_truncated_wrong_value_warning(context);
    }
    return number != 0.0;
}

static bool coerce_value_to_mysql_comparison_double(
    sqlite3_context *context,
    sqlite3_value *value,
    double *out_value
) {
    int type = sqlite3_value_type(value);
    const unsigned char *text = NULL;
    int bytes = 0;
    const char *start = NULL;
    const char *end = NULL;
    char *cursor = NULL;
    double number = 0.0;
    bool truncated = false;

    if (type == SQLITE_INTEGER || type == SQLITE_FLOAT) {
        *out_value = sqlite3_value_double(value);
        return true;
    }

    text = sqlite3_value_text(value);
    bytes = sqlite3_value_bytes(value);
    if (text == NULL && bytes > 0) {
        sqlite3_result_error_nomem(context);
        return false;
    }

    start = text == NULL ? "" : (const char *)text;
    end = start + bytes;
    while (start < end && isspace((unsigned char)*start)) {
        ++start;
    }

    errno = 0;
    number = strtod(start, &cursor);
    truncated = cursor == start;
    if (errno == ERANGE || text_has_non_space_tail(cursor, end)) {
        truncated = true;
    }
    if (truncated) {
        publish_truncated_wrong_value_warning(context);
    }
    *out_value = number;
    return true;
}

static bool value_is_sql_numeric(sqlite3_value *value) {
    int type = sqlite3_value_type(value);

    if (type == SQLITE_INTEGER) {
        return true;
    }
    if (type == SQLITE_FLOAT) {
        return true;
    }
    return false;
}

static bool compare_values_as_mysql_text(
    sqlite3_value *left,
    sqlite3_value *right,
    bool *out_equal
) {
    static const unsigned char empty[] = "";
    const unsigned char *left_text = sqlite3_value_text(left);
    int left_length = sqlite3_value_bytes(left);
    const unsigned char *right_text = sqlite3_value_text(right);
    int right_length = sqlite3_value_bytes(right);

    if ((left_text == NULL && left_length > 0) || (right_text == NULL && right_length > 0)) {
        return false;
    }
    if (left_text == NULL) {
        left_text = empty;
    }
    if (right_text == NULL) {
        right_text = empty;
    }
    left_length = trim_pad_space((struct mylite_sqlite_pad_trim_request){
        .value = left_text,
        .length = left_length,
        .flags = mylite_sqlite_collation_pad_space,
    });
    right_length = trim_pad_space((struct mylite_sqlite_pad_trim_request){
        .value = right_text,
        .length = right_length,
        .flags = mylite_sqlite_collation_pad_space,
    });
    *out_equal = compare_ascii_ci_bytes(left_text, left_length, right_text, right_length) == 0;
    return true;
}

static bool compare_values_as_mysql_binary(
    sqlite3_value *left,
    sqlite3_value *right,
    bool *out_equal
) {
    static const unsigned char empty[] = "";
    const unsigned char *left_value = sqlite3_value_blob(left);
    int left_length = sqlite3_value_bytes(left);
    const unsigned char *right_value = sqlite3_value_blob(right);
    int right_length = sqlite3_value_bytes(right);

    if ((left_value == NULL && left_length > 0) || (right_value == NULL && right_length > 0)) {
        return false;
    }
    if (left_value == NULL) {
        left_value = empty;
    }
    if (right_value == NULL) {
        right_value = empty;
    }
    *out_equal = compare_binary_bytes(left_value, left_length, right_value, right_length) == 0;
    return true;
}

static void publish_truncated_wrong_value_warning(sqlite3_context *context) {
    static const char truncated_sqlstate[] = "22007";

    (void)mylite_sqlite_fork_set_condition(
        sqlite3_context_db_handle(context),
        MYLITE_SQLITE_FORK_CONDITION_WARNING,
        mylite_sqlite_mysql_truncated_wrong_value,
        truncated_sqlstate
    );
}

static bool parse_complete_double(sqlite3_value *value, double *out_value) {
    const unsigned char *text = sqlite3_value_text(value);
    int bytes = sqlite3_value_bytes(value);
    const char *start = (const char *)text;
    const char *cursor = start;
    char *end = NULL;
    double number = 0.0;

    if (text == NULL) {
        return false;
    }

    while (cursor < start + bytes && isspace((unsigned char)*cursor)) {
        ++cursor;
    }
    if (cursor == start + bytes) {
        return false;
    }

    number = strtod(cursor, &end);
    if (end == cursor || !isfinite(number)) {
        return false;
    }

    while (end < start + bytes && isspace((unsigned char)*end)) {
        ++end;
    }
    if (end != start + bytes) {
        return false;
    }

    *out_value = number;
    return true;
}

static bool text_has_non_space_tail(const char *cursor, const char *end) {
    while (cursor < end) {
        if (!isspace((unsigned char)*cursor)) {
            return true;
        }
        ++cursor;
    }
    return false;
}

static void result_coercion_error(sqlite3_context *context, const char *message) {
    sqlite3_result_error(context, message, -1);
}

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
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

static bool collation_is_pad_space(const struct mylite_fork_collation *collation) {
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
