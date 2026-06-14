#if defined(__linux__) && !defined(_XOPEN_SOURCE)
#  define _XOPEN_SOURCE 700 /* NOLINT(bugprone-reserved-identifier): POSIX feature macro. */
#endif

#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_connection.h"
#include "mylite_convert_tz.h"
#include "mylite_date_format.h"
#include "mylite_date_interval_second.h"
#include "mylite_datediff.h"
#include "mylite_diagnostics.h"
#include "mylite_digest.h"
#include "mylite_dynamic_string.h"
#include "mylite_execution_catalog.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_execution_dml_numeric.h"
#include "mylite_execution_loaded_catalog.h"
#include "mylite_execution_plan_types.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_charset_collation.h"
#include "mylite_execution_scalar_numeric.h"
#include "mylite_execution_scalar_regexp.h"
#include "mylite_execution_scalar_string_position.h"
#include "mylite_execution_scalar_string_transform.h"
#include "mylite_execution_scalar_temporal_format.h"
#include "mylite_execution_system_variables.h"
#include "mylite_integer_arithmetic.h"
#include "mylite_json.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_mysql_server_identity.h"
#include "mylite_parser.h"
#include "mylite_period_functions.h"
#include "mylite_rand.h"
#include "mylite_regexp.h"
#include "mylite_result.h"
#include "mylite_sqlite_registration.h"
#include "mylite_statement_context.h"
#include "mylite_string_base64.h"
#include "mylite_string_bitmask.h"
#include "mylite_string_case.h"
#include "mylite_string_char.h"
#include "mylite_string_codepoint.h"
#include "mylite_string_concat.h"
#include "mylite_string_insert.h"
#include "mylite_string_padding.h"
#include "mylite_string_quote.h"
#include "mylite_string_replace.h"
#include "mylite_string_reverse.h"
#include "mylite_string_search.h"
#include "mylite_string_soundex.h"
#include "mylite_string_substring_index.h"
#include "mylite_string_trim.h"
#include "mylite_string_unhex.h"
#include "mylite_temporal_arithmetic.h"
#include "mylite_temporal_constructor.h"
#include "mylite_temporal_extract.h"
#include "mylite_timediff.h"
#include "mylite_timestamp_function.h"
#include "mylite_timestampdiff.h"
#include "mylite_unix_timestamp.h"
#include "mylite_uuid.h"
#include "mylite_weight_string.h"
#include "sqlite3.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#endif

#include "mylite_execution_declarations_00_constants.inc"
#include "mylite_execution_declarations_01_types.inc"
#include "mylite_execution_declarations_10_statement.inc"
#include "mylite_execution_declarations_20_statement_ddl.inc"
#include "mylite_execution_declarations_30_select_metadata.inc"
#include "mylite_execution_declarations_40_information_schema_show.inc"
#include "mylite_execution_declarations_50_ddl_planning.inc"
#include "mylite_execution_declarations_60_dml_query.inc"
#include "mylite_execution_declarations_70_predicate_dml.inc"
#include "mylite_execution_declarations_80_row_scalar.inc"
#include "mylite_execution_declarations_90_sql_builder.inc"
#include "mylite_execution_declarations_99_sqlite_binding.inc"

