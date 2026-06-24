#include "mylite_sys_functions.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_dynamic_string.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_mysql_server_identity.h"
#include "mylite_source_span.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    statement_truncate_len_default = 64,
    sys_function_number_buffer_size = 96,
    sys_config_decimal_parse_base = 10,
};

static const long double sys_format_bytes_unit_size = 1024.0L;
static const long double sys_format_time_unit_size = 1000.0L;

struct sys_function_descriptor {
    const char *name;
    const char *sqlite_name;
    enum mylite_sys_function_kind kind;
    size_t argument_count;
};

struct sys_config_value {
    const char *name;
    const char *value;
};

struct path_prefix_rewrite {
    const char *prefix;
    const char *replacement;
};

static const struct sys_function_descriptor sys_function_descriptors[] = {
    {"extract_schema_from_file_name",
     "_mylite_sys_extract_schema_from_file_name",
     MYLITE_SYS_FUNCTION_EXTRACT_SCHEMA_FROM_FILE_NAME,
     1U},
    {"extract_table_from_file_name",
     "_mylite_sys_extract_table_from_file_name",
     MYLITE_SYS_FUNCTION_EXTRACT_TABLE_FROM_FILE_NAME,
     1U},
    {"format_bytes", "_mylite_sys_format_bytes", MYLITE_SYS_FUNCTION_FORMAT_BYTES, 1U},
    {"format_path", "_mylite_sys_format_path", MYLITE_SYS_FUNCTION_FORMAT_PATH, 1U},
    {"format_statement", "_mylite_sys_format_statement", MYLITE_SYS_FUNCTION_FORMAT_STATEMENT, 1U},
    {"format_time", "_mylite_sys_format_time", MYLITE_SYS_FUNCTION_FORMAT_TIME, 1U},
    {"list_add", "_mylite_sys_list_add", MYLITE_SYS_FUNCTION_LIST_ADD, 2U},
    {"list_drop", "_mylite_sys_list_drop", MYLITE_SYS_FUNCTION_LIST_DROP, 2U},
    {"quote_identifier", "_mylite_sys_quote_identifier", MYLITE_SYS_FUNCTION_QUOTE_IDENTIFIER, 1U},
    {"sys_get_config", "_mylite_sys_sys_get_config", MYLITE_SYS_FUNCTION_SYS_GET_CONFIG, 2U},
    {"version_major", "_mylite_sys_version_major", MYLITE_SYS_FUNCTION_VERSION_MAJOR, 0U},
    {"version_minor", "_mylite_sys_version_minor", MYLITE_SYS_FUNCTION_VERSION_MINOR, 0U},
    {"version_patch", "_mylite_sys_version_patch", MYLITE_SYS_FUNCTION_VERSION_PATCH, 0U},
};

enum {
    sys_function_descriptor_count =
        sizeof(sys_function_descriptors) / sizeof(sys_function_descriptors[0])
};

static const struct sys_config_value sys_config_values[] = {
    {"diagnostics.allow_i_s_tables", "OFF"},
    {"diagnostics.include_raw", "OFF"},
    {"ps_thread_trx_info.max_length", "65535"},
    {"statement_performance_analyzer.limit", "100"},
    {"statement_performance_analyzer.view", NULL},
    {"statement_truncate_len", "64"},
};

static const struct path_prefix_rewrite path_prefix_rewrites[] = {
    {"/var/lib/mysql/", "@@datadir"},
    {"/tmp", "@@tmpdir"},
    {"/usr/", "@@basedir"},
};

