#include "mylite_dml_insert_default.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_dml.h"
#include "mylite_dml_insert_diagnostics.h"
#include "mylite_span.h"
#include "sql/mylite_parser.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int set_insert_bound_text_value(
    mylite_db *database,
    const char *text,
    size_t text_length,
    struct mylite_insert_bound_value *out_value
);

static int reserve_insert_auto_increment(
    mylite_db *database,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    uint64_t first_value
);

static bool insert_column_uses_numeric_implicit_default(
    const struct mylite_insert_table_column *column
);

static bool insert_column_uses_integer_storage(const struct mylite_insert_table_column *column);

static int resolve_insert_large_integer_text_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    const char *text,
    size_t text_length,
    bool ignore,
    struct mylite_insert_bound_value *out_value
);

static bool insert_text_requires_integer_text_coercion(
    const struct mylite_insert_table_column *column,
    const char *text,
    size_t text_length
);

static bool insert_plan_coerces_missing_required_default(
    const mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table_column *column
);

static bool insert_column_uses_temporal_storage(const struct mylite_insert_table_column *column);

static bool insert_column_uses_text_storage(const struct mylite_insert_table_column *column);

static bool insert_text_integer_prefix_exceeds_int64(const char *text, size_t text_length);

static char *insert_current_timestamp_text(unsigned int fsp);

static unsigned int insert_column_current_timestamp_fsp(
    const struct mylite_insert_table_column *column,
    unsigned int default_fsp
);

static unsigned int insert_column_temporal_fsp(const struct mylite_insert_table_column *column);

static bool parse_insert_column_type_fsp(const char *column_type, unsigned int *out_fsp);

static int resolve_insert_generated_default_bound_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
);

static char *insert_generated_default_select_sql(const char *default_text);

static unsigned int insert_default_parse_modes(const mylite_db *database);

static const struct mylite_sql_ast_node *insert_generated_default_expression(
    const struct mylite_sql_parse_result *parse_result
);

int mylite_dml_resolve_insert_default_bound_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
) {
    if (database == NULL || column == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    if (!column->has_default) {
        return mylite_dml_insert_set_no_default_error(database, column->name);
    }
    if (column->auto_increment && state == NULL) {
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = 0,
        };
        return MYLITE_OK;
    }
    if (column->auto_increment && column->default_text == NULL) {
        return mylite_dml_allocate_insert_auto_increment(
            database,
            statement_row_count,
            state,
            out_value
        );
    }
    if (column->default_text == NULL) {
        if (column->nullable) {
            *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
            return MYLITE_OK;
        }
        return mylite_dml_insert_set_no_default_error(database, column->name);
    }
    {
        unsigned int default_fsp = 0U;

        if (mylite_column_default_current_timestamp_fsp(column->default_text, &default_fsp)) {
            char *timestamp = insert_current_timestamp_text(
                insert_column_current_timestamp_fsp(column, default_fsp)
            );

            if (timestamp == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
            *out_value = (struct mylite_insert_bound_value){
                .kind = MYLITE_INSERT_BOUND_TEXT,
                .text_value = timestamp,
                .text_length = timestamp == NULL ? 0U : strlen(timestamp),
            };
            return MYLITE_OK;
        }
    }
    if (column->generated_default) {
        return resolve_insert_generated_default_bound_value(
            database,
            column,
            statement_row_count,
            state,
            out_value
        );
    }
    int status = mylite_dml_resolve_insert_text_value(
        database,
        column,
        column->default_text,
        column->default_text == NULL ? 0U : strlen(column->default_text),
        statement_row_count,
        state,
        false,
        out_value
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (column->auto_increment && out_value->kind == MYLITE_INSERT_BOUND_INTEGER &&
        out_value->integer_value > 0) {
        out_value->generated_auto_increment = true;
    }
    return MYLITE_OK;
}

static int resolve_insert_generated_default_bound_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
) {
    struct mylite_sql_parse_result parse_result = {0};
    struct mylite_insert_values_plan plan = {.table_name = ""};
    struct mylite_insert_table table = {
        .columns = (struct mylite_insert_table_column *)column,
        .column_count = 1U,
    };
    char *sql = insert_generated_default_select_sql(column->default_text);
    const struct mylite_sql_ast_node *expression = NULL;
    int status = MYLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    enum mylite_sql_parse_status parse_status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = strlen(sql),
            .modes = insert_default_parse_modes(database),
        },
        &parse_result
    );
    if (parse_status != MYLITE_SQL_PARSE_OK) {
        status = mylite_dml_insert_set_unsupported_generated_default_error(database, column->name);
        goto done;
    }

    expression = insert_generated_default_expression(&parse_result);
    if (expression == NULL) {
        status = mylite_dml_insert_set_unsupported_generated_default_error(database, column->name);
        goto done;
    }
    status = mylite_dml_resolve_insert_expression_bound_value(
        database,
        "",
        &plan,
        &table,
        NULL,
        column,
        expression,
        statement_row_count,
        state,
        NULL,
        out_value
    );