static int normalize_mysql_compat_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    struct normalized_mysql_compat_sql *out_sql
) {
    const char *current_sql = sql;
    size_t current_size = sql_size;
    char *owned_sql = NULL;
    char *next_sql = NULL;
    size_t next_size = 0U;
    bool changed = false;
    int rc = MYLITE_OK;

    if (out_sql == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = (struct normalized_mysql_compat_sql){.sql = sql, .sql_size = sql_size};

    rc = extract_mysql_executable_comment_sql(
        database,
        current_sql,
        current_size,
        &next_sql,
        &next_size,
        &changed
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (changed) {
        owned_sql = next_sql;
        current_sql = owned_sql;
        current_size = next_size;
    }

    rc = rewrite_set_user_variable_increment_sql(
        database,
        current_sql,
        current_size,
        &next_sql,
        &next_size,
        &changed
    );
    if (rc != MYLITE_OK) {
        free(owned_sql);
        return rc;
    }
    if (changed) {
        free(owned_sql);
        owned_sql = next_sql;
        current_sql = owned_sql;
        current_size = next_size;
    }

    rc = quote_set_bare_compat_values_sql(
        database,
        current_sql,
        current_size,
        &next_sql,
        &next_size,
        &changed
    );
    if (rc != MYLITE_OK) {
        free(owned_sql);
        return rc;
    }
    if (changed) {
        free(owned_sql);
        owned_sql = next_sql;
        current_sql = owned_sql;
        current_size = next_size;
    }

    out_sql->sql = current_sql;
    out_sql->sql_size = current_size;
    out_sql->owned_sql = owned_sql;
    return MYLITE_OK;
}

static void normalized_mysql_compat_sql_deinit(struct normalized_mysql_compat_sql *sql) {
    if (sql == NULL) {
        return;
    }
    free(sql->owned_sql);
    *sql = (struct normalized_mysql_compat_sql){0};
}

static int extract_mysql_executable_comment_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    char **out_sql,
    size_t *out_sql_size,
    bool *out_changed
) {
    size_t start = 0U;
    size_t end = 0U;
    size_t body_start = 0U;
    size_t body_end = 0U;
    size_t close = 0U;
    size_t tail = 0U;
    char *copy = NULL;

    if (out_sql == NULL || out_sql_size == NULL || out_changed == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    *out_sql_size = 0U;
    *out_changed = false;
    if (!trim_sql_span(sql, sql_size, &start, &end) ||
        end - start < mysql_executable_comment_min_length || sql[start] != '/' ||
        sql[start + 1U] != '*' || sql[start + 2U] != '!') {
        return MYLITE_OK;
    }

    close = start + 3U;
    while (close + 1U < end && !(sql[close] == '*' && sql[close + 1U] == '/')) {
        ++close;
    }
    if (close + 1U >= end) {
        return MYLITE_OK;
    }
    tail = close + 2U;
    while (tail < end && isspace((unsigned char)sql[tail])) {
        ++tail;
    }
    if (tail < end && sql[tail] == ';') {
        ++tail;
        while (tail < end && isspace((unsigned char)sql[tail])) {
            ++tail;
        }
    }
    if (tail != end) {
        return MYLITE_OK;
    }

    body_start = start + 3U;
    while (body_start < close && isdigit((unsigned char)sql[body_start])) {
        ++body_start;
    }
    while (body_start < close && isspace((unsigned char)sql[body_start])) {
        ++body_start;
    }
    body_end = close;
    while (body_end > body_start && isspace((unsigned char)sql[body_end - 1U])) {
        --body_end;
    }

    copy = (char *)malloc(body_end - body_start + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(copy, sql + body_start, body_end - body_start);
    copy[body_end - body_start] = '\0';
    *out_sql = copy;
    *out_sql_size = body_end - body_start;
    *out_changed = true;
    return MYLITE_OK;
}

static int rewrite_set_user_variable_increment_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    char **out_sql,
    size_t *out_sql_size,
    bool *out_changed
) {
    struct set_user_variable_increment_parts parts = {0};
    char name[MYLITE_SESSION_USER_VARIABLE_NAME_CAPACITY];
    char rhs_name[MYLITE_SESSION_USER_VARIABLE_NAME_CAPACITY];
    struct mylite_session_user_variable *variable = NULL;
    int64_t result_value = 0;
    int rc = MYLITE_OK;

    if (out_sql == NULL || out_sql_size == NULL || out_changed == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    *out_sql_size = 0U;
    *out_changed = false;
    if (!parse_set_user_variable_increment_sql(sql, sql_size, &parts)) {
        return MYLITE_OK;
    }

    copy_folded_sql_identifier(
        sql + parts.name_start,
        parts.name_end - parts.name_start,
        name,
        sizeof(name)
    );
    copy_folded_sql_identifier(
        sql + parts.rhs_name_start,
        parts.rhs_name_end - parts.rhs_name_start,
        rhs_name,
        sizeof(rhs_name)
    );
    if (strcmp(name, rhs_name) != 0) {
        return MYLITE_OK;
    }
    variable = find_session_user_variable(&database->session, name);
    if (variable == NULL || variable->is_null || variable->value == NULL) {
        return MYLITE_OK;
    }
    if (!set_user_variable_increment_result(sql, &parts, variable, &result_value)) {
        return MYLITE_OK;
    }

    rc = format_set_user_variable_increment_sql(
        database,
        sql,
        &parts,
        result_value,
        out_sql,
        out_sql_size
    );
    if (rc == MYLITE_OK) {
        *out_changed = true;
    }
    return rc;
}

static bool parse_set_user_variable_increment_sql(
    const char *sql,
    size_t sql_size,
    struct set_user_variable_increment_parts *out_parts
) {
    size_t start = 0U;
    size_t end = 0U;
    size_t index = 0U;

    if (out_parts == NULL) {
        return false;
    }
    *out_parts = (struct set_user_variable_increment_parts){0};
    if (!trim_sql_span(sql, sql_size, &start, &end) || end - start < mysql_set_keyword_length ||
        !sql_span_equals_ascii_case_insensitive(sql + start, mysql_set_keyword_length, "SET")) {
        return false;
    }

    index = skip_sql_spaces(sql, end, start + mysql_set_keyword_length);
    if (index >= end || sql[index] != '@') {
        return false;
    }
    out_parts->name_start = index + 1U;
    out_parts->name_end = scan_sql_identifier(sql, end, out_parts->name_start);
    if (out_parts->name_end == out_parts->name_start ||
        out_parts->name_end - out_parts->name_start >= MYLITE_SESSION_USER_VARIABLE_NAME_CAPACITY) {
        return false;
    }

    index = skip_sql_spaces(sql, end, out_parts->name_end);
    if (index >= end || sql[index] != '=') {
        return false;
    }
    index = skip_sql_spaces(sql, end, index + 1U);
    if (index >= end || sql[index] != '@') {
        return false;
    }
    out_parts->rhs_name_start = index + 1U;
    out_parts->rhs_name_end = scan_sql_identifier(sql, end, out_parts->rhs_name_start);

    index = skip_sql_spaces(sql, end, out_parts->rhs_name_end);
    if (index >= end || sql[index] != '+') {
        return false;
    }
    out_parts->delta_start = skip_sql_spaces(sql, end, index + 1U);
    index = out_parts->delta_start;
    while (index < end && isdigit((unsigned char)sql[index])) {
        ++index;
    }
    out_parts->delta_end = index;

    index = skip_sql_spaces(sql, end, index);
    if (index < end && sql[index] == ';') {
        index = skip_sql_spaces(sql, end, index + 1U);
    }
    return out_parts->delta_end != out_parts->delta_start && index == end;
}

static bool set_user_variable_increment_result(
    const char *sql,
    const struct set_user_variable_increment_parts *parts,
    const struct mylite_session_user_variable *variable,
    int64_t *out_value
) {
    intmax_t parsed_current_value = 0;
    int64_t current_value = 0;
    int64_t delta = 0;
    uint64_t delta_magnitude = 0U;
    char *endptr = NULL;

    if (parts == NULL || variable == NULL || out_value == NULL) {
        return false;
    }
    errno = 0;
    parsed_current_value = strtoimax(variable->value, &endptr, decimal_base);
    if (errno != 0 || endptr == variable->value || *endptr != '\0') {
        return false;
    }
    if (parsed_current_value < (intmax_t)INT64_MIN || parsed_current_value > (intmax_t)INT64_MAX) {
        return false;
    }
    current_value = (int64_t)parsed_current_value;
    for (size_t digit = parts->delta_start; digit < parts->delta_end; ++digit) {
        uint64_t next_digit = (uint64_t)(sql[digit] - '0');

        if (delta_magnitude > (UINT64_MAX - next_digit) / (uint64_t)decimal_base) {
            return false;
        }
        delta_magnitude = delta_magnitude * (uint64_t)decimal_base + next_digit;
    }
    if (delta_magnitude > (uint64_t)INT64_MAX) {
        return false;
    }
    delta = (int64_t)delta_magnitude;
    if ((delta > 0 && current_value > INT64_MAX - delta) ||
        (delta < 0 && current_value < INT64_MIN - delta)) {
        return false;
    }
    *out_value = current_value + delta;
    return true;
}

static int format_set_user_variable_increment_sql(
    struct mylite_db *database,
    const char *sql,
    const struct set_user_variable_increment_parts *parts,
    int64_t result_value,
    char **out_sql,
    size_t *out_sql_size
) {
    char result_text[set_user_variable_increment_sql_capacity];
    int written = 0;

    written = snprintf(
        result_text,
        sizeof(result_text),
        "SET @%.*s = %" PRId64,
        (int)(parts->name_end - parts->name_start),
        sql + parts->name_start,
        result_value
    );
    if (written < 0 || (size_t)written >= sizeof(result_text)) {
        set_runtime_error(database, "failed to rewrite SET user variable assignment");
        return MYLITE_ERROR;
    }
    *out_sql = (char *)malloc((size_t)written + 1U);
    if (*out_sql == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(*out_sql, result_text, (size_t)written + 1U);
    *out_sql_size = (size_t)written;
    return MYLITE_OK;
}

static int quote_set_bare_compat_values_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    char **out_sql,
    size_t *out_sql_size,
    bool *out_changed
) {
    struct mylite_dynamic_string rewritten;
    size_t start = 0U;
    size_t end = 0U;
    size_t cursor = 0U;
    size_t scan = 0U;
    bool changed = false;

    if (out_sql == NULL || out_sql_size == NULL || out_changed == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    *out_sql_size = 0U;
    *out_changed = false;
    if (!trim_sql_span(sql, sql_size, &start, &end) || end - start < 3U ||
        !sql_span_equals_ascii_case_insensitive(sql + start, 3U, "SET")) {
        return MYLITE_OK;
    }

    mylite_dynamic_string_init(&rewritten);
    scan = start + 3U;
    while (scan < sql_size) {
        char target_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        size_t equal_index = scan;
        size_t value_start = 0U;
        size_t value_end = 0U;

        while (equal_index < sql_size && sql[equal_index] != '=') {
            ++equal_index;
        }
        if (equal_index >= sql_size) {
            break;
        }
        if (!set_assignment_target_before_equal(
                sql,
                scan,
                equal_index,
                target_name,
                sizeof(target_name)
            ) ||
            !set_target_needs_bare_value_quoting(target_name)) {
            scan = equal_index + 1U;
            continue;
        }

        value_start = equal_index + 1U;
        while (value_start < sql_size && isspace((unsigned char)sql[value_start])) {
            ++value_start;
        }
        if (!set_assignment_value_word(
                sql,
                (struct set_assignment_value_word_request){
                    .sql_size = sql_size,
                    .value_start = value_start,
                },
                &value_end
            ) ||
            sql_span_equals_ascii_case_insensitive(
                sql + value_start,
                value_end - value_start,
                "DEFAULT"
            )) {
            scan = equal_index + 1U;
            continue;
        }

        if (mylite_dynamic_string_append_bytes(&rewritten, sql + cursor, value_start - cursor) !=
                MYLITE_OK ||
            mylite_dynamic_string_append_char(&rewritten, '\'') != MYLITE_OK ||
            mylite_dynamic_string_append_bytes(
                &rewritten,
                sql + value_start,
                value_end - value_start
            ) != MYLITE_OK ||
            mylite_dynamic_string_append_char(&rewritten, '\'') != MYLITE_OK) {
            mylite_dynamic_string_deinit(&rewritten);
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        cursor = value_end;
        scan = value_end;
        changed = true;
    }

    if (!changed) {
        mylite_dynamic_string_deinit(&rewritten);
        return MYLITE_OK;
    }
    if (mylite_dynamic_string_append_bytes(&rewritten, sql + cursor, sql_size - cursor) !=
        MYLITE_OK) {
        mylite_dynamic_string_deinit(&rewritten);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    *out_sql = mylite_dynamic_string_take(&rewritten);
    if (*out_sql == NULL) {
        mylite_dynamic_string_deinit(&rewritten);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    *out_sql_size = strlen(*out_sql);
    *out_changed = true;
    mylite_dynamic_string_deinit(&rewritten);
    return MYLITE_OK;
}

static bool trim_sql_span(const char *sql, size_t sql_size, size_t *out_start, size_t *out_end) {
    size_t start = 0U;
    size_t end = sql_size;

    if (sql == NULL || out_start == NULL || out_end == NULL) {
        return false;
    }
    while (start < end && isspace((unsigned char)sql[start])) {
        ++start;
    }
    while (end > start && isspace((unsigned char)sql[end - 1U])) {
        --end;
    }
    *out_start = start;
    *out_end = end;
    return start < end;
}

static bool sql_span_equals_ascii_case_insensitive(
    const char *text,
    size_t text_size,
    const char *expected
) {
    size_t expected_size = expected == NULL ? 0U : strlen(expected);

    if (text == NULL || expected == NULL || text_size != expected_size) {
        return false;
    }
    for (size_t index = 0U; index < text_size; ++index) {
        if (ascii_lower((unsigned char)text[index]) !=
            ascii_lower((unsigned char)expected[index])) {
            return false;
        }
    }
    return true;
}

static bool sql_byte_is_identifier(char byte) {
    return isalnum((unsigned char)byte) || byte == '_' || byte == '$';
}

static bool sql_byte_starts_identifier(char byte) {
    return isalpha((unsigned char)byte) || byte == '_' || byte == '$';
}

static void copy_folded_sql_identifier(
    const char *text,
    size_t text_size,
    char *buffer,
    size_t buffer_size
) {
    size_t copied = 0U;

    if (buffer == NULL || buffer_size == 0U) {
        return;
    }
    copied = text_size < buffer_size ? text_size : buffer_size - 1U;
    for (size_t index = 0U; index < copied; ++index) {
        buffer[index] = ascii_lower((unsigned char)text[index]);
    }
    buffer[copied] = '\0';
}

static bool set_target_needs_bare_value_quoting(const char *target_name) {
    static const char *const targets[] = {
        "character_set_client",
        "character_set_connection",
        "character_set_results",
        "collation_connection",
        "default_collation_for_utf8mb4",
        "default_storage_engine",
        "default_tmp_storage_engine",
        "resultset_metadata",
        "session_track_gtids",
        "session_track_transaction_info",
        "use_secondary_engine",
    };

    if (target_name == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(targets) / sizeof(targets[0]); ++index) {
        if (strcmp(target_name, targets[index]) == 0) {
            return true;
        }
    }
    return false;
}

static bool set_assignment_target_before_equal(
    const char *sql,
    size_t assignment_start,
    size_t equal_index,
    char *buffer,
    size_t buffer_size
) {
    size_t name_end = equal_index;
    size_t name_start = 0U;

    if (sql == NULL || buffer == NULL || buffer_size == 0U || assignment_start >= equal_index) {
        return false;
    }
    buffer[0] = '\0';
    while (name_end > assignment_start && isspace((unsigned char)sql[name_end - 1U])) {
        --name_end;
    }
    name_start = name_end;
    while (name_start > assignment_start && sql_byte_is_identifier(sql[name_start - 1U])) {
        --name_start;
    }
    if (name_start == name_end || name_end - name_start >= buffer_size) {
        return false;
    }
    copy_folded_sql_identifier(sql + name_start, name_end - name_start, buffer, buffer_size);
    return true;
}

static bool set_assignment_value_word(
    const char *sql,
    struct set_assignment_value_word_request request,
    size_t *out_value_end
) {
    size_t index = request.value_start;

    if (sql == NULL || out_value_end == NULL || index >= request.sql_size ||
        !sql_byte_starts_identifier(sql[index])) {
        return false;
    }
    ++index;
    while (index < request.sql_size && sql_byte_is_identifier(sql[index])) {
        ++index;
    }
    *out_value_end = index;
    return true;
}

static size_t skip_sql_spaces(const char *sql, size_t end, size_t index) {
    while (index < end && isspace((unsigned char)sql[index])) {
        ++index;
    }
    return index;
}

static size_t scan_sql_identifier(const char *sql, size_t end, size_t index) {
    while (index < end && sql_byte_is_identifier(sql[index])) {
        ++index;
    }
    return index;
}

int mylite_execute(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_result **out_result
) {
    struct mylite_statement_context context;
    struct mylite_sql_parse_result parse_result;
    struct normalized_mysql_compat_sql normalized_sql;
    const struct mylite_sql_ast_node *statement = NULL;
    int64_t completed_row_count = -1;
    size_t statement_count = 0U;
    bool preserve_diagnostics_snapshot = false;
    bool completed_statement_is_select = false;
    int rc = MYLITE_OK;

    if (out_result == NULL) {
        if (database != NULL) {
            mylite_diagnostics_set_error(
                mylite_connection_diagnostics(database),
                MYLITE_MISUSE,
                "HY000",
                mylite_diagnostics_misuse_message()
            );
        }
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    if (database == NULL || sql == NULL) {
        if (database != NULL) {
            mylite_diagnostics_set_error(
                mylite_connection_diagnostics(database),
                MYLITE_MISUSE,
                "HY000",
                mylite_diagnostics_misuse_message()
            );
        }
        return MYLITE_MISUSE;
    }

    normalized_sql = (struct normalized_mysql_compat_sql){0};
    rc = normalize_mysql_compat_sql(database, sql, sql_size, &normalized_sql);
    if (rc != MYLITE_OK) {
        return rc;
    }

    mylite_statement_context_init(&context);
    rc = mylite_statement_context_begin(
        &context,
        database,
        normalized_sql.sql,
        normalized_sql.sql_size
    );
    if (rc != MYLITE_OK) {
        normalized_mysql_compat_sql_deinit(&normalized_sql);
        mylite_statement_context_deinit(&context);
        return rc;
    }
    mylite_statement_context_set_previous_row_count(&context, database->session.previous_row_count);
    mylite_statement_context_set_previous_found_rows(&context, database->session.found_rows);

    rc = status_from_parse_status(mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = normalized_sql.sql,
            .length = normalized_sql.sql_size,
            .modes = lexer_modes_for_session_sql_mode(&database->session),
        },
        &parse_result
    ));
    if (rc != MYLITE_OK) {
        rc = finish_parse_failure(database, &parse_result, rc);
        mylite_sql_parse_result_deinit(&parse_result);
        (void)mylite_statement_context_end(&context, rc);
        mylite_statement_context_deinit(&context);
        normalized_mysql_compat_sql_deinit(&normalized_sql);
        return rc;
    }

    rc = script_statement_count(parse_result.root, &statement_count);
    if (rc == MYLITE_OK && statement_count == 0U) {
        rc = execute_empty_statement(database, out_result);
    } else if (rc == MYLITE_OK && statement_count == 1U) {
        statement = child_at(parse_result.root, 0U);
        rc = execute_parsed_statement(database, &context, statement, out_result);
    } else if (rc == MYLITE_OK) {
        set_multi_statement_parse_error(database, parse_result.root);
        rc = MYLITE_ERROR;
    }

    if (rc == MYLITE_OK) {
        completed_row_count = row_count_for_completed_statement(statement, *out_result);
        preserve_diagnostics_snapshot = statement_preserves_diagnostics_snapshot(statement);
        completed_statement_is_select = statement_result_is_select(statement, *out_result);
    }
    mylite_sql_parse_result_deinit(&parse_result);
    if (rc != MYLITE_OK) {
        rc = finish_failed_statement(database, rc, out_result);
    } else {
        rc = finish_completed_statement(
            database,
            completed_statement_is_select,
            completed_row_count,
            preserve_diagnostics_snapshot,
            out_result
        );
    }
    (void)mylite_statement_context_end(&context, rc);
    mylite_statement_context_deinit(&context);
    normalized_mysql_compat_sql_deinit(&normalized_sql);

    return rc;
}

static int finish_parse_failure(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result,
    int parse_rc
) {
    int rc = parse_rc;
    int snapshot_rc = MYLITE_OK;

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else {
        set_parse_error(database, parse_result);
    }
    database->session.previous_row_count = -1;

    snapshot_rc = snapshot_current_diagnostics(database);
    return snapshot_rc == MYLITE_OK ? rc : snapshot_rc;
}

const struct mylite_sql_ast_node *mylite_execution_child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
) {
    return child_at(node, index);
}

const struct mylite_sql_ast_node *mylite_execution_unwrap_parenthesized_expression(
    const struct mylite_sql_ast_node *expression
) {
    return unwrap_parenthesized_expression(expression);
}

int mylite_execution_parse_unsigned_integer_literal(
    const struct mylite_sql_source_span *span,
    uint64_t *out_value
) {
    return parse_unsigned_integer_literal(span, out_value);
}

bool mylite_execution_is_scalar_arithmetic_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    return is_scalar_arithmetic_projection_expression(expression);
}

bool mylite_execution_is_scalar_bitwise_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    return is_scalar_bitwise_projection_expression(expression);
}

int mylite_execution_evaluate_scalar_arithmetic_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
) {
    return evaluate_scalar_arithmetic_expression(database, expression, out_value);
}

int mylite_execution_evaluate_scalar_bitwise_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
) {
    return evaluate_scalar_bitwise_expression(database, expression, out_value);
}

int mylite_execution_evaluate_bit_count_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
) {
    return evaluate_bit_count_operand(database, expression, out_value);
}