static void sys_function_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static const struct sys_function_descriptor *sys_function_descriptor_by_kind(
    enum mylite_sys_function_kind kind
);
static const struct sys_function_descriptor *sys_function_descriptor_by_name(
    const char *name,
    size_t name_size
);
static bool ascii_equals_case_insensitive(const char *left, size_t left_size, const char *right);
static int sys_function_null_result(struct mylite_sys_function_result *out_result);
static int sys_function_copy_result(
    struct mylite_sys_function_result *out_result,
    const char *text,
    size_t text_size
);
static int sys_function_format_result(
    struct mylite_sys_function_result *out_result,
    const char *format,
    ...
);
static int sys_extract_schema_from_file_name(
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_extract_table_from_file_name(
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_format_bytes(
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_format_path(
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_format_statement(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_format_time(
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_list_add(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_list_drop(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_quote_identifier(
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_sys_get_config(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_version_component(
    enum mylite_sys_function_kind kind,
    struct mylite_sys_function_result *out_result
);
static size_t last_slash_before(const char *text, size_t end);
static size_t last_dot_between(const char *text, size_t start, size_t end);
static int copy_argument_text(const struct mylite_sys_function_argument *argument, char **out_text);
static bool parse_argument_number(
    const struct mylite_sys_function_argument *argument,
    long double *out_value,
    bool *out_valid
);
static int format_fixed_unit_result(
    struct mylite_sys_function_result *out_result,
    long double value,
    const char *unit
);
static int format_trimmed_unit_result(
    struct mylite_sys_function_result *out_result,
    long double value,
    const char *unit
);
static void trim_formatted_fraction(char *buffer);
static size_t sys_statement_truncate_length(struct mylite_db *database);
static const char *sys_config_value_for(
    struct mylite_db *database,
    const char *name,
    size_t name_size,
    bool *out_is_null
);
static const char *sys_config_static_value(const char *name, size_t name_size, bool *out_is_null);
static const char *sys_config_user_variable_value(
    struct mylite_db *database,
    const char *name,
    size_t name_size,
    size_t *out_value_size,
    bool *out_is_null
);
static void remove_list_drop_pattern(
    struct mylite_dynamic_string *string,
    const char *value,
    size_t value_size,
    bool include_space
);
static void trim_list_drop_commas(struct mylite_dynamic_string *string);
static void set_sys_list_null_error(struct mylite_db *database, enum mylite_sys_function_kind kind);
static int sys_function_sqlite_argument(
    sqlite3_value *value,
    struct mylite_sys_function_argument *out_argument
);
static void sys_function_sqlite_result(
    sqlite3_context *context,
    struct mylite_sys_function_result *result
);
static void sys_function_sqlite_error(sqlite3_context *context, struct mylite_db *database, int rc);

bool mylite_sys_function_lookup(
    const char *schema,
    size_t schema_size,
    const char *name,
    size_t name_size,
    enum mylite_sys_function_kind *out_kind
) {
    const struct sys_function_descriptor *descriptor = NULL;

    if (out_kind == NULL || name == NULL) {
        return false;
    }
    *out_kind = MYLITE_SYS_FUNCTION_NONE;
    if (schema != NULL && !ascii_equals_case_insensitive(schema, schema_size, "sys")) {
        return false;
    }

    descriptor = sys_function_descriptor_by_name(name, name_size);
    if (descriptor == NULL) {
        return false;
    }
    *out_kind = descriptor->kind;
    return true;
}

bool mylite_sys_function_lookup_span(
    const struct mylite_sql_source_span *schema,
    const struct mylite_sql_source_span *name,
    enum mylite_sys_function_kind *out_kind
) {
    const char *schema_text = schema == NULL ? NULL : schema->text;
    size_t schema_size = schema == NULL ? 0U : schema->length;

    if (name == NULL) {
        if (out_kind != NULL) {
            *out_kind = MYLITE_SYS_FUNCTION_NONE;
        }
        return false;
    }
    return mylite_sys_function_lookup(schema_text, schema_size, name->text, name->length, out_kind);
}

const char *mylite_sys_function_name(enum mylite_sys_function_kind kind) {
    const struct sys_function_descriptor *descriptor = sys_function_descriptor_by_kind(kind);

    return descriptor == NULL ? NULL : descriptor->name;
}

const char *mylite_sys_function_sqlite_name(enum mylite_sys_function_kind kind) {
    const struct sys_function_descriptor *descriptor = sys_function_descriptor_by_kind(kind);

    return descriptor == NULL ? NULL : descriptor->sqlite_name;
}

size_t mylite_sys_function_argument_count(enum mylite_sys_function_kind kind) {
    const struct sys_function_descriptor *descriptor = sys_function_descriptor_by_kind(kind);

    return descriptor == NULL ? 0U : descriptor->argument_count;
}

int mylite_sys_function_evaluate(
    struct mylite_db *database,
    enum mylite_sys_function_kind kind,
    const struct mylite_sys_function_argument *arguments,
    size_t argument_count,
    struct mylite_sys_function_result *out_result
) {
    if (out_result == NULL || (argument_count != 0U && arguments == NULL)) {
        return MYLITE_MISUSE;
    }
    *out_result = (struct mylite_sys_function_result){0};
    if (argument_count != mylite_sys_function_argument_count(kind)) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; index < argument_count; ++index) {
        if (!arguments[index].is_null && arguments[index].text == NULL) {
            return MYLITE_MISUSE;
        }
    }

    switch (kind) {
    case MYLITE_SYS_FUNCTION_EXTRACT_SCHEMA_FROM_FILE_NAME:
        return sys_extract_schema_from_file_name(arguments, out_result);
    case MYLITE_SYS_FUNCTION_EXTRACT_TABLE_FROM_FILE_NAME:
        return sys_extract_table_from_file_name(arguments, out_result);
    case MYLITE_SYS_FUNCTION_FORMAT_BYTES:
        return sys_format_bytes(arguments, out_result);
    case MYLITE_SYS_FUNCTION_FORMAT_PATH:
        return sys_format_path(arguments, out_result);
    case MYLITE_SYS_FUNCTION_FORMAT_STATEMENT:
        return sys_format_statement(database, arguments, out_result);
    case MYLITE_SYS_FUNCTION_FORMAT_TIME:
        return sys_format_time(arguments, out_result);
    case MYLITE_SYS_FUNCTION_LIST_ADD:
        return sys_list_add(database, arguments, out_result);
    case MYLITE_SYS_FUNCTION_LIST_DROP:
        return sys_list_drop(database, arguments, out_result);
    case MYLITE_SYS_FUNCTION_QUOTE_IDENTIFIER:
        return sys_quote_identifier(arguments, out_result);
    case MYLITE_SYS_FUNCTION_SYS_GET_CONFIG:
        return sys_sys_get_config(database, arguments, out_result);
    case MYLITE_SYS_FUNCTION_VERSION_MAJOR:
    case MYLITE_SYS_FUNCTION_VERSION_MINOR:
    case MYLITE_SYS_FUNCTION_VERSION_PATCH:
        return sys_version_component(kind, out_result);
    case MYLITE_SYS_FUNCTION_NONE:
        break;
    }
    return MYLITE_MISUSE;
}

void mylite_sys_function_result_deinit(struct mylite_sys_function_result *result) {
    if (result == NULL) {
        return;
    }
    free(result->text);
    *result = (struct mylite_sys_function_result){0};
}

int mylite_sqlite_register_sys_functions(sqlite3 *sqlite) {
    enum { flags = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC };
    struct mylite_sqlite_function_registration registrations[sys_function_descriptor_count] = {{0}};

    for (size_t index = 0U; index < sys_function_descriptor_count; ++index) {
        const struct sys_function_descriptor *descriptor = &sys_function_descriptors[index];

        registrations[index] = (struct mylite_sqlite_function_registration){
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = descriptor->sqlite_name,
            .argument_count = (int)descriptor->argument_count,
            .text_representation = flags,
            .application_data = (void *)descriptor,
            .scalar_callback = sys_function_sqlite_callback,
        };
    }

    return mylite_sqlite_register_functions(sqlite, registrations, sys_function_descriptor_count);
}

static void sys_function_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_sys_function_argument arguments[2] = {{0}};
    struct mylite_sys_function_result result = {0};
    struct mylite_db *database = NULL;
    const struct sys_function_descriptor *descriptor = NULL;
    enum mylite_sys_function_kind kind = MYLITE_SYS_FUNCTION_NONE;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    if (context == NULL || argc < 0 || (argc != 0 && argv == NULL) || argc > 2) {
        sqlite3_result_error(context, "invalid MyLite sys helper callback", -1);
        return;
    }

    descriptor = (const struct sys_function_descriptor *)sqlite3_user_data(context);
    if (descriptor == NULL) {
        sqlite3_result_error(context, "invalid MyLite sys helper callback data", -1);
        return;
    }
    kind = descriptor->kind;
    argument_count = (size_t)argc;
    for (size_t index = 0U; rc == MYLITE_OK && index < argument_count; ++index) {
        rc = sys_function_sqlite_argument(argv[index], &arguments[index]);
    }
    if (rc == MYLITE_OK) {
        database = mylite_sqlite_bootstrap_owner_from_context(context);
        rc = mylite_sys_function_evaluate(database, kind, arguments, argument_count, &result);
    }
    if (rc == MYLITE_OK) {
        sys_function_sqlite_result(context, &result);
    } else {
        sys_function_sqlite_error(context, database, rc);
        mylite_sys_function_result_deinit(&result);
    }
}

static const struct sys_function_descriptor *sys_function_descriptor_by_kind(
    enum mylite_sys_function_kind kind
) {
    for (size_t index = 0U;
         index < sizeof(sys_function_descriptors) / sizeof(sys_function_descriptors[0]);
         ++index) {
        if (sys_function_descriptors[index].kind == kind) {
            return &sys_function_descriptors[index];
        }
    }
    return NULL;
}

static const struct sys_function_descriptor *sys_function_descriptor_by_name(
    const char *name,
    size_t name_size
) {
    for (size_t index = 0U;
         index < sizeof(sys_function_descriptors) / sizeof(sys_function_descriptors[0]);
         ++index) {
        if (ascii_equals_case_insensitive(name, name_size, sys_function_descriptors[index].name)) {
            return &sys_function_descriptors[index];
        }
    }
    return NULL;
}

static bool ascii_equals_case_insensitive(const char *left, size_t left_size, const char *right) {
    size_t right_size = right == NULL ? 0U : strlen(right);

    if (left == NULL || right == NULL || left_size != right_size) {
        return false;
    }
    for (size_t index = 0U; index < left_size; ++index) {
        if (tolower((unsigned char)left[index]) != tolower((unsigned char)right[index])) {
            return false;
        }
    }
    return true;
}

static int sys_function_null_result(struct mylite_sys_function_result *out_result) {
    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = (struct mylite_sys_function_result){.is_null = true};
    return MYLITE_OK;
}

static int sys_function_copy_result(
    struct mylite_sys_function_result *out_result,
    const char *text,
    size_t text_size
) {
    char *copy = NULL;

    if (out_result == NULL || text == NULL) {
        return MYLITE_MISUSE;
    }
    copy = (char *)malloc(text_size + 1U);
    if (copy == NULL) {
        return MYLITE_NOMEM;
    }
    memcpy(copy, text, text_size);
    copy[text_size] = '\0';
    out_result->text = copy;
    out_result->text_size = text_size;
    out_result->is_null = false;
    return MYLITE_OK;
}

static int sys_function_format_result(
    struct mylite_sys_function_result *out_result,
    const char *format,
    ...
) {
    char buffer[sys_function_number_buffer_size];
    va_list args;
    int written = 0;

    va_start(args, format);
    written = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return MYLITE_NOMEM;
    }
    return sys_function_copy_result(out_result, buffer, (size_t)written);
}

static int sys_extract_schema_from_file_name(
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    const char *path = arguments[0].text;
    size_t end = arguments[0].text_size;
    size_t last_slash = 0U;
    size_t previous_slash = 0U;
    size_t start = 0U;

    if (arguments[0].is_null) {
        return sys_function_null_result(out_result);
    }
    while (end > 0U && path[end - 1U] == '/') {
        --end;
    }
    if (end == 0U) {
        return sys_function_copy_result(out_result, "", 0U);
    }
    last_slash = last_slash_before(path, end);
    if (last_slash == SIZE_MAX) {
        return sys_function_copy_result(out_result, path, end);
    }
    previous_slash = last_slash_before(path, last_slash);
    start = previous_slash == SIZE_MAX ? 0U : previous_slash + 1U;
    return sys_function_copy_result(out_result, path + start, last_slash - start);
}

static int sys_extract_table_from_file_name(
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    const char *path = arguments[0].text;
    size_t end = arguments[0].text_size;
    size_t start = 0U;
    size_t last_slash = 0U;
    size_t dot = 0U;

    if (arguments[0].is_null) {
        return sys_function_null_result(out_result);
    }
    if (end > 0U && path[end - 1U] == '/') {
        return sys_function_copy_result(out_result, "", 0U);
    }
    last_slash = last_slash_before(path, end);
    start = last_slash == SIZE_MAX ? 0U : last_slash + 1U;
    dot = last_dot_between(path, start, end);
    if (dot != SIZE_MAX) {
        end = dot;
    }
    return sys_function_copy_result(out_result, path + start, end - start);
}

static int sys_format_bytes(
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    static const char *const units[] = {"KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    long double value = 0.0L;
    long double absolute_value = 0.0L;
    bool valid = false;

    if (arguments[0].is_null) {
        return sys_function_null_result(out_result);
    }
    if (!parse_argument_number(&arguments[0], &value, &valid)) {
        return MYLITE_NOMEM;
    }
    if (!valid) {
        value = 0.0L;
    }
    absolute_value = fabsl(value);
    if (absolute_value < sys_format_bytes_unit_size) {
        return sys_function_format_result(out_result, "%4.0Lf bytes", value);
    }
    for (size_t index = 0U; index < sizeof(units) / sizeof(units[0]); ++index) {
        value /= sys_format_bytes_unit_size;
        absolute_value /= sys_format_bytes_unit_size;
        if (absolute_value < sys_format_bytes_unit_size ||
            index + 1U == sizeof(units) / sizeof(units[0])) {
            return format_fixed_unit_result(out_result, value, units[index]);
        }
    }
    return MYLITE_ERROR;
}

static int sys_format_path(
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    const char *path = NULL;
    size_t path_size = 0U;

    if (arguments[0].is_null) {
        return sys_function_null_result(out_result);
    }
    if (arguments[0].text == NULL) {
        return MYLITE_MISUSE;
    }
    path = arguments[0].text;
    path_size = arguments[0].text_size;
    for (size_t index = 0U; index < sizeof(path_prefix_rewrites) / sizeof(path_prefix_rewrites[0]);
         ++index) {
        const struct path_prefix_rewrite *rewrite = &path_prefix_rewrites[index];
        size_t prefix_size = strlen(rewrite->prefix);
        size_t replacement_size = strlen(rewrite->replacement);
        struct mylite_dynamic_string string;
        int rc = MYLITE_OK;

        if (path_size < prefix_size || memcmp(path, rewrite->prefix, prefix_size) != 0) {
            continue;
        }

        mylite_dynamic_string_init(&string);
        rc = mylite_dynamic_string_append_bytes(&string, rewrite->replacement, replacement_size);
        if (rc == MYLITE_OK && prefix_size > 0U && rewrite->prefix[prefix_size - 1U] == '/') {
            rc = mylite_dynamic_string_append_char(&string, '/');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_bytes(
                &string,
                path + prefix_size,
                path_size - prefix_size
            );
        }
        if (rc == MYLITE_OK) {
            out_result->text = mylite_dynamic_string_take(&string);
            if (out_result->text == NULL) {
                rc = MYLITE_NOMEM;
            } else {
                out_result->text_size = strlen(out_result->text);
                out_result->is_null = false;
            }
        }
        mylite_dynamic_string_deinit(&string);
        return rc;
    }
    return sys_function_copy_result(out_result, path, path_size);
}

static int sys_format_statement(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    static const char separator[] = " ... ";
    const char *statement = arguments[0].text;
    size_t statement_size = arguments[0].text_size;
    size_t truncate_length = sys_statement_truncate_length(database);
    size_t separator_size = sizeof(separator) - 1U;
    size_t prefix_size = 0U;
    size_t suffix_size = 0U;
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (arguments[0].is_null) {
        return sys_function_null_result(out_result);
    }
    if (truncate_length == 0U || statement_size <= truncate_length ||
        truncate_length <= separator_size) {
        return sys_function_copy_result(out_result, statement, statement_size);
    }

    prefix_size = (truncate_length - separator_size + 1U) / 2U;
    suffix_size = prefix_size;

    mylite_dynamic_string_init(&string);
    rc = mylite_dynamic_string_append_bytes(&string, statement, prefix_size);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_bytes(&string, separator, separator_size);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_bytes(
            &string,
            statement + statement_size - suffix_size,
            suffix_size
        );
    }
    if (rc == MYLITE_OK) {
        out_result->text = mylite_dynamic_string_take(&string);
        if (out_result->text == NULL) {
            rc = MYLITE_NOMEM;
        } else {
            out_result->text_size = strlen(out_result->text);
            out_result->is_null = false;
        }
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int sys_format_time(
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    static const struct {
        long double divisor;
        long double limit;
        const char *unit;
    } units[] = {
        {1000.0L, 1000000.0L, "ns"},
        {1000000.0L, 1000000000.0L, "us"},
        {1000000000.0L, 1000000000000.0L, "ms"},
        {1000000000000.0L, 60000000000000.0L, "s"},
        {60000000000000.0L, 3600000000000000.0L, "m"},
        {3600000000000000.0L, 86400000000000000.0L, "h"},
        {86400000000000000.0L, 0.0L, "d"},
    };

    long double value = 0.0L;
    long double absolute_value = 0.0L;
    bool valid = false;

    if (arguments[0].is_null) {
        return sys_function_null_result(out_result);
    }
    if (!parse_argument_number(&arguments[0], &value, &valid)) {
        return MYLITE_NOMEM;
    }
    if (!valid) {
        struct mylite_dynamic_string string;
        int rc = MYLITE_OK;

        mylite_dynamic_string_init(&string);
        rc = mylite_dynamic_string_append_bytes(&string, arguments[0].text, arguments[0].text_size);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, " ps");
        }
        if (rc == MYLITE_OK) {
            out_result->text = mylite_dynamic_string_take(&string);
            if (out_result->text == NULL) {
                rc = MYLITE_NOMEM;
            } else {
                out_result->text_size = strlen(out_result->text);
                out_result->is_null = false;
            }
        }
        mylite_dynamic_string_deinit(&string);
        return rc;
    }

    absolute_value = fabsl(value);
    if (absolute_value < sys_format_time_unit_size) {
        return format_trimmed_unit_result(out_result, value, "ps");
    }
    for (size_t index = 0U; index < sizeof(units) / sizeof(units[0]); ++index) {
        if (units[index].limit == 0.0L || absolute_value < units[index].limit) {
            return format_trimmed_unit_result(
                out_result,
                value / units[index].divisor,
                units[index].unit
            );
        }
    }
    return MYLITE_ERROR;
}

static int sys_list_add(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (arguments[1].is_null) {
        set_sys_list_null_error(database, MYLITE_SYS_FUNCTION_LIST_ADD);
        return MYLITE_ERROR;
    }
    if (arguments[0].is_null || arguments[0].text_size == 0U) {
        return sys_function_copy_result(out_result, arguments[1].text, arguments[1].text_size);
    }

    mylite_dynamic_string_init(&string);
    rc = mylite_dynamic_string_append_bytes(&string, arguments[0].text, arguments[0].text_size);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, ',');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_bytes(&string, arguments[1].text, arguments[1].text_size);
    }
    if (rc == MYLITE_OK) {
        out_result->text = mylite_dynamic_string_take(&string);
        if (out_result->text == NULL) {
            rc = MYLITE_NOMEM;
        } else {
            out_result->text_size = strlen(out_result->text);
            out_result->is_null = false;
        }
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int sys_list_drop(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (arguments[1].is_null) {
        set_sys_list_null_error(database, MYLITE_SYS_FUNCTION_LIST_DROP);
        return MYLITE_ERROR;
    }
    if (arguments[0].is_null) {
        return sys_function_null_result(out_result);
    }

    mylite_dynamic_string_init(&string);
    rc = mylite_dynamic_string_append_char(&string, ',');
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_bytes(&string, arguments[0].text, arguments[0].text_size);
    }
    if (rc == MYLITE_OK) {
        remove_list_drop_pattern(&string, arguments[1].text, arguments[1].text_size, false);
        remove_list_drop_pattern(&string, arguments[1].text, arguments[1].text_size, true);
        trim_list_drop_commas(&string);
    }
    if (rc == MYLITE_OK) {
        size_t result_size = string.length;

        out_result->text = mylite_dynamic_string_take(&string);
        if (out_result->text == NULL) {
            rc = MYLITE_NOMEM;
        } else {
            out_result->text_size = result_size;
            out_result->is_null = false;
        }
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int sys_quote_identifier(
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    struct mylite_dynamic_string string;
    char *identifier = NULL;
    int rc = MYLITE_OK;

    if (arguments[0].is_null) {
        return sys_function_null_result(out_result);
    }

    rc = copy_argument_text(&arguments[0], &identifier);
    if (rc != MYLITE_OK) {
        return rc;
    }
    mylite_dynamic_string_init(&string);
    rc = mylite_dynamic_string_append_mysql_quoted_identifier(&string, identifier);
    if (rc == MYLITE_OK) {
        out_result->text = mylite_dynamic_string_take(&string);
        if (out_result->text == NULL) {
            rc = MYLITE_NOMEM;
        } else {
            out_result->text_size = strlen(out_result->text);
            out_result->is_null = false;
        }
    }
    mylite_dynamic_string_deinit(&string);
    free(identifier);
    return rc;
}

static int sys_sys_get_config(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    bool config_is_null = true;
    const char *config_value = NULL;

    (void)database;
    if (!arguments[0].is_null) {
        config_value =
            sys_config_static_value(arguments[0].text, arguments[0].text_size, &config_is_null);
        if (config_value != NULL && !config_is_null) {
            return sys_function_copy_result(out_result, config_value, strlen(config_value));
        }
    }
    if (arguments[1].is_null) {
        return sys_function_null_result(out_result);
    }
    return sys_function_copy_result(out_result, arguments[1].text, arguments[1].text_size);
}

static int sys_version_component(
    enum mylite_sys_function_kind kind,
    struct mylite_sys_function_result *out_result
) {
    const char *version = MYLITE_MYSQL_SERVER_VERSION_STRING;
    const char *component_start = version;
    const char *component_end = NULL;
    unsigned int component_index = 0U;
    unsigned int target_index = 0U;

    if (kind == MYLITE_SYS_FUNCTION_VERSION_MINOR) {
        target_index = 1U;
    } else if (kind == MYLITE_SYS_FUNCTION_VERSION_PATCH) {
        target_index = 2U;
    }

    while (component_index < target_index && *component_start != '\0') {
        if (*component_start == '.') {
            ++component_index;
        }
        ++component_start;
    }
    component_end = component_start;
    while (*component_end != '\0' && *component_end != '.') {
        ++component_end;
    }
    return sys_function_copy_result(
        out_result,
        component_start,
        (size_t)(component_end - component_start)
    );
}

static size_t last_slash_before(const char *text, size_t end) {
    while (end > 0U) {
        --end;
        if (text[end] == '/') {
            return end;
        }
    }
    return SIZE_MAX;
}

static size_t last_dot_between(const char *text, size_t start, size_t end) {
    while (end > start) {
        --end;
        if (text[end] == '.') {
            return end;
        }
    }
    return SIZE_MAX;
}

static int copy_argument_text(
    const struct mylite_sys_function_argument *argument,
    char **out_text
) {
    char *copy = NULL;

    if (argument == NULL || out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (argument->is_null) {
        return MYLITE_OK;
    }
    if (argument->text == NULL) {
        return MYLITE_MISUSE;
    }
    copy = (char *)malloc(argument->text_size + 1U);
    if (copy == NULL) {
        return MYLITE_NOMEM;
    }
    memcpy(copy, argument->text, argument->text_size);
    copy[argument->text_size] = '\0';
    *out_text = copy;
    return MYLITE_OK;
}

static bool parse_argument_number(
    const struct mylite_sys_function_argument *argument,
    long double *out_value,
    bool *out_valid
) {
    char *copy = NULL;
    char *start = NULL;
    char *end = NULL;
    int rc = MYLITE_OK;

    if (out_value == NULL || out_valid == NULL) {
        return false;
    }
    *out_value = 0.0L;
    *out_valid = false;
    rc = copy_argument_text(argument, &copy);
    if (rc != MYLITE_OK) {
        return false;
    }
    if (copy == NULL) {
        return true;
    }
    start = copy;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        ++start;
    }
    errno = 0;
    *out_value = strtold(start, &end);
    *out_valid = errno != ERANGE && end != start;
    free(copy);
    return true;
}

static int format_fixed_unit_result(
    struct mylite_sys_function_result *out_result,
    long double value,
    const char *unit
) {
    return sys_function_format_result(out_result, "%.2Lf %s", value, unit);
}

static int format_trimmed_unit_result(
    struct mylite_sys_function_result *out_result,
    long double value,
    const char *unit
) {
    char buffer[sys_function_number_buffer_size];
    int written = snprintf(buffer, sizeof(buffer), "%.2Lf %s", value, unit);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return MYLITE_NOMEM;
    }
    trim_formatted_fraction(buffer);
    return sys_function_copy_result(out_result, buffer, strlen(buffer));
}

static void trim_formatted_fraction(char *buffer) {
    char *space = strchr(buffer, ' ');
    char *tail = space == NULL ? NULL : space - 1;

    while (tail != NULL && tail > buffer && *tail == '0') {
        memmove(tail, tail + 1, strlen(tail));
        --tail;
        --space;
    }
    if (tail != NULL && tail > buffer && *tail == '.') {
        memmove(tail, tail + 1, strlen(tail));
    }
}

static size_t sys_statement_truncate_length(struct mylite_db *database) {
    bool is_null = false;
    const char *value = sys_config_value_for(
        database,
        "statement_truncate_len",
        sizeof("statement_truncate_len") - 1U,
        &is_null
    );
    char *end = NULL;
    unsigned long parsed = 0UL;

    if (value == NULL || is_null) {
        return statement_truncate_len_default;
    }
    errno = 0;
    parsed = strtoul(value, &end, sys_config_decimal_parse_base);
    if (errno == ERANGE || end == value || (end != NULL && *end != '\0')) {
        return statement_truncate_len_default;
    }
    return (size_t)parsed;
}

static const char *sys_config_value_for(
    struct mylite_db *database,
    const char *name,
    size_t name_size,
    bool *out_is_null
) {
    size_t user_value_size = 0U;
    const char *user_value =
        sys_config_user_variable_value(database, name, name_size, &user_value_size, out_is_null);

    (void)user_value_size;
    if (user_value != NULL || (out_is_null != NULL && *out_is_null)) {
        return user_value;
    }
    return sys_config_static_value(name, name_size, out_is_null);
}

static const char *sys_config_static_value(const char *name, size_t name_size, bool *out_is_null) {
    if (out_is_null != NULL) {
        *out_is_null = false;
    }
    for (size_t index = 0U; index < sizeof(sys_config_values) / sizeof(sys_config_values[0]);
         ++index) {
        if (ascii_equals_case_insensitive(name, name_size, sys_config_values[index].name)) {
            if (out_is_null != NULL) {
                *out_is_null = sys_config_values[index].value == NULL;
            }
            return sys_config_values[index].value;
        }
    }
    return NULL;
}

static const char *sys_config_user_variable_value(
    struct mylite_db *database,
    const char *name,
    size_t name_size,
    size_t *out_value_size,
    bool *out_is_null
) {
    char variable_name[MYLITE_SESSION_USER_VARIABLE_NAME_CAPACITY];
    int written = 0;

    if (out_value_size != NULL) {
        *out_value_size = 0U;
    }
    if (out_is_null != NULL) {
        *out_is_null = false;
    }
    if (database == NULL || name == NULL) {
        return NULL;
    }
    written = snprintf(variable_name, sizeof(variable_name), "sys.%.*s", (int)name_size, name);
    if (written < 0 || (size_t)written >= sizeof(variable_name)) {
        return NULL;
    }
    for (char *cursor = variable_name; *cursor != '\0'; ++cursor) {
        *cursor = (char)tolower((unsigned char)*cursor);
    }
    for (size_t index = 0U; index < database->session.user_variable_count; ++index) {
        const struct mylite_session_user_variable *variable =
            &database->session.user_variables[index];

        if (strcmp(variable->name, variable_name) != 0) {
            continue;
        }
        if (out_is_null != NULL) {
            *out_is_null = variable->is_null;
        }
        if (variable->is_null) {
            return NULL;
        }
        if (out_value_size != NULL) {
            *out_value_size = variable->value_size;
        }
        return variable->value;
    }
    return NULL;
}

static void remove_list_drop_pattern(
    struct mylite_dynamic_string *string,
    const char *value,
    size_t value_size,
    bool include_space
) {
    size_t prefix_size = include_space ? 2U : 1U;
    size_t pattern_size = prefix_size + value_size;
    size_t index = 0U;

    if (string == NULL || string->text == NULL || value == NULL || pattern_size == 0U) {
        return;
    }
    while (index + pattern_size <= string->length) {
        bool matches = string->text[index] == ',';

        if (matches && include_space) {
            matches = string->text[index + 1U] == ' ';
        }
        if (matches && value_size != 0U) {
            matches = memcmp(string->text + index + prefix_size, value, value_size) == 0;
        }
        if (!matches) {
            ++index;
            continue;
        }
        memmove(
            string->text + index,
            string->text + index + pattern_size,
            string->length - index - pattern_size + 1U
        );
        string->length -= pattern_size;
    }
}

static void trim_list_drop_commas(struct mylite_dynamic_string *string) {
    size_t start = 0U;
    size_t end = 0U;

    if (string == NULL || string->text == NULL) {
        return;
    }
    end = string->length;
    while (start < end && string->text[start] == ',') {
        ++start;
    }
    while (end > start && string->text[end - 1U] == ',') {
        --end;
    }
    if (start != 0U && end > start) {
        memmove(string->text, string->text + start, end - start);
    }
    string->length = end - start;
    string->text[string->length] = '\0';
}

static void set_sys_list_null_error(
    struct mylite_db *database,
    enum mylite_sys_function_kind kind
) {
    const char *function_name =
        kind == MYLITE_SYS_FUNCTION_LIST_ADD ? "sys.list_add" : "sys.list_drop";
    const char *argument_name =
        kind == MYLITE_SYS_FUNCTION_LIST_ADD ? "in_add_value" : "in_drop_value";
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Function %s: %s input variable should not be NULL",
        function_name,
        argument_name
    );

    if (database == NULL) {
        return;
    }
    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_diagnostics_set_error(
            &database->diagnostics,
            mysql_error_unknown,
            "HY000",
            "sys list helper failed"
        );
        return;
    }
    mylite_diagnostics_set_error(
        &database->diagnostics,
        mysql_error_invalid_use_of_null,
        "02200",
        message
    );
}

static int sys_function_sqlite_argument(
    sqlite3_value *value,
    struct mylite_sys_function_argument *out_argument
) {
    const unsigned char *text = NULL;
    int text_size = 0;

    if (value == NULL || out_argument == NULL) {
        return MYLITE_MISUSE;
    }
    *out_argument = (struct mylite_sys_function_argument){0};
    if (sqlite3_value_type(value) == SQLITE_NULL) {
        out_argument->is_null = true;
        return MYLITE_OK;
    }
    text = sqlite3_value_text(value);
    text_size = sqlite3_value_bytes(value);
    if (text_size < 0) {
        return MYLITE_NOMEM;
    }
    if (text == NULL && text_size == 0) {
        text = (const unsigned char *)"";
    }
    if (text == NULL) {
        return MYLITE_NOMEM;
    }
    out_argument->text = (const char *)text;
    out_argument->text_size = (size_t)text_size;
    return MYLITE_OK;
}

static void sys_function_sqlite_result(
    sqlite3_context *context,
    struct mylite_sys_function_result *result
) {
    if (result->is_null) {
        sqlite3_result_null(context);
        return;
    }
    sqlite3_result_text64(
        context,
        result->text,
        (sqlite3_uint64)result->text_size,
        free,
        SQLITE_UTF8
    );
    *result = (struct mylite_sys_function_result){0};
}

static void sys_function_sqlite_error(
    sqlite3_context *context,
    struct mylite_db *database,
    int rc
) {
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (database != NULL && mylite_diagnostics_errmsg(&database->diagnostics) != NULL) {
        sqlite3_result_error(context, mylite_diagnostics_errmsg(&database->diagnostics), -1);
        return;
    }
    sqlite3_result_error(context, "MyLite sys helper failed", -1);
}
