#include "mylite_sys_functions.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_dynamic_string.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_mysql_server_identity.h"
#include "mylite_numeric_locale.h"
#include "mylite_source_span.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"
#include "mylite_statement_digest.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    statement_truncate_len_default = 64,
    sys_function_number_buffer_size = 96,
    sys_function_json_buffer_size = 256,
    sys_function_uint64_buffer_size = 32,
    sys_config_decimal_parse_base = 10,
};

static const long double sys_format_bytes_unit_size = 1024.0L;
static const long double sys_format_time_unit_size = 1000.0L;

struct sys_function_descriptor {
    const char *name;
    const char *sqlite_name;
    size_t argument_count;
    enum mylite_sys_function_kind kind;
    bool allow_unqualified;
    bool allow_sys_schema;
};

struct sys_config_value {
    const char *name;
    const char *value;
};

struct sys_consumer_default {
    const char *name;
    const char *enabled;
};

struct path_prefix_rewrite {
    const char *prefix;
    const char *replacement;
};

static const struct sys_function_descriptor sys_function_descriptors[] = {
    {
        "FORMAT_BYTES",
        "_mylite_native_format_bytes",
        1U,
        MYLITE_SYS_FUNCTION_NATIVE_FORMAT_BYTES,
        true,
        false,
    },
    {
        "FORMAT_PICO_TIME",
        "_mylite_native_format_pico_time",
        1U,
        MYLITE_SYS_FUNCTION_NATIVE_FORMAT_PICO_TIME,
        true,
        false,
    },
    {
        "PS_CURRENT_THREAD_ID",
        "_mylite_native_ps_current_thread_id",
        0U,
        MYLITE_SYS_FUNCTION_NATIVE_PS_CURRENT_THREAD_ID,
        true,
        false,
    },
    {
        "PS_THREAD_ID",
        "_mylite_native_ps_thread_id",
        1U,
        MYLITE_SYS_FUNCTION_NATIVE_PS_THREAD_ID,
        true,
        false,
    },
    {
        "VALIDATE_PASSWORD_STRENGTH",
        "_mylite_validate_password_strength",
        1U,
        MYLITE_SYS_FUNCTION_VALIDATE_PASSWORD_STRENGTH,
        true,
        false,
    },
    {
        "ROLES_GRAPHML",
        "_mylite_roles_graphml",
        0U,
        MYLITE_SYS_FUNCTION_ROLES_GRAPHML,
        true,
        false,
    },
    {
        "STATEMENT_DIGEST_TEXT",
        "_mylite_statement_digest_text",
        1U,
        MYLITE_SYS_FUNCTION_STATEMENT_DIGEST_TEXT,
        true,
        false,
    },
    {
        "STATEMENT_DIGEST",
        "_mylite_statement_digest",
        1U,
        MYLITE_SYS_FUNCTION_STATEMENT_DIGEST,
        true,
        false,
    },
    {"extract_schema_from_file_name",
     "_mylite_sys_extract_schema_from_file_name",
     1U,
     MYLITE_SYS_FUNCTION_EXTRACT_SCHEMA_FROM_FILE_NAME,
     true,
     true},
    {"extract_table_from_file_name",
     "_mylite_sys_extract_table_from_file_name",
     1U,
     MYLITE_SYS_FUNCTION_EXTRACT_TABLE_FROM_FILE_NAME,
     true,
     true},
    /*
     * Internal post-normalization spellings keep schema-qualified sys helpers
     * distinct from native functions with the same unqualified MySQL name.
     */
    {
        "_mylite_sys_format_bytes",
        "_mylite_sys_format_bytes_alias",
        1U,
        MYLITE_SYS_FUNCTION_FORMAT_BYTES,
        true,
        false,
    },
    {
        "format_bytes",
        "_mylite_sys_format_bytes",
        1U,
        MYLITE_SYS_FUNCTION_FORMAT_BYTES,
        true,
        true,
    },
    {
        "format_path",
        "_mylite_sys_format_path",
        1U,
        MYLITE_SYS_FUNCTION_FORMAT_PATH,
        true,
        true,
    },
    {"format_statement",
     "_mylite_sys_format_statement",
     1U,
     MYLITE_SYS_FUNCTION_FORMAT_STATEMENT,
     true,
     true},
    {
        "format_time",
        "_mylite_sys_format_time",
        1U,
        MYLITE_SYS_FUNCTION_FORMAT_TIME,
        true,
        true,
    },
    {"list_add", "_mylite_sys_list_add", 2U, MYLITE_SYS_FUNCTION_LIST_ADD, true, true},
    {"list_drop", "_mylite_sys_list_drop", 2U, MYLITE_SYS_FUNCTION_LIST_DROP, true, true},
    {"quote_identifier",
     "_mylite_sys_quote_identifier",
     1U,
     MYLITE_SYS_FUNCTION_QUOTE_IDENTIFIER,
     true,
     true},
    {"sys_get_config",
     "_mylite_sys_sys_get_config",
     2U,
     MYLITE_SYS_FUNCTION_SYS_GET_CONFIG,
     true,
     true},
    {
        "version_major",
        "_mylite_sys_version_major",
        0U,
        MYLITE_SYS_FUNCTION_VERSION_MAJOR,
        true,
        true,
    },
    {
        "version_minor",
        "_mylite_sys_version_minor",
        0U,
        MYLITE_SYS_FUNCTION_VERSION_MINOR,
        true,
        true,
    },
    {
        "version_patch",
        "_mylite_sys_version_patch",
        0U,
        MYLITE_SYS_FUNCTION_VERSION_PATCH,
        true,
        true,
    },
    {"ps_is_account_enabled",
     "_mylite_sys_ps_is_account_enabled",
     2U,
     MYLITE_SYS_FUNCTION_PS_IS_ACCOUNT_ENABLED,
     true,
     true},
    {"ps_is_consumer_enabled",
     "_mylite_sys_ps_is_consumer_enabled",
     1U,
     MYLITE_SYS_FUNCTION_PS_IS_CONSUMER_ENABLED,
     true,
     true},
    {"ps_is_instrument_default_enabled",
     "_mylite_sys_ps_is_instrument_default_enabled",
     1U,
     MYLITE_SYS_FUNCTION_PS_IS_INSTRUMENT_DEFAULT_ENABLED,
     true,
     true},
    {"ps_is_instrument_default_timed",
     "_mylite_sys_ps_is_instrument_default_timed",
     1U,
     MYLITE_SYS_FUNCTION_PS_IS_INSTRUMENT_DEFAULT_TIMED,
     true,
     true},
    {"ps_is_thread_instrumented",
     "_mylite_sys_ps_is_thread_instrumented",
     1U,
     MYLITE_SYS_FUNCTION_PS_IS_THREAD_INSTRUMENTED,
     true,
     true},
    {
        "ps_thread_account",
        "_mylite_sys_ps_thread_account",
        1U,
        MYLITE_SYS_FUNCTION_PS_THREAD_ACCOUNT,
        true,
        true,
    },
    {
        "_mylite_sys_ps_thread_id",
        "_mylite_sys_ps_thread_id_alias",
        1U,
        MYLITE_SYS_FUNCTION_PS_THREAD_ID,
        true,
        false,
    },
    {
        "ps_thread_id",
        "_mylite_sys_ps_thread_id",
        1U,
        MYLITE_SYS_FUNCTION_PS_THREAD_ID,
        true,
        true,
    },
    {"ps_thread_stack",
     "_mylite_sys_ps_thread_stack",
     2U,
     MYLITE_SYS_FUNCTION_PS_THREAD_STACK,
     true,
     true},
    {"ps_thread_trx_info",
     "_mylite_sys_ps_thread_trx_info",
     1U,
     MYLITE_SYS_FUNCTION_PS_THREAD_TRX_INFO,
     true,
     true},
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

static const struct sys_consumer_default sys_consumer_defaults[] = {
    {"events_stages_current", "NO"},
    {"events_stages_history", "NO"},
    {"events_stages_history_long", "NO"},
    {"events_statements_cpu", "NO"},
    {"events_statements_current", "YES"},
    {"events_statements_history", "YES"},
    {"events_statements_history_long", "NO"},
    {"events_transactions_current", "YES"},
    {"events_transactions_history", "YES"},
    {"events_transactions_history_long", "NO"},
    {"events_waits_current", "NO"},
    {"events_waits_history", "NO"},
    {"events_waits_history_long", "NO"},
    {"global_instrumentation", "YES"},
    {"statements_digest", "YES"},
    {"thread_instrumentation", "YES"},
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
    size_t name_size,
    bool schema_qualified
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
static int sys_format_bytes_with_native_warnings(
    struct mylite_db *database,
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
static int sys_format_pico_time(
    struct mylite_db *database,
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
static int sys_ps_is_account_enabled(struct mylite_sys_function_result *out_result);
static int sys_ps_is_consumer_enabled(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_ps_is_instrument_default_enabled(
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_ps_is_instrument_default_timed(
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_ps_is_thread_instrumented(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_ps_thread_account(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_ps_thread_id(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_ps_current_thread_id(
    struct mylite_db *database,
    struct mylite_sys_function_result *out_result
);
static int sys_ps_thread_id_native(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_ps_thread_stack(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_ps_thread_trx_info(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_validate_password_strength(
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_roles_graphml(struct mylite_sys_function_result *out_result);
static int sys_statement_digest(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static int sys_statement_digest_text(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
);
static size_t last_slash_before(const char *text, size_t end);
static size_t last_dot_between(const char *text, size_t start, size_t end);
static int copy_argument_text(const struct mylite_sys_function_argument *argument, char **out_text);
static bool argument_text_starts_with_case_insensitive(
    const struct mylite_sys_function_argument *argument,
    const char *prefix
);
static bool parse_argument_number(
    const struct mylite_sys_function_argument *argument,
    long double *out_value,
    bool *out_valid
);
static bool argument_number_has_trailing_text(const struct mylite_sys_function_argument *argument);
static int parse_unsigned_argument(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *argument,
    const char *column_name,
    uint64_t *out_value
);
static int parse_native_thread_id_argument(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *argument,
    uint64_t *out_value,
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
static const char *sys_consumer_default_value(const char *name, size_t name_size);
static bool sys_instrument_default_disabled(const struct mylite_sys_function_argument *argument);
static bool sys_instrument_default_not_timed(const struct mylite_sys_function_argument *argument);
static int sys_processlist_contains_connection_id(
    struct mylite_db *database,
    uint64_t id,
    bool *out_found
);
static int sys_processlist_account_for_thread_id(
    struct mylite_db *database,
    uint64_t id,
    struct mylite_sys_function_result *out_result
);
static int sys_current_connection_id_result(
    struct mylite_db *database,
    struct mylite_sys_function_result *out_result
);
static int append_truncated_native_double_warning(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *argument
);
static int append_truncated_native_integer_warning(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *argument
);
static int append_truncated_native_warning(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *argument,
    const char *type_name
);
static int sys_thread_stack_empty_json_result(
    struct mylite_db *database,
    struct mylite_sys_function_result *out_result
);
static void set_sys_invalid_consumer_error(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *argument
);
static void set_sys_unsigned_argument_error(
    struct mylite_db *database,
    int code,
    const char *sqlstate,
    const char *column_name,
    const struct mylite_sys_function_argument *argument
);
static void remove_list_drop_pattern(
    struct mylite_dynamic_string *string,
    const char *value,
    size_t value_size,
    bool include_space
);
static void trim_list_drop_commas(struct mylite_dynamic_string *string);
static void set_sys_list_null_error(struct mylite_db *database, enum mylite_sys_function_kind kind);
static void sys_function_identifier_span_text(
    const struct mylite_sql_source_span *span,
    const char **out_text,
    size_t *out_size
);
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

    descriptor = sys_function_descriptor_by_name(name, name_size, schema != NULL);
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
    const char *schema_text = NULL;
    const char *name_text = NULL;
    size_t schema_size = 0U;
    size_t name_size = 0U;

    if (name == NULL) {
        if (out_kind != NULL) {
            *out_kind = MYLITE_SYS_FUNCTION_NONE;
        }
        return false;
    }
    if (schema != NULL) {
        sys_function_identifier_span_text(schema, &schema_text, &schema_size);
    }
    sys_function_identifier_span_text(name, &name_text, &name_size);
    return mylite_sys_function_lookup(schema_text, schema_size, name_text, name_size, out_kind);
}

static void sys_function_identifier_span_text(
    const struct mylite_sql_source_span *span,
    const char **out_text,
    size_t *out_size
) {
    const char *text = span == NULL ? NULL : span->text;
    size_t size = span == NULL ? 0U : span->length;

    if (text != NULL && size >= 2U &&
        ((text[0] == '`' && text[size - 1U] == '`') || (text[0] == '"' && text[size - 1U] == '"')
        )) {
        ++text;
        size -= 2U;
    }
    *out_text = text;
    *out_size = size;
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
    case MYLITE_SYS_FUNCTION_NATIVE_FORMAT_BYTES:
        return sys_format_bytes_with_native_warnings(database, arguments, out_result);
    case MYLITE_SYS_FUNCTION_NATIVE_FORMAT_PICO_TIME:
        return sys_format_pico_time(database, arguments, out_result);
    case MYLITE_SYS_FUNCTION_NATIVE_PS_CURRENT_THREAD_ID:
        return sys_ps_current_thread_id(database, out_result);
    case MYLITE_SYS_FUNCTION_NATIVE_PS_THREAD_ID:
        return sys_ps_thread_id_native(database, arguments, out_result);
    case MYLITE_SYS_FUNCTION_VALIDATE_PASSWORD_STRENGTH:
        return sys_validate_password_strength(arguments, out_result);
    case MYLITE_SYS_FUNCTION_ROLES_GRAPHML:
        return sys_roles_graphml(out_result);
    case MYLITE_SYS_FUNCTION_STATEMENT_DIGEST:
        return sys_statement_digest(database, arguments, out_result);
    case MYLITE_SYS_FUNCTION_STATEMENT_DIGEST_TEXT:
        return sys_statement_digest_text(database, arguments, out_result);
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
    case MYLITE_SYS_FUNCTION_PS_IS_ACCOUNT_ENABLED:
        return sys_ps_is_account_enabled(out_result);
    case MYLITE_SYS_FUNCTION_PS_IS_CONSUMER_ENABLED:
        return sys_ps_is_consumer_enabled(database, arguments, out_result);
    case MYLITE_SYS_FUNCTION_PS_IS_INSTRUMENT_DEFAULT_ENABLED:
        return sys_ps_is_instrument_default_enabled(arguments, out_result);
    case MYLITE_SYS_FUNCTION_PS_IS_INSTRUMENT_DEFAULT_TIMED:
        return sys_ps_is_instrument_default_timed(arguments, out_result);
    case MYLITE_SYS_FUNCTION_PS_IS_THREAD_INSTRUMENTED:
        return sys_ps_is_thread_instrumented(database, arguments, out_result);
    case MYLITE_SYS_FUNCTION_PS_THREAD_ACCOUNT:
        return sys_ps_thread_account(database, arguments, out_result);
    case MYLITE_SYS_FUNCTION_PS_THREAD_ID:
        return sys_ps_thread_id(database, arguments, out_result);
    case MYLITE_SYS_FUNCTION_PS_THREAD_STACK:
        return sys_ps_thread_stack(database, arguments, out_result);
    case MYLITE_SYS_FUNCTION_PS_THREAD_TRX_INFO:
        return sys_ps_thread_trx_info(database, arguments, out_result);
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
    size_t name_size,
    bool schema_qualified
) {
    for (size_t index = 0U;
         index < sizeof(sys_function_descriptors) / sizeof(sys_function_descriptors[0]);
         ++index) {
        const struct sys_function_descriptor *descriptor = &sys_function_descriptors[index];
        const bool exposed =
            schema_qualified ? descriptor->allow_sys_schema : descriptor->allow_unqualified;

        if (exposed && ascii_equals_case_insensitive(name, name_size, descriptor->name)) {
            return descriptor;
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
    written = mylite_numeric_vformat(buffer, sizeof(buffer), format, args);
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

static int sys_validate_password_strength(
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    if (arguments == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    if (arguments[0].is_null) {
        return sys_function_null_result(out_result);
    }
    return sys_function_copy_result(out_result, "0", 1U);
}

static int sys_roles_graphml(struct mylite_sys_function_result *out_result) {
    static const char roles_graphml[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\" "
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "xsi:schemaLocation=\"http://graphml.graphdrawing.org/xmlns "
        "http://graphml.graphdrawing.org/xmlns/1.0/graphml.xsd\">\n"
        "  <key id=\"key0\" for=\"edge\" attr.name=\"color\" attr.type=\"int\" />\n"
        "  <key id=\"key1\" for=\"node\" attr.name=\"name\" attr.type=\"string\" />\n"
        "  <graph id=\"G\" edgedefault=\"directed\" parse.nodeids=\"canonical\" "
        "parse.edgeids=\"canonical\" parse.order=\"nodesfirst\">\n"
        "    <node id=\"n0\">\n"
        "      <data key=\"key1\">`root`@`%`</data>\n"
        "    </node>\n"
        "  </graph>\n"
        "</graphml>\n";

    return sys_function_copy_result(out_result, roles_graphml, sizeof(roles_graphml) - 1U);
}

static int sys_statement_digest(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    if (database == NULL || arguments == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    if (arguments[0].is_null) {
        return sys_function_null_result(out_result);
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        "STATEMENT_DIGEST() hash computation is not yet supported"
    );
    return MYLITE_ERROR;
}

static int sys_statement_digest_text(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    if (arguments == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    if (arguments[0].is_null) {
        return sys_function_null_result(out_result);
    }
    return mylite_statement_digest_text(
        database,
        arguments[0].text,
        arguments[0].text_size,
        &out_result->text,
        &out_result->text_size
    );
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

static int sys_format_bytes_with_native_warnings(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    long double ignored = 0.0L;
    bool valid = false;
    int rc = MYLITE_OK;

    if (arguments[0].is_null) {
        return sys_function_null_result(out_result);
    }
    if (!parse_argument_number(&arguments[0], &ignored, &valid)) {
        return MYLITE_NOMEM;
    }
    if (!valid || argument_number_has_trailing_text(&arguments[0])) {
        rc = append_truncated_native_double_warning(database, &arguments[0]);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return sys_format_bytes(arguments, out_result);
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

static int sys_format_pico_time(
    struct mylite_db *database,
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
        {60000000000000.0L, 3600000000000000.0L, "min"},
        {3600000000000000.0L, 86400000000000000.0L, "h"},
        {86400000000000000.0L, 0.0L, "d"},
    };

    long double value = 0.0L;
    long double absolute_value = 0.0L;
    bool valid = false;
    int rc = MYLITE_OK;

    if (arguments[0].is_null) {
        return sys_function_null_result(out_result);
    }
    if (!parse_argument_number(&arguments[0], &value, &valid)) {
        return MYLITE_NOMEM;
    }
    if (!valid || argument_number_has_trailing_text(&arguments[0])) {
        rc = append_truncated_native_double_warning(database, &arguments[0]);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (!valid) {
        value = 0.0L;
    }

    absolute_value = fabsl(value);
    if (absolute_value < sys_format_time_unit_size) {
        return sys_function_format_result(out_result, "%3.0Lf ps", value);
    }
    for (size_t index = 0U; index < sizeof(units) / sizeof(units[0]); ++index) {
        if (units[index].limit == 0.0L || absolute_value < units[index].limit) {
            return format_fixed_unit_result(
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

static int sys_ps_is_account_enabled(struct mylite_sys_function_result *out_result) {
    return sys_function_copy_result(out_result, "YES", sizeof("YES") - 1U);
}

static int sys_ps_is_consumer_enabled(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    const char *value = NULL;

    if (arguments[0].is_null) {
        return sys_function_null_result(out_result);
    }
    value = sys_consumer_default_value(arguments[0].text, arguments[0].text_size);
    if (value == NULL) {
        set_sys_invalid_consumer_error(database, &arguments[0]);
        return MYLITE_ERROR;
    }
    return sys_function_copy_result(out_result, value, strlen(value));
}

static int sys_ps_is_instrument_default_enabled(
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    const char *value = sys_instrument_default_disabled(&arguments[0]) ? "NO" : "YES";

    return sys_function_copy_result(out_result, value, strlen(value));
}

static int sys_ps_is_instrument_default_timed(
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    const char *value = sys_instrument_default_not_timed(&arguments[0]) ? "NO" : "YES";

    return sys_function_copy_result(out_result, value, strlen(value));
}

static int sys_ps_is_thread_instrumented(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    uint64_t connection_id = 0U;
    bool found = false;
    int rc = MYLITE_OK;

    if (arguments[0].is_null) {
        return sys_function_null_result(out_result);
    }
    rc = parse_unsigned_argument(database, &arguments[0], "in_connection_id", &connection_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = sys_processlist_contains_connection_id(database, connection_id, &found);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!found) {
        return sys_function_copy_result(out_result, "UNKNOWN", sizeof("UNKNOWN") - 1U);
    }
    return sys_function_copy_result(out_result, "YES", sizeof("YES") - 1U);
}

static int sys_ps_thread_account(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    uint64_t thread_id = 0U;
    int rc = MYLITE_OK;

    if (arguments[0].is_null) {
        return sys_function_null_result(out_result);
    }
    rc = parse_unsigned_argument(database, &arguments[0], "in_thread_id", &thread_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return sys_processlist_account_for_thread_id(database, thread_id, out_result);
}

static int sys_ps_thread_id(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    uint64_t connection_id = 0U;
    bool found = false;
    int rc = MYLITE_OK;

    if (arguments[0].is_null) {
        return sys_current_connection_id_result(database, out_result);
    }
    rc = parse_unsigned_argument(database, &arguments[0], "in_connection_id", &connection_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = sys_processlist_contains_connection_id(database, connection_id, &found);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!found) {
        return sys_function_null_result(out_result);
    }
    return sys_function_format_result(out_result, "%" PRIu64, connection_id);
}

static int sys_ps_current_thread_id(
    struct mylite_db *database,
    struct mylite_sys_function_result *out_result
) {
    return sys_current_connection_id_result(database, out_result);
}

static int sys_ps_thread_id_native(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    uint64_t connection_id = 0U;
    bool argument_valid = false;
    bool found = false;
    int rc = MYLITE_OK;

    if (arguments[0].is_null) {
        return sys_function_null_result(out_result);
    }
    rc = parse_native_thread_id_argument(database, &arguments[0], &connection_id, &argument_valid);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!argument_valid) {
        return sys_function_null_result(out_result);
    }
    rc = sys_processlist_contains_connection_id(database, connection_id, &found);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!found) {
        return sys_function_null_result(out_result);
    }
    return sys_function_format_result(out_result, "%" PRIu64, connection_id);
}

static int sys_ps_thread_stack(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    uint64_t ignored_thread_id = 0U;
    int rc = MYLITE_OK;

    if (!arguments[0].is_null) {
        rc = parse_unsigned_argument(database, &arguments[0], "thd_id", &ignored_thread_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    (void)ignored_thread_id;
    (void)arguments;
    return sys_thread_stack_empty_json_result(database, out_result);
}

static int sys_ps_thread_trx_info(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *arguments,
    struct mylite_sys_function_result *out_result
) {
    uint64_t thread_id = 0U;
    bool found = false;
    int rc = MYLITE_OK;

    if (arguments[0].is_null) {
        return sys_function_null_result(out_result);
    }
    rc = parse_unsigned_argument(database, &arguments[0], "in_thread_id", &thread_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = sys_processlist_contains_connection_id(database, thread_id, &found);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!found) {
        return sys_function_null_result(out_result);
    }
    return sys_function_copy_result(out_result, "[]", sizeof("[]") - 1U);
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

static bool argument_text_starts_with_case_insensitive(
    const struct mylite_sys_function_argument *argument,
    const char *prefix
) {
    size_t prefix_size = prefix == NULL ? 0U : strlen(prefix);

    if (argument == NULL || argument->is_null || argument->text == NULL || prefix == NULL ||
        argument->text_size < prefix_size) {
        return false;
    }
    for (size_t index = 0U; index < prefix_size; ++index) {
        if (tolower((unsigned char)argument->text[index]) !=
            tolower((unsigned char)prefix[index])) {
            return false;
        }
    }
    return true;
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
    *out_value = mylite_numeric_parse_long_double(start, &end);
    *out_valid = errno != ERANGE && end != start;
    free(copy);
    return true;
}

static bool argument_number_has_trailing_text(const struct mylite_sys_function_argument *argument) {
    char *copy = NULL;
    char *start = NULL;
    char *end = NULL;
    bool has_trailing_text = false;
    int rc = MYLITE_OK;

    if (argument == NULL || argument->is_null) {
        return false;
    }
    rc = copy_argument_text(argument, &copy);
    if (rc != MYLITE_OK || copy == NULL) {
        return false;
    }
    start = copy;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        ++start;
    }
    errno = 0;
    (void)mylite_numeric_parse_long_double(start, &end);
    if (errno != ERANGE && end != start) {
        while (end != NULL && *end != '\0' && isspace((unsigned char)*end)) {
            ++end;
        }
        has_trailing_text = end != NULL && *end != '\0';
    }
    free(copy);
    return has_trailing_text;
}

static int parse_unsigned_argument(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *argument,
    const char *column_name,
    uint64_t *out_value
) {
    char *copy = NULL;
    char *start = NULL;
    char *end = NULL;
    unsigned long long parsed = 0ULL;
    int rc = MYLITE_OK;

    if (argument == NULL || column_name == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0U;
    rc = copy_argument_text(argument, &copy);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (copy == NULL) {
        return MYLITE_MISUSE;
    }
    start = copy;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        ++start;
    }
    if (*start == '-') {
        set_sys_unsigned_argument_error(
            database,
            mysql_error_data_out_of_range,
            "22003",
            column_name,
            argument
        );
        free(copy);
        return MYLITE_ERROR;
    }
    errno = 0;
    parsed = strtoull(start, &end, sys_config_decimal_parse_base);
    while (end != NULL && *end != '\0' && isspace((unsigned char)*end)) {
        ++end;
    }
    if (errno == ERANGE) {
        set_sys_unsigned_argument_error(
            database,
            mysql_error_data_out_of_range,
            "22003",
            column_name,
            argument
        );
        free(copy);
        return MYLITE_ERROR;
    }
    if (end == start || (end != NULL && *end != '\0')) {
        set_sys_unsigned_argument_error(
            database,
            mysql_error_truncated_wrong_value_for_field,
            "HY000",
            column_name,
            argument
        );
        free(copy);
        return MYLITE_ERROR;
    }
    *out_value = (uint64_t)parsed;
    free(copy);
    return MYLITE_OK;
}

static int parse_native_thread_id_argument(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *argument,
    uint64_t *out_value,
    bool *out_valid
) {
    char *copy = NULL;
    char *start = NULL;
    char *end = NULL;
    long double parsed = 0.0L;
    int rc = MYLITE_OK;

    if (argument == NULL || out_value == NULL || out_valid == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0U;
    *out_valid = false;
    rc = copy_argument_text(argument, &copy);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (copy == NULL) {
        return MYLITE_MISUSE;
    }
    start = copy;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        ++start;
    }
    if (*start == '-') {
        free(copy);
        return MYLITE_OK;
    }

    errno = 0;
    parsed = mylite_numeric_parse_long_double(start, &end);
    while (end != NULL && *end != '\0' && isspace((unsigned char)*end)) {
        ++end;
    }
    if (errno == ERANGE || end == start || (end != NULL && *end != '\0')) {
        rc = append_truncated_native_integer_warning(database, argument);
        free(copy);
        return rc;
    }
    if (parsed < 0.0L || parsed > (long double)UINT64_MAX) {
        free(copy);
        return MYLITE_OK;
    }
    *out_value = (uint64_t)parsed;
    *out_valid = true;
    free(copy);
    return MYLITE_OK;
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
    int written = mylite_numeric_format(buffer, sizeof(buffer), "%.2Lf %s", value, unit);

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

static const char *sys_consumer_default_value(const char *name, size_t name_size) {
    for (size_t index = 0U;
         index < sizeof(sys_consumer_defaults) / sizeof(sys_consumer_defaults[0]);
         ++index) {
        if (ascii_equals_case_insensitive(name, name_size, sys_consumer_defaults[index].name)) {
            return sys_consumer_defaults[index].enabled;
        }
    }
    return NULL;
}

static bool sys_instrument_default_disabled(const struct mylite_sys_function_argument *argument) {
    if (argument == NULL || argument->is_null) {
        return false;
    }
    return argument_text_starts_with_case_insensitive(argument, "wait/synch/mutex/pfs/") ||
           argument_text_starts_with_case_insensitive(
               argument,
               "wait/synch/mutex/sql/MYSQL_BIN_LOG::"
           ) ||
           ascii_equals_case_insensitive(
               argument->text,
               argument->text_size,
               "wait/synch/mutex/sql/TC_LOG_MMAP::LOCK_tc"
           );
}

static bool sys_instrument_default_not_timed(const struct mylite_sys_function_argument *argument) {
    if (argument == NULL || argument->is_null) {
        return false;
    }
    return sys_instrument_default_disabled(argument) ||
           argument_text_starts_with_case_insensitive(argument, "memory/");
}

static int sys_processlist_contains_connection_id(
    struct mylite_db *database,
    uint64_t id,
    bool *out_found
) {
    struct mylite_processlist_session_snapshot *sessions = NULL;
    size_t session_count = 0U;
    int rc = MYLITE_OK;

    if (out_found == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found = false;
    if (database == NULL) {
        return MYLITE_OK;
    }
    rc = mylite_connection_collect_processlist_sessions(database, &sessions, &session_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    for (size_t index = 0U; index < session_count && !*out_found; ++index) {
        *out_found = sessions[index].connection_id == id;
    }
    free(sessions);
    return MYLITE_OK;
}

static int sys_processlist_account_for_thread_id(
    struct mylite_db *database,
    uint64_t id,
    struct mylite_sys_function_result *out_result
) {
    struct mylite_processlist_session_snapshot *sessions = NULL;
    size_t session_count = 0U;
    int rc = MYLITE_OK;

    if (database == NULL) {
        return sys_function_null_result(out_result);
    }
    rc = mylite_connection_collect_processlist_sessions(database, &sessions, &session_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    for (size_t index = 0U; index < session_count; ++index) {
        if (sessions[index].connection_id == id) {
            rc = sys_function_copy_result(
                out_result,
                sessions[index].client_user_identity,
                strlen(sessions[index].client_user_identity)
            );
            free(sessions);
            return rc;
        }
    }
    free(sessions);
    return sys_function_null_result(out_result);
}

static int sys_current_connection_id_result(
    struct mylite_db *database,
    struct mylite_sys_function_result *out_result
) {
    const struct mylite_session_state *session = mylite_connection_session_state(database);
    char buffer[sys_function_uint64_buffer_size];
    int written = 0;

    if (session == NULL) {
        return sys_function_null_result(out_result);
    }
    written = snprintf(buffer, sizeof(buffer), "%" PRIu64, session->connection_id);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return MYLITE_NOMEM;
    }
    return sys_function_copy_result(out_result, buffer, (size_t)written);
}

static int append_truncated_native_double_warning(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *argument
) {
    return append_truncated_native_warning(database, argument, "DOUBLE");
}

static int append_truncated_native_integer_warning(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *argument
) {
    return append_truncated_native_warning(database, argument, "INTEGER");
}

static int append_truncated_native_warning(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *argument,
    const char *type_name
) {
    enum { warning_value_preview_length = 160 };

    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t value_size = 0U;
    int written = 0;

    if (database == NULL) {
        return MYLITE_OK;
    }
    if (argument == NULL || type_name == NULL) {
        return MYLITE_MISUSE;
    }
    value_size = argument->text_size > warning_value_preview_length ? warning_value_preview_length
                                                                    : argument->text_size;
    written = snprintf(
        message,
        sizeof(message),
        "Truncated incorrect %s value: '%.*s'",
        type_name,
        (int)value_size,
        argument->text == NULL ? "" : argument->text
    );
    if (written < 0 || (size_t)written >= sizeof(message)) {
        return MYLITE_ERROR;
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        strcmp(type_name, "INTEGER") == 0 ? mysql_warning_truncated_incorrect_integer
                                          : mysql_warning_truncated_incorrect_double,
        "22007",
        message
    );
}

static int sys_thread_stack_empty_json_result(
    struct mylite_db *database,
    struct mylite_sys_function_result *out_result
) {
    const struct mylite_session_state *session = mylite_connection_session_state(database);
    const char *user = session == NULL ? "root@%" : session->current_user_identity;
    char buffer[sys_function_json_buffer_size];
    int written = snprintf(
        buffer,
        sizeof(buffer),
        "{\"rankdir\": \"LR\",\"nodesep\": \"0.10\",\"stack_created\": "
        "\"1970-01-01 00:00:00\",\"mysql_version\": \"%s\",\"mysql_user\": \"%s\","
        "\"events\": []}",
        MYLITE_MYSQL_SERVER_VERSION_STRING,
        user
    );

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return MYLITE_NOMEM;
    }
    return sys_function_copy_result(out_result, buffer, (size_t)written);
}

static void set_sys_invalid_consumer_error(
    struct mylite_db *database,
    const struct mylite_sys_function_argument *argument
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = 0;

    if (database == NULL || argument == NULL) {
        return;
    }
    written = snprintf(
        message,
        sizeof(message),
        "Invalid argument error: %.*s in function sys.ps_is_consumer_enabled.",
        (int)argument->text_size,
        argument->text == NULL ? "" : argument->text
    );
    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_diagnostics_set_error(
            &database->diagnostics,
            mysql_error_unknown,
            "HY000",
            "invalid sys consumer"
        );
        return;
    }
    mylite_diagnostics_set_error(
        &database->diagnostics,
        mysql_error_invalid_argument_for_function,
        "HY000",
        message
    );
}

static void set_sys_unsigned_argument_error(
    struct mylite_db *database,
    int code,
    const char *sqlstate,
    const char *column_name,
    const struct mylite_sys_function_argument *argument
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *format = code == mysql_error_data_out_of_range
                             ? "Out of range value for column '%s' at row 1"
                             : "Incorrect integer value: '%.*s' for column '%s' at row 1";
    int written = 0;

    if (database == NULL || sqlstate == NULL || column_name == NULL || argument == NULL) {
        return;
    }
    if (code == mysql_error_data_out_of_range) {
        written = snprintf(message, sizeof(message), format, column_name);
    } else {
        written = snprintf(
            message,
            sizeof(message),
            format,
            (int)argument->text_size,
            argument->text == NULL ? "" : argument->text,
            column_name
        );
    }
    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_diagnostics_set_error(
            &database->diagnostics,
            mysql_error_unknown,
            "HY000",
            "invalid sys unsigned argument"
        );
        return;
    }
    mylite_diagnostics_set_error(&database->diagnostics, code, sqlstate, message);
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