int mylite_execution_scalar_hex_numeric_runtime_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value,
    bool *out_handled
) {
    enum mylite_execution_system_variable_kind variable = MYLITE_EXECUTION_SYSTEM_VARIABLE_NONE;
    int64_t timestamp = 0;
    int rc = MYLITE_OK;

    if (out_value == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_bitwise_value){.is_null = false, .integer = 0U};
    *out_handled = true;
    if (expression == NULL) {
        *out_handled = false;
        return MYLITE_OK;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
        out_value->integer = database->session.connection_id;
        return MYLITE_OK;
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
        out_value->integer = (uint64_t)database->session.previous_row_count;
        return MYLITE_OK;
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION:
        out_value->integer = database->session.found_rows;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
        out_value->integer = database->session.last_insert_id;
        return MYLITE_OK;
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        rc = resolve_session_system_variable(database, expression, &variable);
        if (rc != MYLITE_OK) {
            return rc;
        }
        break;
    default:
        *out_handled = false;
        return MYLITE_OK;
    }

    switch (variable) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET:
        out_value->integer = auto_increment_step_system_variable_value(
            database,
            variable,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT:
        out_value->integer = timeout_system_variable_value(
            database,
            variable,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN:
        if (system_variable_expression_has_global_scope(expression)) {
            out_value->integer = group_concat_max_len_default_value;
        } else {
            out_value->integer = database->session.group_concat_max_len;
        }
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INFORMATION_SCHEMA_STATS_EXPIRY:
        if (system_variable_expression_has_global_scope(expression)) {
            out_value->integer = information_schema_stats_expiry_default_value;
        } else {
            out_value->integer = database->session.information_schema_stats_expiry;
        }
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS:
        out_value->integer = foreign_key_checks_system_variable_uint64_value(
            database,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BIG_TABLES:
        out_value->integer = big_tables_system_variable_uint64_value(
            database,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOCOMMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_QUOTE_SHOW_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UNIQUE_CHECKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_NOTES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BIG_SELECTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLICIT_DEFAULTS_FOR_TIMESTAMP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN:
        out_value->integer = 1U;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_WARNINGS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_GENERATE_INVISIBLE_PRIMARY_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_OFF:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REPLICA_SKIP_COUNTER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER:
        out_value->integer = 0U;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID:
        out_value->integer = server_id_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID_BITS:
        out_value->integer = server_id_bits_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PORT:
        out_value->integer = port_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_VERSION:
        out_value->integer = protocol_version_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT:
        out_value->integer = sql_select_limit_system_variable_value(
            database,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TIMESTAMP:
        timestamp = current_timestamp_epoch(database);
        out_value->integer = (uint64_t)timestamp;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WARNING_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ERROR_COUNT:
        return diagnostics_count_system_variable_value(
            system_variable_count_diagnostics(database),
            variable,
            &out_value->integer
        );
    default:
        break;
    }

    *out_handled = false;
    return MYLITE_OK;
}

int mylite_execution_accumulate_staged_division_by_zero_warnings(
    struct mylite_db *database,
    size_t staged_count,
    size_t *inout_warning_count
) {
    return accumulate_staged_division_by_zero_warnings(database, staged_count, inout_warning_count);
}

int mylite_execution_accumulate_staged_warning_count(
    struct mylite_db *database,
    size_t staged_count,
    size_t *inout_warning_count
) {
    return accumulate_staged_warning_count(database, staged_count, inout_warning_count);
}

int mylite_execution_append_division_by_zero_warnings(
    struct mylite_db *database,
    size_t warning_count
) {
    return append_division_by_zero_warnings(database, warning_count);
}

int mylite_execution_decode_sql_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    char **out_text,
    size_t *out_text_length
) {
    return decode_sql_string_literal(
        database,
        literal_node,
        unsupported_message,
        nul_message,
        out_text,
        out_text_length
    );
}

int mylite_execution_current_timestamp_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return current_timestamp_scalar_value(database, out_cell);
}

int mylite_execution_validate_temporal_fractional_precision(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_execution_temporal_fractional_precision_context context
) {
    enum { temporal_fractional_precision_max = 6 };
    struct mylite_sql_source_span precision_span = {0};
    uint64_t precision = 0U;

    if (expression == NULL || !mylite_sql_ast_node_has_temporal_fractional_precision(expression)) {
        return MYLITE_OK;
    }

    precision_span = mylite_sql_ast_node_temporal_fractional_precision_span(expression);
    if (parse_unsigned_integer_literal(&precision_span, &precision) != MYLITE_OK) {
        set_unsupported_error(
            database,
            "temporal fractional precision supports only integer values"
        );
        return MYLITE_ERROR;
    }
    if (precision > temporal_fractional_precision_max) {
        set_temporal_precision_too_big_error(
            database,
            context.subject_name == NULL ? "temporal" : context.subject_name,
            precision
        );
        return MYLITE_ERROR;
    }
    if (precision != 0U) {
        set_unsupported_error(
            database,
            context.unsupported_message == NULL ? "fractional temporal precision is not supported"
                                                : context.unsupported_message
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_supported_temporal_fractional_precision(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *subject_name,
    uint64_t *out_precision
) {
    enum { temporal_fractional_precision_max = 6 };
    struct mylite_sql_source_span precision_span = {0};
    uint64_t precision = 0U;

    if (out_precision == NULL) {
        return MYLITE_MISUSE;
    }
    *out_precision = 0U;
    if (!mylite_sql_ast_node_has_temporal_fractional_precision(expression)) {
        return MYLITE_OK;
    }

    precision_span = mylite_sql_ast_node_temporal_fractional_precision_span(expression);
    if (parse_unsigned_integer_literal(&precision_span, &precision) != MYLITE_OK) {
        set_unsupported_error(
            database,
            "temporal fractional precision supports only integer values"
        );
        return MYLITE_ERROR;
    }
    if (precision > temporal_fractional_precision_max) {
        set_temporal_precision_too_big_error(
            database,
            subject_name == NULL ? "temporal" : subject_name,
            precision
        );
        return MYLITE_ERROR;
    }
    *out_precision = precision;

    return MYLITE_OK;
}

int mylite_execution_sysdate_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return sysdate_scalar_value(database, expression, out_cell);
}

int mylite_execution_current_date_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return current_date_scalar_value(database, out_cell);
}

int mylite_execution_current_time_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return current_time_scalar_value(database, out_cell);
}

int mylite_execution_utc_date_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return utc_date_scalar_value(database, out_cell);
}

int mylite_execution_utc_time_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return utc_time_scalar_value(database, out_cell);
}