done:
    mylite_sql_parse_result_deinit(&parse_result);
    free(sql);
    return status;
}

static char *insert_generated_default_select_sql(const char *default_text) {
    static const char prefix[] = "SELECT ";
    size_t prefix_length = sizeof(prefix) - 1U;
    size_t default_length = default_text == NULL ? 0U : strlen(default_text);
    char *sql = malloc(prefix_length + default_length + 1U);

    if (sql == NULL) {
        return NULL;
    }
    memcpy(sql, prefix, prefix_length);
    if (default_length > 0U) {
        memcpy(sql + prefix_length, default_text, default_length);
    }
    sql[prefix_length + default_length] = '\0';
    return sql;
}

static unsigned int insert_default_parse_modes(const mylite_db *database) {
    unsigned int modes = 0U;

    if (mylite_connection_sql_mode_has_ansi_quotes(database)) {
        modes |= MYLITE_SQL_MODE_ANSI_QUOTES;
    }
    if (mylite_connection_sql_mode_has_no_backslash_escapes(database)) {
        modes |= MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES;
    }
    return modes;
}

static const struct mylite_sql_ast_node *insert_generated_default_expression(
    const struct mylite_sql_parse_result *parse_result
) {
    const struct mylite_sql_ast_node *statement =
        parse_result == NULL ? NULL : mylite_ast_single_statement(parse_result->root);
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *item = select_list == NULL ? NULL : select_list->first_child;

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT ||
        select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST || item == NULL ||
        item->kind != MYLITE_SQL_AST_SELECT_ITEM || item->next_sibling != NULL) {
        return NULL;
    }
    return mylite_ast_child_at(item, 0U);
}

