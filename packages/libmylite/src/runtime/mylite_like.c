#include "mylite_like.h"

#include "mylite_sqlite_registration.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>

struct like_pattern_item_request {
    const char *pattern;
    size_t pattern_length;
    size_t pattern_index;
    unsigned char value_byte;
    bool case_sensitive;
    bool backslash_escapes;
};

static void like_binary_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static bool like_pattern_matches(
    const char *pattern,
    size_t pattern_length,
    const char *value,
    size_t value_length,
    bool case_sensitive,
    bool backslash_escapes
);
static size_t like_skip_percent_run(
    const char *pattern,
    size_t pattern_length,
    size_t pattern_index
);
static bool like_pattern_item_matches(
    struct like_pattern_item_request request,
    size_t *out_next_pattern_index
);
static bool like_bytes_equal(unsigned char left, unsigned char right, bool case_sensitive);
static unsigned char like_ascii_lower(unsigned char byte);

int mylite_sqlite_register_like_functions(sqlite3 *sqlite) {
    static const struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_like_binary",
            .argument_count = 3,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = like_binary_sqlite_callback,
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

static void like_binary_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const unsigned char *value = NULL;
    const unsigned char *pattern = NULL;
    int value_length = 0;
    int pattern_length = 0;
    bool backslash_escapes = true;
    bool matches = false;

    if (argc != 3) {
        sqlite3_result_error(context, "invalid _mylite_like_binary call", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL ||
        sqlite3_value_type(argv[1]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    value = sqlite3_value_text(argv[0]);
    value_length = sqlite3_value_bytes(argv[0]);
    pattern = sqlite3_value_text(argv[1]);
    pattern_length = sqlite3_value_bytes(argv[1]);
    if ((value == NULL && value_length != 0) || (pattern == NULL && pattern_length != 0)) {
        sqlite3_result_error_nomem(context);
        return;
    }

    backslash_escapes = sqlite3_value_int(argv[2]) != 0;
    matches = like_pattern_matches(
        pattern == NULL ? "" : (const char *)pattern,
        (size_t)pattern_length,
        value == NULL ? "" : (const char *)value,
        (size_t)value_length,
        true,
        backslash_escapes
    );
    sqlite3_result_int(context, matches ? 1 : 0);
}

static bool like_pattern_matches(
    const char *pattern,
    size_t pattern_length,
    const char *value,
    size_t value_length,
    bool case_sensitive,
    bool backslash_escapes
) {
    const size_t no_retry_pattern = (size_t)-1;
    size_t pattern_index = 0U;
    size_t value_index = 0U;
    size_t retry_pattern_index = no_retry_pattern;
    size_t retry_value_index = 0U;

    while (value_index < value_length) {
        size_t next_pattern_index = pattern_index;

        if (pattern_index < pattern_length && pattern[pattern_index] == '%') {
            pattern_index = like_skip_percent_run(pattern, pattern_length, pattern_index);
            if (pattern_index == pattern_length) {
                return true;
            }
            retry_pattern_index = pattern_index;
            retry_value_index = value_index;
            continue;
        }
        if (like_pattern_item_matches(
                (struct like_pattern_item_request){
                    .pattern = pattern,
                    .pattern_length = pattern_length,
                    .pattern_index = pattern_index,
                    .value_byte = (unsigned char)value[value_index],
                    .case_sensitive = case_sensitive,
                    .backslash_escapes = backslash_escapes,
                },
                &next_pattern_index
            )) {
            pattern_index = next_pattern_index;
            ++value_index;
            continue;
        }
        if (retry_pattern_index == no_retry_pattern || retry_value_index >= value_length) {
            return false;
        }
        ++retry_value_index;
        value_index = retry_value_index;
        pattern_index = retry_pattern_index;
    }

    pattern_index = like_skip_percent_run(pattern, pattern_length, pattern_index);

    return pattern_index == pattern_length;
}

static size_t like_skip_percent_run(
    const char *pattern,
    size_t pattern_length,
    size_t pattern_index
) {
    while (pattern_index < pattern_length && pattern[pattern_index] == '%') {
        ++pattern_index;
    }
    return pattern_index;
}

static bool like_pattern_item_matches(
    struct like_pattern_item_request request,
    size_t *out_next_pattern_index
) {
    unsigned char pattern_byte = '\0';
    size_t next_pattern_index = request.pattern_index;

    if (request.pattern_index >= request.pattern_length || out_next_pattern_index == NULL) {
        return false;
    }

    pattern_byte = (unsigned char)request.pattern[request.pattern_index];
    if (pattern_byte == '_') {
        *out_next_pattern_index = request.pattern_index + 1U;
        return true;
    }
    if (request.backslash_escapes && pattern_byte == '\\' &&
        request.pattern_index + 1U < request.pattern_length) {
        ++next_pattern_index;
        pattern_byte = (unsigned char)request.pattern[next_pattern_index];
    }
    ++next_pattern_index;
    if (!like_bytes_equal(pattern_byte, request.value_byte, request.case_sensitive)) {
        return false;
    }

    *out_next_pattern_index = next_pattern_index;
    return true;
}

static bool like_bytes_equal(unsigned char left, unsigned char right, bool case_sensitive) {
    if (case_sensitive) {
        return left == right;
    }

    return like_ascii_lower(left) == like_ascii_lower(right);
}

static unsigned char like_ascii_lower(unsigned char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (unsigned char)(byte - 'A' + 'a');
    }
    return byte;
}