int mylite_execution_utc_timestamp_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    return utc_timestamp_scalar_value(database, out_cell);
}

int mylite_execution_system_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return system_variable_value(database, expression, out_cell);
}

int mylite_execution_decode_sql_string_literal_with_policy(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    bool allow_nul,
    char **out_text,
    size_t *out_text_length
) {
    return decode_sql_string_literal_with_policy(
        database,
        literal_node,
        unsupported_message,
        nul_message,
        allow_nul,
        out_text,
        out_text_length
    );
}

int mylite_execution_decode_binary_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_bytes,
    size_t *out_byte_count
) {
    return decode_binary_hex_literal(database, literal_node, out_bytes, out_byte_count);
}

int mylite_execution_copy_source_span_text(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_text
) {
    return copy_source_span_text(database, span, out_text);
}

int mylite_execution_copy_identifier_text(
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size,
    struct mylite_db *database
) {
    return copy_identifier_text(node, destination, destination_size, database);
}

int mylite_execution_normalize_decimal_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    bool is_negative,
    char *buffer,
    size_t buffer_size
) {
    return normalize_decimal_integer_literal(database, span, is_negative, buffer, buffer_size);
}

int mylite_execution_format_uint64(
    struct mylite_db *database,
    uint64_t value,
    char *buffer,
    size_t buffer_size
) {
    return format_uint64(database, value, buffer, buffer_size);
}