int mylite_dml_resolve_insert_implicit_expression_default(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_insert_bound_value *out_value
) {
    const char *text_default = "";

    if (database == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    if (column != NULL && column->nullable) {
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        return MYLITE_OK;
    }
    if (insert_column_uses_numeric_implicit_default(column)) {
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = 0,
        };
        return MYLITE_OK;
    }
    if (column != NULL && column->data_type != NULL) {
        if (mylite_ascii_case_equal(column->data_type, "date")) {
            text_default = "0000-00-00";
        } else if (mylite_ascii_case_equal(column->data_type, "time")) {
            text_default = "00:00:00";
        } else if (
            mylite_ascii_case_equal(column->data_type, "datetime") ||
            mylite_ascii_case_equal(column->data_type, "timestamp")
        ) {
            text_default = "0000-00-00 00:00:00";
        }
    }

    out_value->text_value = mylite_copy_span_text(text_default, strlen(text_default));
    if (out_value->text_value == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_value->kind = MYLITE_INSERT_BOUND_TEXT;
    out_value->text_length = strlen(text_default);
    return MYLITE_OK;
}

int mylite_dml_resolve_insert_current_timestamp_bound_value(
    mylite_db *database,
    struct mylite_insert_bound_value *out_value
) {
    char *timestamp = NULL;

    if (database == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    timestamp = insert_current_timestamp_text(0U);
    if (timestamp == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *out_value = (struct mylite_insert_bound_value){
        .kind = MYLITE_INSERT_BOUND_TEXT,
        .text_value = timestamp,
        .text_length = timestamp == NULL ? 0U : strlen(timestamp),
    };
    return MYLITE_OK;
}

uint64_t mylite_dml_insert_auto_increment_next_value(
    const struct mylite_insert_execution_state *state
) {
    if (state == NULL) {
        return 0U;
    }
    if (state->reserved_auto_increment_end > state->next_auto_increment) {
        return state->reserved_auto_increment_end;
    }
    return state->next_auto_increment;
}

int mylite_dml_resolve_insert_explicit_default_value(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table_column *column,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
) {
    if (insert_plan_coerces_missing_required_default(database, plan, column)) {
        int status = mylite_dml_insert_append_no_default_warning(database, column->name);

        if (status != MYLITE_OK) {
            return status;
        }
        return mylite_dml_resolve_insert_implicit_expression_default(database, column, out_value);
    }
    return mylite_dml_resolve_insert_default_bound_value(
        database,
        column,
        statement_row_count,
        state,
        out_value
    );
}

int mylite_dml_resolve_insert_omitted_default_value(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table_column *column,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    size_t column_index,
    struct mylite_insert_bound_value *out_value
) {
    if (insert_plan_coerces_missing_required_default(database, plan, column)) {
        int status =
            mylite_dml_insert_append_no_default_warning_once(database, column, state, column_index);

        if (status != MYLITE_OK) {
            return status;
        }
        return mylite_dml_resolve_insert_implicit_expression_default(database, column, out_value);
    }
    return mylite_dml_resolve_insert_default_bound_value(
        database,
        column,
        statement_row_count,
        state,
        out_value
    );
}

int mylite_dml_resolve_insert_text_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    const char *text,
    size_t text_length,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    bool ignore,
    struct mylite_insert_bound_value *out_value
) {
    int64_t integer_value = 0;
    double real_value = 0.0;

    if (text == NULL) {
        if (!column->nullable) {
            return mylite_dml_set_not_null_column_error(database, column->name);
        }
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        return MYLITE_OK;
    }
    if (insert_column_uses_temporal_storage(column)) {
        int status = set_insert_bound_text_value(database, text, text_length, out_value);

        if (status == MYLITE_OK) {
            status =
                mylite_dml_coerce_insert_temporal_value(database, column, 1U, ignore, out_value);
        }
        return status == MYLITE_OK
                   ? mylite_dml_coerce_insert_string_value(database, column, 1U, ignore, out_value)
                   : status;
    }
    if (mylite_dml_parse_insert_integer_text(text, &integer_value)) {
        int status = MYLITE_OK;

        if (integer_value == 0 &&
            mylite_dml_insert_auto_increment_zero_generates(database, column)) {
            return mylite_dml_allocate_insert_auto_increment(
                database,
                statement_row_count,
                state,
                out_value
            );
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = integer_value,
        };
        status = mylite_dml_coerce_insert_numeric_value(database, column, 1U, ignore, out_value);
        if (status == MYLITE_OK) {
            status = mylite_dml_coerce_insert_string_value(database, column, 1U, ignore, out_value);
        }
        return status;
    }
    if (column->auto_increment) {
        return mylite_dml_insert_set_unsupported_expression_error(database);
    }
    if (insert_text_requires_integer_text_coercion(column, text, text_length)) {
        return resolve_insert_large_integer_text_value(
            database,
            column,
            text,
            text_length,
            ignore,
            out_value
        );
    }
    if (mylite_dml_parse_insert_real_text(text, &real_value)) {
        int status = MYLITE_OK;

        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_REAL,
            .real_value = real_value,
        };
        status = mylite_dml_coerce_insert_numeric_value(database, column, 1U, ignore, out_value);
        if (status == MYLITE_OK) {
            status = mylite_dml_coerce_insert_string_value(database, column, 1U, ignore, out_value);
        }
        return status;
    }

    int status = set_insert_bound_text_value(database, text, text_length, out_value);

    if (status == MYLITE_OK) {
        status = mylite_dml_coerce_insert_numeric_value(database, column, 1U, ignore, out_value);
    }
    return status == MYLITE_OK
               ? mylite_dml_coerce_insert_string_value(database, column, 1U, ignore, out_value)
               : status;
}

int mylite_dml_resolve_insert_quoted_text_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    const char *text,
    size_t text_length,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    bool ignore,
    struct mylite_insert_bound_value *out_value
) {
    if (text == NULL || !insert_column_uses_text_storage(column)) {
        return mylite_dml_resolve_insert_text_value(
            database,
            column,
            text,
            text_length,
            statement_row_count,
            state,
            ignore,
            out_value
        );
    }
    int status = set_insert_bound_text_value(database, text, text_length, out_value);

    return status == MYLITE_OK
               ? mylite_dml_coerce_insert_string_value(database, column, 1U, ignore, out_value)
               : status;
}

bool mylite_dml_insert_auto_increment_zero_generates(
    const mylite_db *database,
    const struct mylite_insert_table_column *column
) {
    return (column != NULL && column->auto_increment &&
            !mylite_connection_sql_mode_has_no_auto_value_on_zero(database)) != 0;
}

