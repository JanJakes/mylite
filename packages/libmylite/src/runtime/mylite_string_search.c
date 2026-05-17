#include "mylite_string_search.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    mysql_error_parse = 1064,
    ascii_upper_a = 'A',
    ascii_upper_z = 'Z',
    ascii_lower_a = 'a',
    ascii_lower_z = 'z',
    ascii_case_delta = 'a' - 'A',
    ascii_max = 0x7f,
};

static void string_search_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void find_in_set_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int locate_ascii_ci_position(
    struct mylite_db *database,
    const char *needle,
    size_t needle_length,
    const char *haystack,
    size_t haystack_length,
    int64_t position,
    int64_t *out_position
);
static int find_in_set_ascii_ci_position(
    const char *search,
    size_t search_length,
    const char *list,
    size_t list_length,
    int64_t *out_position
);
static bool ascii_text_contains_comma(const char *text, size_t text_length);
static bool ascii_text_segment_equals_ci(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length
);
static bool ascii_text_is_supported(const char *text, size_t text_length);
static bool ascii_bytes_equal_ci(unsigned char left, unsigned char right);
static unsigned char ascii_fold(unsigned char byte);
static void set_string_search_unsupported_error(struct mylite_db *database);

int mylite_string_search_locate_ascii_ci_value(
    struct mylite_db *database,
    const char *needle,
    size_t needle_length,
    const char *haystack,
    size_t haystack_length,
    int64_t position,
    int64_t *out_position
) {
    if (needle == NULL || haystack == NULL || out_position == NULL) {
        return MYLITE_MISUSE;
    }
    *out_position = 0;
    if (!ascii_text_is_supported(needle, needle_length) ||
        !ascii_text_is_supported(haystack, haystack_length) ||
        haystack_length > (size_t)INT64_MAX - 1U) {
        set_string_search_unsupported_error(database);
        return MYLITE_ERROR;
    }

    return locate_ascii_ci_position(
        database,
        needle,
        needle_length,
        haystack,
        haystack_length,
        position,
        out_position
    );
}

int mylite_string_search_find_in_set_ascii_ci_value(
    struct mylite_db *database,
    const char *search,
    size_t search_length,
    const char *list,
    size_t list_length,
    int64_t *out_position
) {
    if (search == NULL || list == NULL || out_position == NULL) {
        return MYLITE_MISUSE;
    }
    *out_position = 0;
    if (!ascii_text_is_supported(search, search_length) ||
        !ascii_text_is_supported(list, list_length)) {
        set_string_search_unsupported_error(database);
        return MYLITE_ERROR;
    }

    return find_in_set_ascii_ci_position(search, search_length, list, list_length, out_position);
}

int mylite_sqlite_register_string_search_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_locate_ascii_ci",
            .argument_count = 3,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = string_search_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_find_in_set_ascii_ci",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = find_in_set_sqlite_callback,
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

static void string_search_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    struct mylite_db *database = NULL;
    const unsigned char *needle = NULL;
    const unsigned char *haystack = NULL;
    int needle_length = 0;
    int haystack_length = 0;
    int64_t result = 0;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 3 || argv == NULL || argv[0] == NULL || argv[1] == NULL ||
        argv[2] == NULL) {
        sqlite3_result_error(context, "invalid MyLite string search callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL ||
        sqlite3_value_type(argv[2]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    database = mylite_sqlite_bootstrap_owner_from_context(context);
    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite string search owner", -1);
        return;
    }

    needle = sqlite3_value_text(argv[0]);
    haystack = sqlite3_value_text(argv[1]);
    needle_length = sqlite3_value_bytes(argv[0]);
    haystack_length = sqlite3_value_bytes(argv[1]);
    if (needle == NULL || haystack == NULL || needle_length < 0 || haystack_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }

    rc = mylite_string_search_locate_ascii_ci_value(
        database,
        (const char *)needle,
        (size_t)needle_length,
        (const char *)haystack,
        (size_t)haystack_length,
        sqlite3_value_int64(argv[2]),
        &result
    );
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "MyLite string search failed", -1);
        return;
    }

    sqlite3_result_int64(context, (sqlite3_int64)result);
}