int mylite_execution_duplicate_text(
    struct mylite_db *database,
    const char *source,
    char **out_text
) {
    return duplicate_text(database, source, out_text);
}

int mylite_execution_cast_binary_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return cast_binary_value(database, expression, out_cell);
}

int mylite_execution_convert_binary_type_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return convert_binary_type_value(database, expression, out_cell);
}

int mylite_execution_convert_using_binary_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return convert_using_binary_value(database, expression, out_cell);
}

int mylite_execution_convert_using_charset_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return convert_using_charset_value(database, expression, out_cell);
}

int mylite_execution_collate_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return collate_expression_value(database, expression, out_cell);
}

int mylite_execution_scalar_convert_charset_info_for_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_convert_charset_info *out_info
) {
    return scalar_convert_charset_info_for_expression(database, expression, out_info);
}

int mylite_execution_rand_seed_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    uint32_t *out_seed
) {
    return rand_seed_value(database, expression, out_seed);
}

int64_t mylite_execution_current_timestamp_epoch(const struct mylite_db *database) {
    return current_timestamp_epoch(database);
}

int mylite_execution_date_add_set_unknown_identifier_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_source_span *span = expression == NULL ? NULL : &expression->span;
    const char *source = NULL;
    size_t source_size = 0U;
    size_t destination_index = 0U;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

    if (span == NULL || span->text == NULL || span->length == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    source = span->text;
    source_size = span->length;
    if (source[0] != '`' && source[0] != '"') {
        if (source_size >= sizeof(column_name)) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        memcpy(column_name, source, source_size);
        column_name[source_size] = '\0';
        set_unknown_column_error(database, column_name);
        return MYLITE_ERROR;
    }
    if (source_size < 2U || source[source_size - 1U] != source[0]) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    for (size_t source_index = 1U; source_index + 1U < source_size; ++source_index) {
        if (destination_index + 1U >= sizeof(column_name)) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        if (source[source_index] == source[0] && source[source_index + 1U] == source[0]) {
            column_name[destination_index] = source[0];
            ++source_index;
        } else {
            column_name[destination_index] = source[source_index];
        }
        ++destination_index;
    }
    if (destination_index == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    column_name[destination_index] = '\0';
    set_unknown_column_error(database, column_name);
    return MYLITE_ERROR;
}

size_t mylite_execution_temporal_constructor_function_argument_count(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return temporal_constructor_function_argument_count(ast_kind);
}

const char *mylite_execution_temporal_constructor_function_name(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return temporal_constructor_function_name(ast_kind);
}

bool mylite_execution_is_temporal_constructor_function_kind(enum mylite_sql_ast_node_kind ast_kind
) {
    return is_temporal_constructor_function_kind(ast_kind);
}

int mylite_execution_copy_identifier_name_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_node,
    char *destination,
    size_t destination_size,
    const char *identifier_kind,
    const char *nul_message
) {
    return copy_table_option_name_text(
        database,
        name_node,
        destination,
        destination_size,
        (struct table_option_name_policy){
            .identifier_kind = identifier_kind,
            .nul_message = nul_message,
        }
    );
}

const char *mylite_execution_national_character_set_name(void) {
    return national_character_set_name;
}

const char *mylite_execution_national_collation_name(void) {
    return national_collation_name;
}

void mylite_execution_set_parse_error(struct mylite_db *database) {
    set_parse_error(database, NULL);
}

void mylite_execution_set_unsupported_error(struct mylite_db *database, const char *message) {
    set_unsupported_error(database, message);
}

void mylite_execution_set_native_function_parameter_count_error(
    struct mylite_db *database,
    const char *function_name
) {
    set_native_function_parameter_count_error(database, function_name);
}

void mylite_execution_set_scalar_division_unsupported_error(struct mylite_db *database) {
    set_scalar_division_unsupported_error(database);
}

void mylite_execution_set_abs_signed_minimum_overflow_error(struct mylite_db *database) {
    set_abs_signed_minimum_overflow_error(database);
}

void mylite_execution_set_abs_unsupported_error(struct mylite_db *database) {
    set_abs_unsupported_error(database);
}

void mylite_execution_set_sign_unsupported_error(struct mylite_db *database) {
    set_sign_unsupported_error(database);
}