int mylite_dml_allocate_insert_auto_increment(
    mylite_db *database,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
) {
    uint64_t value = 0U;
    int status = MYLITE_OK;

    if (state == NULL) {
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = 0,
        };
        return MYLITE_OK;
    }

    value = state->next_auto_increment == 0U ? 1U : state->next_auto_increment;
    if (value > (uint64_t)INT64_MAX) {
        (void)
            mylite_diagnostics_set_error_message(database, "AUTO_INCREMENT value is out of range");
        return MYLITE_EXEC_ERROR;
    }
    status = reserve_insert_auto_increment(database, statement_row_count, state, value);
    if (status != MYLITE_OK) {
        return status;
    }
    state->next_auto_increment = value + 1U;
    *out_value = (struct mylite_insert_bound_value){
        .kind = MYLITE_INSERT_BOUND_INTEGER,
        .integer_value = (int64_t)value,
        .generated_auto_increment = true,
    };
    return MYLITE_OK;
}

static int set_insert_bound_text_value(
    mylite_db *database,
    const char *text,
    size_t text_length,
    struct mylite_insert_bound_value *out_value
) {
    out_value->text_value = mylite_copy_span_text(text, text_length);
    if (out_value->text_value == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_value->kind = MYLITE_INSERT_BOUND_TEXT;
    out_value->text_length = text_length;
    return MYLITE_OK;
}

static int reserve_insert_auto_increment(
    mylite_db *database,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    uint64_t first_value
) {
    if (state->reserved_auto_increment_end != 0U) {
        return MYLITE_OK;
    }
    if (statement_row_count > (uint64_t)INT64_MAX - first_value) {
        (void)
            mylite_diagnostics_set_error_message(database, "AUTO_INCREMENT value is out of range");
        return MYLITE_EXEC_ERROR;
    }
    state->reserved_auto_increment_end = first_value + statement_row_count;
    return MYLITE_OK;
}

static bool insert_plan_coerces_missing_required_default(
    const mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table_column *column
) {
    return (plan != NULL && column != NULL && !column->has_default &&
            (plan->ignore || !mylite_connection_sql_mode_is_strict(database))) != 0;
}

static bool insert_column_uses_numeric_implicit_default(
    const struct mylite_insert_table_column *column
) {
    static const char *const numeric_types[] = {
        "tinyint",
        "smallint",
        "mediumint",
        "int",
        "bigint",
        "decimal",
        "float",
        "double",
        "bool",
        "boolean",
        "year",
    };

    if (column == NULL || column->data_type == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(numeric_types) / sizeof(numeric_types[0]); ++index) {
        if (mylite_ascii_case_equal(column->data_type, numeric_types[index])) {
            return true;
        }
    }
    return false;
}

static bool insert_column_uses_integer_storage(const struct mylite_insert_table_column *column) {
    static const char *const integer_types[] = {
        "tinyint",
        "smallint",
        "mediumint",
        "int",
        "bigint",
        "bool",
        "boolean",
        "year",
    };

    if (column == NULL || column->data_type == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(integer_types) / sizeof(integer_types[0]); ++index) {
        if (mylite_ascii_case_equal(column->data_type, integer_types[index])) {
            return true;
        }
    }
    return false;
}

static int resolve_insert_large_integer_text_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    const char *text,
    size_t text_length,
    bool ignore,
    struct mylite_insert_bound_value *out_value
) {
    int status = set_insert_bound_text_value(database, text, text_length, out_value);

    if (status == MYLITE_OK) {
        status = mylite_dml_coerce_insert_numeric_value(database, column, 1U, ignore, out_value);
    }
    if (status == MYLITE_OK) {
        status = mylite_dml_coerce_insert_string_value(database, column, 1U, ignore, out_value);
    }
    return status;
}

static bool insert_text_requires_integer_text_coercion(
    const struct mylite_insert_table_column *column,
    const char *text,
    size_t text_length
) {
    if (!insert_column_uses_integer_storage(column)) {
        return false;
    }
    return insert_text_integer_prefix_exceeds_int64(text, text_length);
}

static bool insert_column_uses_temporal_storage(const struct mylite_insert_table_column *column) {
    static const char *const temporal_types[] = {
        "date",
        "datetime",
        "timestamp",
    };

    if (column == NULL || column->data_type == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(temporal_types) / sizeof(temporal_types[0]); ++index) {
        if (mylite_ascii_case_equal(column->data_type, temporal_types[index])) {
            return true;
        }
    }
    return false;
}

