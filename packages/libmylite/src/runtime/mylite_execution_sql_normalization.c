#include "mylite_execution_sql_normalization.h"

#include <mylite/mylite.h>

#include "mylite_catalog.h"
#include "mylite_connection.h"
#include "mylite_dynamic_string.h"
#include "mylite_execution_diagnostics.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct set_user_variable_increment_parts {
    size_t name_start;
    size_t name_end;
    size_t rhs_name_start;
    size_t rhs_name_end;
    size_t delta_start;
    size_t delta_end;
};

struct set_assignment_value_word_request {
    size_t sql_size;
    size_t value_start;
};

enum {
    normalization_decimal_base = 10,
    mysql_executable_comment_min_length = 5U,
    mysql_set_keyword_length = 3U,
    set_user_variable_increment_sql_capacity = 64U,
};

static int extract_mysql_executable_comment_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    char **out_sql,
    size_t *out_sql_size,
    bool *out_changed
);
static int rewrite_set_user_variable_increment_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    char **out_sql,
    size_t *out_sql_size,
    bool *out_changed
);
static int quote_set_bare_compat_values_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    char **out_sql,
    size_t *out_sql_size,
    bool *out_changed
);
static bool trim_sql_span(const char *sql, size_t sql_size, size_t *out_start, size_t *out_end);
static bool parse_set_user_variable_increment_sql(
    const char *sql,
    size_t sql_size,
    struct set_user_variable_increment_parts *out_parts
);
static bool sql_span_equals_ascii_case_insensitive(
    const char *text,
    size_t text_size,
    const char *expected
);
static size_t skip_sql_spaces(const char *sql, size_t end, size_t index);
static size_t scan_sql_identifier(const char *sql, size_t end, size_t index);
static bool sql_byte_is_identifier(char byte);
static bool set_user_variable_increment_result(
    const char *sql,
    const struct set_user_variable_increment_parts *parts,
    const struct mylite_session_user_variable *variable,
    int64_t *out_value
);
static const struct mylite_session_user_variable *find_normalization_session_user_variable(
    const struct mylite_session_state *session,
    const char *name
);
static int format_set_user_variable_increment_sql(
    struct mylite_db *database,
    const char *sql,
    const struct set_user_variable_increment_parts *parts,
    int64_t result_value,
    char **out_sql,
    size_t *out_sql_size
);
static bool set_assignment_target_before_equal(
    const char *sql,
    size_t assignment_start,
    size_t equal_index,
    char *buffer,
    size_t buffer_size
);
static bool set_target_needs_bare_value_quoting(const char *target_name);
static bool set_assignment_value_word(
    const char *sql,
    struct set_assignment_value_word_request request,
    size_t *out_value_end
);
static void copy_folded_sql_identifier(
    const char *text,
    size_t text_size,
    char *buffer,
    size_t buffer_size
);
static bool sql_byte_starts_identifier(char byte);
static char normalization_ascii_lower(unsigned char byte);

int mylite_execution_normalize_mysql_compat_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    struct mylite_execution_normalized_sql *out_sql
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
    *out_sql = (struct mylite_execution_normalized_sql){.sql = sql, .sql_size = sql_size};

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

void mylite_execution_normalized_sql_deinit(struct mylite_execution_normalized_sql *sql) {
    if (sql == NULL) {
        return;
    }
    free(sql->owned_sql);
    *sql = (struct mylite_execution_normalized_sql){0};
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
        mylite_execution_diagnostics_set_nomem_error(database);
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
    const struct mylite_session_user_variable *variable = NULL;
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
    variable = find_normalization_session_user_variable(&database->session, name);
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
    parsed_current_value = strtoimax(variable->value, &endptr, normalization_decimal_base);
    if (errno != 0 || endptr == variable->value || *endptr != '\0') {
        return false;
    }
    if (parsed_current_value < (intmax_t)INT64_MIN || parsed_current_value > (intmax_t)INT64_MAX) {
        return false;
    }
    current_value = (int64_t)parsed_current_value;
    for (size_t digit = parts->delta_start; digit < parts->delta_end; ++digit) {
        uint64_t next_digit = (uint64_t)(sql[digit] - '0');

        if (delta_magnitude > (UINT64_MAX - next_digit) / (uint64_t)normalization_decimal_base) {
            return false;
        }
        delta_magnitude = delta_magnitude * (uint64_t)normalization_decimal_base + next_digit;
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

static const struct mylite_session_user_variable *find_normalization_session_user_variable(
    const struct mylite_session_state *session,
    const char *name
) {
    if (session == NULL || name == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < session->user_variable_count; ++index) {
        if (strcmp(session->user_variables[index].name, name) == 0) {
            return &session->user_variables[index];
        }
    }
    return NULL;
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
        mylite_execution_diagnostics_set_runtime_error(
            database,
            "failed to rewrite SET user variable assignment"
        );
        return MYLITE_ERROR;
    }
    *out_sql = (char *)malloc((size_t)written + 1U);
    if (*out_sql == NULL) {
        mylite_execution_diagnostics_set_nomem_error(database);
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
    if (!trim_sql_span(sql, sql_size, &start, &end) || end - start < mysql_set_keyword_length ||
        !sql_span_equals_ascii_case_insensitive(sql + start, mysql_set_keyword_length, "SET")) {
        return MYLITE_OK;
    }

    mylite_dynamic_string_init(&rewritten);
    scan = start + mysql_set_keyword_length;
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
            mylite_execution_diagnostics_set_nomem_error(database);
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
        mylite_execution_diagnostics_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    *out_sql = mylite_dynamic_string_take(&rewritten);
    if (*out_sql == NULL) {
        mylite_dynamic_string_deinit(&rewritten);
        mylite_execution_diagnostics_set_nomem_error(database);
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
        if (normalization_ascii_lower((unsigned char)text[index]) !=
            normalization_ascii_lower((unsigned char)expected[index])) {
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
        buffer[index] = normalization_ascii_lower((unsigned char)text[index]);
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

static char normalization_ascii_lower(unsigned char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte - 'A' + 'a');
    }
    return (char)byte;
}