void mylite_execution_set_rounding_unsupported_error(struct mylite_db *database) {
    set_rounding_unsupported_error(database);
}

int mylite_execution_set_rounding_signed_overflow_error(struct mylite_db *database) {
    return set_rounding_signed_overflow_error(database);
}

void mylite_execution_set_sqrt_unsupported_error(struct mylite_db *database) {
    set_sqrt_unsupported_error(database);
}

void mylite_execution_set_angle_conversion_unsupported_error(struct mylite_db *database) {
    set_angle_conversion_unsupported_error(database);
}

void mylite_execution_set_inverse_trig_unsupported_error(struct mylite_db *database) {
    set_inverse_trig_unsupported_error(database);
}

void mylite_execution_set_direct_trig_unsupported_error(struct mylite_db *database) {
    set_direct_trig_unsupported_error(database);
}

void mylite_execution_set_atan_unsupported_error(struct mylite_db *database) {
    set_atan_unsupported_error(database);
}

void mylite_execution_set_exp_log_power_unsupported_error(struct mylite_db *database) {
    set_exp_log_power_unsupported_error(database);
}

void mylite_execution_set_format_unsupported_error(struct mylite_db *database) {
    set_format_unsupported_error(database);
}

void mylite_execution_set_truncate_unsupported_error(struct mylite_db *database) {
    set_truncate_unsupported_error(database);
}

void mylite_execution_set_base_conversion_unsupported_error(struct mylite_db *database) {
    set_base_conversion_unsupported_error(database);
}

void mylite_execution_set_bit_count_unsupported_error(struct mylite_db *database) {
    set_bit_count_unsupported_error(database);
}

void mylite_execution_set_crc32_unsupported_error(struct mylite_db *database) {
    set_crc32_unsupported_error(database);
}

void mylite_execution_set_hex_unsupported_error(struct mylite_db *database) {
    set_hex_unsupported_error(database);
}

void mylite_execution_set_invalid_json_function_text_error(
    struct mylite_db *database,
    size_t position
) {
    set_invalid_json_function_text_error(database, position);
}

int mylite_execution_append_invalid_json_value_warning(
    struct mylite_db *database,
    const struct mylite_json_normalize_result *result
) {
    return append_invalid_json_value_warning(database, result);
}

void mylite_execution_set_invalid_json_path_error(struct mylite_db *database, size_t position) {
    set_invalid_json_path_error(database, position);
}

void mylite_execution_set_json_path_not_allowed_error(struct mylite_db *database) {
    set_json_path_not_allowed_error(database);
}

void mylite_execution_set_invalid_json_data_type_error(
    struct mylite_db *database,
    const char *function_name
) {
    set_invalid_json_data_type_error(database, function_name);
}

void mylite_execution_set_invalid_json_one_or_all_error(struct mylite_db *database) {
    set_invalid_json_one_or_all_error(database);
}

void mylite_execution_set_json_unquote_incorrect_type_error(struct mylite_db *database) {
    set_json_unquote_incorrect_type_error(database);
}

void mylite_execution_set_json_quote_incorrect_type_error(struct mylite_db *database) {
    set_json_quote_incorrect_type_error(database);
}

void mylite_execution_set_json_binary_charset_error(struct mylite_db *database) {
    set_json_binary_charset_error(database);
}

void mylite_execution_set_json_null_member_name_error(struct mylite_db *database) {
    set_json_null_member_name_error(database);
}

bool mylite_execution_text_equals_ascii_case_insensitive(const char *left, const char *right) {
    return text_equals_ascii_case_insensitive(left, right);
}

bool mylite_execution_text_value_is_supported_string_key(const char *text, size_t text_length) {
    return text_value_is_supported_string_key(text, text_length);
}

const char *mylite_execution_scalar_pi_text(void) {
    return scalar_pi_text;
}

int mylite_execution_scalar_rand_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return rand_function_value(database, expression, out_cell);
}

int mylite_execution_literal_projection_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return literal_projection_value(database, expression, out_cell);
}

int mylite_execution_format_session_scalar_uint64_value(
    struct mylite_db *database,
    uint64_t value,
    struct session_scalar_cell *out_cell
) {
    return format_session_scalar_uint64_value(database, value, out_cell);
}

int mylite_execution_validate_utf8_text(
    const char *text,
    size_t text_length,
    size_t *out_character_count
) {
    return validate_utf8_text(text, text_length, out_character_count);
}

int mylite_execution_utf8_sequence_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    return utf8_sequence_width(text, text_length, index, out_width);
}

bool mylite_execution_is_session_scalar_expression(const struct mylite_sql_ast_node *expression) {
    return is_session_scalar_expression(expression);
}

int mylite_execution_session_user_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct session_scalar_cell *out_cell
) {
    return session_user_variable_value(database, node, out_cell);
}