static bool insert_column_uses_text_storage(const struct mylite_insert_table_column *column) {
    static const char *const text_types[] = {
        "char",
        "varchar",
        "tinytext",
        "text",
        "mediumtext",
        "longtext",
        "binary",
        "varbinary",
        "tinyblob",
        "blob",
        "mediumblob",
        "longblob",
    };

    if (column == NULL || column->data_type == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(text_types) / sizeof(text_types[0]); ++index) {
        if (mylite_ascii_case_equal(column->data_type, text_types[index])) {
            return true;
        }
    }
    return false;
}

static bool insert_text_integer_prefix_exceeds_int64(const char *text, size_t text_length) {
    enum { decimal_base = 10 };

    uint64_t magnitude = 0U;
    size_t offset = 0U;
    uint64_t limit = (uint64_t)INT64_MAX;
    bool saw_digit = false;

    if (text == NULL) {
        return false;
    }
    while (offset < text_length && isspace((unsigned char)text[offset])) {
        ++offset;
    }
    if (offset < text_length && text[offset] == '+') {
        ++offset;
    } else if (offset < text_length && text[offset] == '-') {
        limit = (uint64_t)INT64_MAX + UINT64_C(1);
        ++offset;
    }
    while (offset < text_length && isdigit((unsigned char)text[offset])) {
        uint64_t digit = (uint64_t)(text[offset] - '0');

        saw_digit = true;
        if (magnitude > (limit - digit) / decimal_base) {
            return true;
        }
        magnitude = (magnitude * decimal_base) + digit;
        ++offset;
    }
    return (saw_digit && magnitude > limit) != 0;
}

static char *insert_current_timestamp_text(unsigned int fsp) {
    enum {
        timestamp_length = 19U,
        microsecond_length = 6U,
    };

    time_t now = time(NULL);
    long microseconds = 0;
    struct tm tm_value;
    size_t text_length = timestamp_length + (fsp == 0U ? 0U : 1U + fsp);
    char *timestamp = malloc(text_length + 1U);

    if (timestamp == NULL) {
        return NULL;
    }
#ifdef TIME_UTC
    {
        struct timespec timespec_now;

        if (timespec_get(&timespec_now, TIME_UTC) == TIME_UTC) {
            now = timespec_now.tv_sec;
            microseconds = timespec_now.tv_nsec / 1000L;
        }
    }
#endif
#ifdef _WIN32
    if (gmtime_s(&tm_value, &now) != 0) {
        free(timestamp);
        return NULL;
    }
#else
    if (gmtime_r(&now, &tm_value) == NULL) {
        free(timestamp);
        return NULL;
    }
#endif
    if (strftime(timestamp, timestamp_length + 1U, "%Y-%m-%d %H:%M:%S", &tm_value) == 0U) {
        free(timestamp);
        return NULL;
    }
    if (fsp > 0U) {
        char microsecond_text[microsecond_length + 1U];

        snprintf(microsecond_text, sizeof(microsecond_text), "%06ld", microseconds);
        timestamp[timestamp_length] = '.';
        memcpy(timestamp + timestamp_length + 1U, microsecond_text, fsp);
        timestamp[text_length] = '\0';
    }
    return timestamp;
}

static unsigned int insert_column_current_timestamp_fsp(
    const struct mylite_insert_table_column *column,
    unsigned int default_fsp
) {
    unsigned int column_fsp = insert_column_temporal_fsp(column);

    return default_fsp < column_fsp ? default_fsp : column_fsp;
}

static unsigned int insert_column_temporal_fsp(const struct mylite_insert_table_column *column) {
    unsigned int fsp = 0U;

    if (column == NULL || (!mylite_ascii_case_equal(column->data_type, "datetime") &&
                           !mylite_ascii_case_equal(column->data_type, "timestamp"))) {
        return 0U;
    }
    (void)parse_insert_column_type_fsp(column->column_type, &fsp);
    return fsp;
}

static bool parse_insert_column_type_fsp(const char *column_type, unsigned int *out_fsp) {
    const char *open = column_type == NULL ? NULL : strchr(column_type, '(');
    const char *close = open == NULL ? NULL : strchr(open, ')');
    unsigned int fsp = 0U;

    if (out_fsp != NULL) {
        *out_fsp = 0U;
    }
    if (open == NULL || close == NULL || close <= open + 1) {
        return false;
    }
    for (const char *cursor = open + 1; cursor < close; ++cursor) {
        if (!isdigit((unsigned char)*cursor)) {
            return false;
        }
        fsp = (fsp * 10U) + (unsigned int)(*cursor - '0');
        if (fsp > 6U) {
            return false;
        }
    }
    if (out_fsp != NULL) {
        *out_fsp = fsp;
    }
    return true;
}
