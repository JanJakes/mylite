#ifndef MYLITE_RUNTIME_MYLITE_SYS_FUNCTIONS_H
#define MYLITE_RUNTIME_MYLITE_SYS_FUNCTIONS_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

struct mylite_sql_source_span;

enum mylite_sys_function_kind {
    MYLITE_SYS_FUNCTION_NONE = 0,
    MYLITE_SYS_FUNCTION_EXTRACT_SCHEMA_FROM_FILE_NAME = 1,
    MYLITE_SYS_FUNCTION_EXTRACT_TABLE_FROM_FILE_NAME = 2,
    MYLITE_SYS_FUNCTION_FORMAT_BYTES = 3,
    MYLITE_SYS_FUNCTION_FORMAT_PATH = 4,
    MYLITE_SYS_FUNCTION_FORMAT_STATEMENT = 5,
    MYLITE_SYS_FUNCTION_FORMAT_TIME = 6,
    MYLITE_SYS_FUNCTION_LIST_ADD = 7,
    MYLITE_SYS_FUNCTION_LIST_DROP = 8,
    MYLITE_SYS_FUNCTION_QUOTE_IDENTIFIER = 9,
    MYLITE_SYS_FUNCTION_SYS_GET_CONFIG = 10,
    MYLITE_SYS_FUNCTION_VERSION_MAJOR = 11,
    MYLITE_SYS_FUNCTION_VERSION_MINOR = 12,
    MYLITE_SYS_FUNCTION_VERSION_PATCH = 13,
    MYLITE_SYS_FUNCTION_PS_IS_ACCOUNT_ENABLED = 14,
    MYLITE_SYS_FUNCTION_PS_IS_CONSUMER_ENABLED = 15,
    MYLITE_SYS_FUNCTION_PS_IS_INSTRUMENT_DEFAULT_ENABLED = 16,
    MYLITE_SYS_FUNCTION_PS_IS_INSTRUMENT_DEFAULT_TIMED = 17,
    MYLITE_SYS_FUNCTION_PS_IS_THREAD_INSTRUMENTED = 18,
    MYLITE_SYS_FUNCTION_PS_THREAD_ACCOUNT = 19,
    MYLITE_SYS_FUNCTION_PS_THREAD_ID = 20,
    MYLITE_SYS_FUNCTION_PS_THREAD_STACK = 21,
    MYLITE_SYS_FUNCTION_PS_THREAD_TRX_INFO = 22,
    MYLITE_SYS_FUNCTION_NATIVE_FORMAT_BYTES = 23,
    MYLITE_SYS_FUNCTION_NATIVE_FORMAT_PICO_TIME = 24,
    MYLITE_SYS_FUNCTION_NATIVE_PS_CURRENT_THREAD_ID = 25,
    MYLITE_SYS_FUNCTION_NATIVE_PS_THREAD_ID = 26,
    MYLITE_SYS_FUNCTION_VALIDATE_PASSWORD_STRENGTH = 27,
    MYLITE_SYS_FUNCTION_ROLES_GRAPHML = 28,
};

struct mylite_sys_function_argument {
    const char *text;
    size_t text_size;
    bool is_null;
};

struct mylite_sys_function_result {
    char *text;
    size_t text_size;
    bool is_null;
};

bool mylite_sys_function_lookup(
    const char *schema,
    size_t schema_size,
    const char *name,
    size_t name_size,
    enum mylite_sys_function_kind *out_kind
);
bool mylite_sys_function_lookup_span(
    const struct mylite_sql_source_span *schema,
    const struct mylite_sql_source_span *name,
    enum mylite_sys_function_kind *out_kind
);
const char *mylite_sys_function_name(enum mylite_sys_function_kind kind);
const char *mylite_sys_function_sqlite_name(enum mylite_sys_function_kind kind);
size_t mylite_sys_function_argument_count(enum mylite_sys_function_kind kind);
int mylite_sys_function_evaluate(
    struct mylite_db *database,
    enum mylite_sys_function_kind kind,
    const struct mylite_sys_function_argument *arguments,
    size_t argument_count,
    struct mylite_sys_function_result *out_result
);
void mylite_sys_function_result_deinit(struct mylite_sys_function_result *result);
int mylite_sqlite_register_sys_functions(sqlite3 *sqlite);

#endif