int mylite_execution_set_unknown_column_reference_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = collect_column_reference_parts(database, expression, parts, &part_count);

    if (rc == MYLITE_OK) {
        rc = format_column_reference_name(
            database,
            parts,
            part_count,
            column_name,
            sizeof(column_name)
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    set_unknown_column_error(database, column_name);
    return MYLITE_ERROR;
}

void mylite_execution_set_illegal_mix_of_collations_error(
    struct mylite_db *database,
    const char *first_collation,
    const char *second_collation,
    const char *operation
) {
    set_illegal_mix_of_collations_error(database, first_collation, second_collation, operation);
}

void mylite_execution_set_unknown_collation_error(
    struct mylite_db *database,
    const char *collation_name
) {
    set_unknown_collation_error(database, collation_name);
}

void mylite_execution_set_collation_not_valid_for_charset_error(
    struct mylite_db *database,
    const char *collation_name,
    const char *charset_name
) {
    set_collation_not_valid_for_charset_error(database, collation_name, charset_name);
}

void mylite_execution_set_nomem_error(struct mylite_db *database) {
    set_nomem_error(database);
}

void mylite_execution_set_runtime_error(struct mylite_db *database, const char *message) {
    set_runtime_error(database, message);
}

void mylite_execution_set_regexp_illegal_argument_error(struct mylite_db *database) {
    set_regexp_illegal_argument_error(database);
}

void mylite_execution_set_regexp_error(struct mylite_db *database, const char *message) {
    set_regexp_error(database, message);
}

void mylite_execution_set_regexp_character_range_error(
    struct mylite_db *database,
    const char *message
) {
    set_regexp_character_range_error(database, message);
}

void mylite_execution_session_scalar_cell_deinit(struct session_scalar_cell *cell) {
    session_scalar_cell_deinit(cell);
}

#include "mylite_execution_statement_entry.inc"

#include "mylite_execution_explain_statement.inc"

#include "mylite_execution_statement_session_handlers.inc"

#include "mylite_execution_prepared_statement_execution.inc"

#include "mylite_execution_transaction_characteristics.inc"

#include "mylite_execution_statement_transaction_boundaries.inc"

#include "mylite_execution_transaction_statements.inc"

#include "mylite_execution_lock_tables.inc"

#include "mylite_execution_statement_implicit_commits.inc"

#include "mylite_execution_session_savepoints.inc"

#include "mylite_execution_statement_sqlite_transactions.inc"

#include "mylite_execution_set_connection_charset.inc"

#include "mylite_execution_set_assignments.inc"

#include "mylite_execution_admin_placeholders.inc"

#include "mylite_execution_prepared_statement_support.inc"

#include "mylite_execution_stored_procedures.inc"

#include "mylite_execution_set_session_snapshot.inc"

#include "mylite_execution_set_system_variable_dispatch.inc"

#include "mylite_execution_set_boolean_variables.inc"

#include "mylite_execution_set_numeric_transaction_variables.inc"

#include "mylite_execution_set_limit_size_expiry_variables.inc"

#include "mylite_execution_set_timeout_variables.inc"

#include "mylite_execution_set_sql_mode_timestamp_time_zone.inc"

#include "mylite_execution_ddl_create_table_statements.inc"

#include "mylite_execution_ddl_create_view_statements.inc"

#include "mylite_execution_ddl_create_schema_index_statements.inc"

#include "mylite_execution_ddl_drop_existence_statements.inc"

#include "mylite_execution_ddl_table_action_statements.inc"

#include "mylite_execution_ddl_alter_table_index_constraint_statements.inc"

#include "mylite_execution_ddl_alter_table_column_statements.inc"

#include "mylite_execution_ddl_alter_table_schema_option_statements.inc"

#include "mylite_execution_ddl_alter_table_maintenance_statements.inc"

#include "mylite_execution_dml_statements.inc"

#include "mylite_execution_metadata_queries.inc"

#include "mylite_execution_select_into_user_variables.inc"

#include "mylite_execution_information_schema_join_compat.inc"

#include "mylite_execution_mysql_system_query_dispatch.inc"

#include "mylite_execution_mysql_system_sys_auto_increment_rows.inc"

#include "mylite_execution_mysql_system_sys_statistics_rows.inc"

#include "mylite_execution_mysql_system_sys_table_index_health_rows.inc"

#include "mylite_execution_mysql_system_sys_object_overview_rows.inc"

#include "mylite_execution_mysql_system_innodb_stats_rows.inc"

#include "mylite_execution_information_schema_query_execution.inc"

#include "mylite_execution_information_schema_system_dispatch_rows.inc"

#include "mylite_execution_information_schema_catalog_dispatch_rows.inc"

#include "mylite_execution_information_schema_row_helpers.inc"

#include "mylite_execution_information_schema_static_core_rows.inc"

#include "mylite_execution_information_schema_static_storage_rows.inc"

#include "mylite_execution_information_schema_builtin_table_status_helpers.inc"

#include "mylite_execution_information_schema_base_table_status_rows.inc"

#include "mylite_execution_information_schema_columns_system_rows.inc"

#include "mylite_execution_information_schema_columns_base_rows.inc"

#include "mylite_execution_information_schema_innodb_virtual_rows.inc"

#include "mylite_execution_information_schema_innodb_column_rows.inc"

#include "mylite_execution_information_schema_innodb_table_rows.inc"

#include "mylite_execution_information_schema_innodb_index_rows.inc"

#include "mylite_execution_information_schema_innodb_foreign_rows.inc"

#include "mylite_execution_information_schema_constraint_rows.inc"

#include "mylite_execution_information_schema_key_constraint_rows.inc"

#include "mylite_execution_information_schema_statistics_rows.inc"

#include "mylite_execution_information_schema_result_rows.inc"

#include "mylite_execution_information_schema_predicate_validation.inc"

#include "mylite_execution_information_schema_predicate_evaluation.inc"

#include "mylite_execution_information_schema_predicate_comparison.inc"

#include "mylite_execution_information_schema_predicate_values.inc"

#include "mylite_execution_information_schema_query_planning.inc"

#include "mylite_execution_information_schema_compare_format_helpers.inc"

#include "mylite_execution_information_schema_descriptor_metadata.inc"

#include "mylite_execution_table_maintenance_queries.inc"

#include "mylite_execution_show_tables_status_general.inc"

#include "mylite_execution_show_charset_variables_status.inc"

#include "mylite_execution_show_variables_where_eval.inc"

#include "mylite_execution_show_schema_objects_processlist_privileges.inc"

#include "mylite_execution_show_replication_metadata.inc"

#include "mylite_execution_show_diagnostics_output.inc"

#include "mylite_execution_show_columns_indexes.inc"

#include "mylite_execution_show_create.inc"

#include "mylite_execution_result_completion.inc"

#include "mylite_execution_create_table_planning_core.inc"

#include "mylite_execution_create_table_column_default_charset.inc"

#include "mylite_execution_create_table_item_validation.inc"

#include "mylite_execution_primary_key_definition_planning.inc"

#include "mylite_execution_create_table_secondary_index_planning.inc"

#include "mylite_execution_create_table_foreign_key_planning.inc"

#include "mylite_execution_create_table_check_constraint_planning.inc"

#include "mylite_execution_create_table_generated_expression_rendering.inc"

#include "mylite_execution_check_expression_rendering.inc"

#include "mylite_execution_create_table_constraints.inc"

#include "mylite_execution_create_table_variants.inc"

#include "mylite_execution_table_options_planning.inc"

#include "mylite_execution_create_table_execution.inc"

#include "mylite_execution_schema_table_admin.inc"

#include "mylite_execution_alter_table_add_column.inc"

#include "mylite_execution_alter_table_add_index.inc"

#include "mylite_execution_alter_table_foreign_key_index.inc"

#include "mylite_execution_alter_table_check_constraints.inc"

#include "mylite_execution_alter_table_drop_rename_column.inc"

#include "mylite_execution_alter_table_modify_column_entry.inc"

#include "mylite_execution_alter_table_default_visibility_options.inc"

#include "mylite_execution_alter_table_charset_conversion_options.inc"

#include "mylite_execution_alter_table_table_option_actions.inc"

#include "mylite_execution_alter_table_modify_column_resolution.inc"

#include "mylite_execution_alter_table_modify_column_execution.inc"

#include "mylite_execution_alter_table_check_rebuild_sql.inc"

#include "mylite_execution_alter_table_rename_check_constraints.inc"

#include "mylite_execution_load_data_planning.inc"

#include "mylite_execution_dml_planning.inc"

#include "mylite_execution_insert_execution.inc"

#include "mylite_execution_insert_select_planning.inc"

#include "mylite_execution_insert_select_table_execution.inc"

#include "mylite_execution_insert_select_row_scalar_execution.inc"

#include "mylite_execution_insert_select_table_rows.inc"

#include "mylite_execution_insert_select_value_materialization.inc"

#include "mylite_execution_insert_select_validation_core.inc"

#include "mylite_execution_insert_select_string_validation.inc"

#include "mylite_execution_insert_select_type_validation.inc"

#include "mylite_execution_update_planning.inc"

#include "mylite_execution_update_execution.inc"

#include "mylite_execution_select_planning_core.inc"

#include "mylite_execution_grouped_aggregate_entry.inc"

#include "mylite_execution_grouped_aggregate_source_planning.inc"

#include "mylite_execution_grouped_aggregate_group_columns.inc"

#include "mylite_execution_grouped_aggregate_projection_columns.inc"

#include "mylite_execution_grouped_aggregate_function_planning.inc"

#include "mylite_execution_grouped_aggregate_having_planning.inc"

#include "mylite_execution_grouped_aggregate_literal_conversion.inc"

#include "mylite_execution_grouped_aggregate_order_planning.inc"

#include "mylite_execution_select_execution.inc"

#include "mylite_execution_aggregate_execution.inc"

#include "mylite_execution_scalar_projection_classification.inc"

#include "mylite_execution_values_statement.inc"

#include "mylite_execution_scalar_projection_select_execution.inc"

#include "mylite_execution_scalar_result_metadata.inc"

#include "mylite_execution_session_scalar_result_helpers.inc"

#include "mylite_execution_session_scalar_warnings.inc"

#include "mylite_execution_scalar_projection_argument_diagnostics.inc"

#include "mylite_execution_scalar.inc"

#include "mylite_execution_scalar_string_core.inc"

#include "mylite_execution_scalar_temporal_core.inc"

#include "mylite_execution_scalar_string_extended.inc"

#include "mylite_execution_scalar_misc.inc"

#include "mylite_execution_scalar_conversion.inc"

#include "mylite_execution_scalar_temporal_format.inc"

#include "mylite_execution_scalar_bitwise_eval.inc"

#include "mylite_execution_scalar_logical_eval.inc"

#include "mylite_execution_scalar_comparison_eval.inc"

#include "mylite_execution_scalar_arithmetic_eval.inc"

#include "mylite_execution_scalar_diagnostic_helpers.inc"

#include "mylite_execution_scalar_control_case_entry.inc"

#include "mylite_execution_scalar_control_if_eval.inc"

#include "mylite_execution_scalar_literal_projection.inc"

#include "mylite_execution_scalar_system_variables.inc"

#include "mylite_execution_scalar_control_validation.inc"

#include "mylite_execution_scalar_projection.inc"

#include "mylite_execution_delete_planning.inc"

#include "mylite_execution_column_plan_entry.inc"

#include "mylite_execution_column_default_finalization.inc"

#include "mylite_execution_column_default_text.inc"

#include "mylite_execution_column_default_integer_eval.inc"

#include "mylite_execution_column_type_mapping.inc"

#include "mylite_execution_column_type_predicates.inc"

#include "mylite_execution_column_descriptor_parsing.inc"

#include "mylite_execution_column_row_size_validation.inc"

#include "mylite_execution_column_key_modify_validation.inc"

#include "mylite_execution_descriptor_helpers.inc"

#include "mylite_execution_insert_row_planning.inc"

#include "mylite_execution_insert_value_conversion.inc"

#include "mylite_execution_dml_default_values.inc"

#include "mylite_execution_dml_integer_conversion.inc"

#include "mylite_execution_dml_enum_set_conversion.inc"

#include "mylite_execution_dml_string_binary_conversion.inc"

#include "mylite_execution_dml_decimal_approx_conversion.inc"

#include "mylite_execution_dml_temporal_defaults.inc"

#include "mylite_execution_dml_value_helpers.inc"

#include "mylite_execution_dml_string_validation.inc"

#include "mylite_execution_dml_implicit_values.inc"

#include "mylite_execution_row_scalar_select_items.inc"

#include "mylite_execution_query_planning.inc"

#include "mylite_execution_row_scalar_string_basic_planning.inc"

#include "mylite_execution_row_scalar_string_shape_planning.inc"

#include "mylite_execution_row_scalar_string_bitmask_search_planning.inc"

#include "mylite_execution_row_scalar_string_edit_planning.inc"

#include "mylite_execution_row_scalar_string_transform_planning.inc"

#include "mylite_execution_row_scalar_string_compare_set_planning.inc"

#include "mylite_execution_row_scalar_string_regexp_planning.inc"

#include "mylite_execution_row_scalar_json_planning.inc"

#include "mylite_execution_row_scalar_binary_value_planning.inc"

#include "mylite_execution_row_scalar_char_charset_planning.inc"

#include "mylite_execution_row_scalar_control_flow_planning.inc"

#include "mylite_execution_row_scalar_conversion_value_planning.inc"

#include "mylite_execution_row_scalar_concat_planning.inc"

#include "mylite_execution_row_scalar_temporal_format_planning.inc"

#include "mylite_execution_row_scalar_temporal_interval_extract_planning.inc"

#include "mylite_execution_row_scalar_temporal_conversion_planning.inc"

#include "mylite_execution_row_scalar_temporal_period_timezone_weight_planning.inc"

#include "mylite_execution_row_scalar_temporal_diff_planning.inc"

#include "mylite_execution_row_scalar_temporal_timestamp_planning.inc"

#include "mylite_execution_row_scalar_misc_planning.inc"

#include "mylite_execution_select_column_planning.inc"

#include "mylite_execution_select_predicate_entry.inc"

#include "mylite_execution_select_predicate_leaf_comparison.inc"

#include "mylite_execution_select_predicate_temporal_extract.inc"

#include "mylite_execution_select_predicate_string_functions.inc"

#include "mylite_execution_select_predicate_json_regexp_functions.inc"

#include "mylite_execution_select_predicate_subquery_correlation.inc"

#include "mylite_execution_select_predicate_special_in.inc"

#include "mylite_execution_select_predicate_work_helpers.inc"

#include "mylite_execution_select_predicate_value_conversion.inc"

#include "mylite_execution_select_predicate_temporal_literals.inc"

#include "mylite_execution_select_order_planning.inc"

#include "mylite_execution_update_planning_helpers.inc"

#include "mylite_execution_show_tables_helpers.inc"

#include "mylite_execution_show_table_status_rows_helpers.inc"

#include "mylite_execution_show_table_status_where_helpers.inc"

#include "mylite_execution_show_columns_helpers.inc"

#include "mylite_execution_show_index_rows_helpers.inc"

#include "mylite_execution_show_index_where_helpers.inc"

#include "mylite_execution_show_column_display_helpers.inc"

#include "mylite_execution_show_databases_helpers.inc"

#include "mylite_execution_show_filter_helpers.inc"

#include "mylite_execution_show_result_name_helpers.inc"

#include "mylite_execution_show_table_status_count_helpers.inc"

#include "mylite_execution_show_like_pattern_helpers.inc"

#include "mylite_execution_sql_builder_create_table_index_helpers.inc"

#include "mylite_execution_sql_builder_drop_alter_add_column_index.inc"

#include "mylite_execution_sql_builder_alter_column_defaults.inc"

#include "mylite_execution_sql_builder_alter_modify_copy.inc"

#include "mylite_execution_sql_builder_alter_order_force_rename_truncate.inc"

#include "mylite_execution_insert_sql_builders.inc"

#include "mylite_execution_select_sql_builders.inc"

#include "mylite_execution_row_scalar_sql_core.inc"

#include "mylite_execution_row_scalar_sql_functions.inc"

#include "mylite_execution_row_scalar_sql_json_control.inc"

#include "mylite_execution_aggregate_predicate_sql_builders.inc"

#include "mylite_execution_dml_sql_builders.inc"

#include "mylite_execution_sqlite_write_statements.inc"

#include "mylite_execution_insert_duplicate_write_helpers.inc"

#include "mylite_execution_update_unique_key_write_conflicts.inc"

#include "mylite_execution_foreign_key_write_validation.inc"

#include "mylite_execution_unique_key_write_lookup.inc"

#include "mylite_execution_key_tuple_formatting.inc"

#include "mylite_execution_row_scalar_select_parameter_binding.inc"

#include "mylite_execution_count_having_select.inc"

#include "mylite_execution_count_expression_aggregate.inc"

#include "mylite_execution_row_scalar_expression_parameter_dispatch.inc"

#include "mylite_execution_row_scalar_window_parameter_binding.inc"

#include "mylite_execution_row_scalar_conversion_parameter_binding.inc"

#include "mylite_execution_row_scalar_arithmetic_parameter_binding.inc"

#include "mylite_execution_row_scalar_temporal_string_parameter_binding.inc"

#include "mylite_execution_row_scalar_string_regexp_parameter_binding.inc"

#include "mylite_execution_row_scalar_json_parameter_binding.inc"

#include "mylite_execution_row_scalar_control_flow_parameter_binding.inc"

#include "mylite_execution_row_scalar_encoding_uuid_char_parameter_binding.inc"

#include "mylite_execution_predicate_dml_parameter_binding.inc"

#include "mylite_execution_sqlite_result_extraction.inc"