static void find_in_set_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_db *database = NULL;
    const unsigned char *search = NULL;
    const unsigned char *list = NULL;
    int search_length = 0;
    int list_length = 0;
    int64_t result = 0;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        sqlite3_result_error(context, "invalid MyLite FIND_IN_SET callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    database = mylite_sqlite_bootstrap_owner_from_context(context);
    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite FIND_IN_SET owner", -1);
        return;
    }

    search = sqlite3_value_text(argv[0]);
    list = sqlite3_value_text(argv[1]);
    search_length = sqlite3_value_bytes(argv[0]);
    list_length = sqlite3_value_bytes(argv[1]);
    if (search == NULL || list == NULL || search_length < 0 || list_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }

    rc = mylite_string_search_find_in_set_ascii_ci_value(
        database,
        (const char *)search,
        (size_t)search_length,
        (const char *)list,
        (size_t)list_length,
        &result
    );
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "MyLite FIND_IN_SET failed", -1);
        return;
    }

    sqlite3_result_int64(context, (sqlite3_int64)result);
}

static int locate_ascii_ci_position(
    struct mylite_db *database,
    const char *needle,
    size_t needle_length,
    const char *haystack,
    size_t haystack_length,
    int64_t position,
    int64_t *out_position
) {
    size_t start_index = 0U;
    size_t last_start = 0U;

    (void)database;
    if (needle == NULL || haystack == NULL || out_position == NULL) {
        return MYLITE_MISUSE;
    }
    *out_position = 0;
    if (position <= 0) {
        return MYLITE_OK;
    }
    if (needle_length == 0U) {
        if (position <= (int64_t)haystack_length + 1) {
            *out_position = position;
        }
        return MYLITE_OK;
    }
    if ((uint64_t)position > (uint64_t)haystack_length || needle_length > haystack_length) {
        return MYLITE_OK;
    }

    start_index = (size_t)position - 1U;
    last_start = haystack_length - needle_length;
    for (size_t index = start_index; index <= last_start; ++index) {
        bool matches = true;

        for (size_t needle_index = 0U; needle_index < needle_length; ++needle_index) {
            if (!ascii_bytes_equal_ci(
                    (unsigned char)needle[needle_index],
                    (unsigned char)haystack[index + needle_index]
                )) {
                matches = false;
                break;
            }
        }
        if (matches) {
            *out_position = (int64_t)index + 1;
            return MYLITE_OK;
        }
    }

    return MYLITE_OK;
}

static int find_in_set_ascii_ci_position(
    const char *search,
    size_t search_length,
    const char *list,
    size_t list_length,
    int64_t *out_position
) {
    size_t member_start = 0U;
    int64_t position = 1;

    if (search == NULL || list == NULL || out_position == NULL) {
        return MYLITE_MISUSE;
    }
    *out_position = 0;
    if (list_length == 0U || ascii_text_contains_comma(search, search_length)) {
        return MYLITE_OK;
    }

    for (size_t index = 0U; index <= list_length; ++index) {
        if (index == list_length || list[index] == ',') {
            if (ascii_text_segment_equals_ci(
                    search,
                    search_length,
                    list + member_start,
                    index - member_start
                )) {
                *out_position = position;
                return MYLITE_OK;
            }
            member_start = index + 1U;
            ++position;
        }
    }

    return MYLITE_OK;
}

static bool ascii_text_contains_comma(const char *text, size_t text_length) {
    if (text == NULL) {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        if (text[index] == ',') {
            return true;
        }
    }
    return false;
}

static bool ascii_text_segment_equals_ci(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length
) {
    if (left == NULL || right == NULL || left_length != right_length) {
        return false;
    }
    for (size_t index = 0U; index < left_length; ++index) {
        if (!ascii_bytes_equal_ci((unsigned char)left[index], (unsigned char)right[index])) {
            return false;
        }
    }
    return true;
}

static bool ascii_text_is_supported(const char *text, size_t text_length) {
    if (text == NULL) {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (byte == '\0' || byte > ascii_max) {
            return false;
        }
    }
    return true;
}

static bool ascii_bytes_equal_ci(unsigned char left, unsigned char right) {
    return ascii_fold(left) == ascii_fold(right);
}

static unsigned char ascii_fold(unsigned char byte) {
    if (byte >= ascii_upper_a && byte <= ascii_upper_z) {
        return (unsigned char)(byte + ascii_case_delta);
    }
    if (byte >= ascii_lower_a && byte <= ascii_lower_z) {
        return byte;
    }
    return byte;
}

static void set_string_search_unsupported_error(struct mylite_db *database) {
    if (database == NULL) {
        return;
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        "string search functions support only ASCII text values"
    );
}
