#include "mylite_connection_statement.h"

#include "mylite_charset.h"
#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_statement.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int copy_connection_charset_statement(const struct mylite_sql_ast_node *statement,
                                             struct mylite_connection_charset_plan *plan);
static int copy_connection_sql_mode_statement(mylite_db *database,
                                              const struct mylite_sql_ast_node *statement,
                                              struct mylite_connection_sql_mode_plan *plan);
static int copy_connection_sql_mode_replace_statement(mylite_db *database,
                                                      const struct mylite_sql_ast_node *value,
                                                      struct mylite_connection_sql_mode_plan *plan);
static bool set_sql_mode_variable_is_session_sql_mode(const char *variable);
static int execute_set_sql_mode_replace_statement(mylite_stmt *stmt);
static char *replace_sql_mode_text(mylite_db *database, const char *value, const char *search,
                                   const char *replacement);

int mylite_connection_prepare_charset_statement(mylite_db *database,
                                                const struct mylite_sql_ast_node *statement,
                                                mylite_stmt **out_stmt)
{
    mylite_stmt *stmt = NULL;
    enum mylite_stmt_kind kind = MYLITE_STMT_SQLITE;
    int status = MYLITE_OK;

    if (statement->kind == MYLITE_SQL_AST_SET_NAMES_STATEMENT) {
        kind = MYLITE_STMT_SET_NAMES;
    } else if (statement->kind == MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT) {
        kind = MYLITE_STMT_SET_CHARACTER_SET;
    } else {
        return MYLITE_UNSUPPORTED;
    }

    stmt = calloc(1U, sizeof(*stmt));
    if (stmt == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    *stmt = (mylite_stmt){
        .database = database,
        .kind = kind,
        .affected_rows = 0,
    };

    status = copy_connection_charset_statement(statement, &stmt->connection_charset);
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        mylite_finalize(stmt);
        return status;
    }

    *out_stmt = stmt;
    return MYLITE_OK;
}

int mylite_connection_prepare_sql_mode_statement(mylite_db *database,
                                                 const struct mylite_sql_ast_node *statement,
                                                 mylite_stmt **out_stmt)
{
    mylite_stmt *stmt = calloc(1U, sizeof(*stmt));
    int status = MYLITE_OK;

    if (stmt == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    *stmt = (mylite_stmt){
        .database = database,
        .kind = MYLITE_STMT_SET_SQL_MODE,
        .affected_rows = 0,
    };

    status = copy_connection_sql_mode_statement(database, statement, &stmt->connection_sql_mode);
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        mylite_finalize(stmt);
        return status;
    }

    *out_stmt = stmt;
    return MYLITE_OK;
}

int mylite_connection_execute_set_names_statement(mylite_stmt *stmt)
{
    const struct mylite_connection_charset_plan *plan = &stmt->connection_charset;

    if (plan->use_default) {
        return mylite_connection_set_default_state(stmt->database);
    }
    return mylite_connection_set_names_state(stmt->database,
                                             (struct mylite_connection_names_state){
                                                 .character_set_name = plan->character_set_name,
                                                 .collation_name = plan->collation_name,
                                             });
}

int mylite_connection_execute_set_character_set_statement(mylite_stmt *stmt)
{
    const struct mylite_connection_charset_plan *plan = &stmt->connection_charset;

    if (plan->use_default) {
        return mylite_connection_set_character_set_state(stmt->database,
                                                         mylite_charset_default_name());
    }
    return mylite_connection_set_character_set_state(stmt->database, plan->character_set_name);
}

int mylite_connection_execute_set_sql_mode_statement(mylite_stmt *stmt)
{
    const struct mylite_connection_sql_mode_plan *plan = &stmt->connection_sql_mode;

    if (plan->use_default) {
        return mylite_connection_set_default_sql_mode(stmt->database);
    }
    if (plan->replace_current_value) {
        return execute_set_sql_mode_replace_statement(stmt);
    }
    return mylite_connection_set_sql_mode(stmt->database, plan->value);
}

void mylite_connection_charset_plan_deinit(struct mylite_connection_charset_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    free(plan->character_set_name);
    free(plan->collation_name);
    *plan = (struct mylite_connection_charset_plan){0};
}

void mylite_connection_sql_mode_plan_deinit(struct mylite_connection_sql_mode_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    free(plan->value);
    free(plan->replace_search);
    free(plan->replace_replacement);
    *plan = (struct mylite_connection_sql_mode_plan){0};
}

static int copy_connection_charset_statement(const struct mylite_sql_ast_node *statement,
                                             struct mylite_connection_charset_plan *plan)
{
    const struct mylite_sql_ast_node *character_set = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *collation = mylite_ast_child_at(statement, 1U);

    if (character_set != NULL && character_set->kind == MYLITE_SQL_AST_DEFAULT) {
        plan->use_default = true;
        return MYLITE_OK;
    }

    plan->character_set_name = mylite_copy_schema_text_span(character_set);
    if (plan->character_set_name == NULL) {
        return MYLITE_NOMEM;
    }

    if (collation != NULL) {
        plan->collation_name = mylite_copy_schema_text_span(collation);
        if (plan->collation_name == NULL) {
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

static int copy_connection_sql_mode_statement(mylite_db *database,
                                              const struct mylite_sql_ast_node *statement,
                                              struct mylite_connection_sql_mode_plan *plan)
{
    const struct mylite_sql_ast_node *variable = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *value = mylite_ast_child_at(statement, 1U);
    char *variable_name = mylite_copy_schema_text_span(variable);

    if (variable_name == NULL) {
        return MYLITE_NOMEM;
    }
    if (!set_sql_mode_variable_is_session_sql_mode(variable_name)) {
        free(variable_name);
        (void)mylite_diagnostics_set_error_message(database, "unsupported SET variable");
        return MYLITE_UNSUPPORTED;
    }
    free(variable_name);

    if (value != NULL && value->kind == MYLITE_SQL_AST_DEFAULT) {
        plan->use_default = true;
        return MYLITE_OK;
    }
    if (value != NULL && value->kind == MYLITE_SQL_AST_FUNCTION_CALL) {
        return copy_connection_sql_mode_replace_statement(database, value, plan);
    }

    plan->value = mylite_copy_string_literal_span(value);
    return plan->value == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static int copy_connection_sql_mode_replace_statement(mylite_db *database,
                                                      const struct mylite_sql_ast_node *value,
                                                      struct mylite_connection_sql_mode_plan *plan)
{
    const struct mylite_sql_ast_node *function_name = mylite_ast_child_at(value, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(value, 1U);
    const struct mylite_sql_ast_node *variable =
        arguments == NULL ? NULL : mylite_ast_child_at(arguments, 0U);
    const struct mylite_sql_ast_node *search =
        arguments == NULL ? NULL : mylite_ast_child_at(arguments, 1U);
    const struct mylite_sql_ast_node *replacement =
        arguments == NULL ? NULL : mylite_ast_child_at(arguments, 2U);
    char *variable_name = NULL;

    if (function_name == NULL || !mylite_span_equal_ci(function_name->span, "REPLACE") ||
        arguments == NULL || mylite_sql_ast_node_child_count(arguments) != 3U || variable == NULL ||
        search == NULL || replacement == NULL || search->kind != MYLITE_SQL_AST_LITERAL ||
        search->literal_kind != MYLITE_SQL_AST_LITERAL_STRING ||
        replacement->kind != MYLITE_SQL_AST_LITERAL ||
        replacement->literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        (void)mylite_diagnostics_set_error_message(database, "unsupported SET sql_mode value");
        return MYLITE_UNSUPPORTED;
    }

    variable_name = mylite_copy_schema_text_span(variable);
    if (variable_name == NULL) {
        return MYLITE_NOMEM;
    }
    if (!set_sql_mode_variable_is_session_sql_mode(variable_name)) {
        free(variable_name);
        (void)mylite_diagnostics_set_error_message(database, "unsupported SET sql_mode value");
        return MYLITE_UNSUPPORTED;
    }
    free(variable_name);

    plan->replace_search = mylite_copy_string_literal_span(search);
    plan->replace_replacement = mylite_copy_string_literal_span(replacement);
    if (plan->replace_search == NULL || plan->replace_replacement == NULL) {
        return MYLITE_NOMEM;
    }
    plan->replace_current_value = true;
    return MYLITE_OK;
}

static bool set_sql_mode_variable_is_session_sql_mode(const char *variable)
{
    if (mylite_ascii_case_equal(variable, "sql_mode")) {
        return true;
    }
    if (variable == NULL || variable[0] != '@' || variable[1] != '@') {
        return false;
    }
    if (mylite_ascii_case_equal(variable + 2, "sql_mode")) {
        return true;
    }
    if (mylite_ascii_case_equal(variable + 2, "session.sql_mode")) {
        return true;
    }
    return mylite_ascii_case_equal(variable + 2, "local.sql_mode");
}

static int execute_set_sql_mode_replace_statement(mylite_stmt *stmt)
{
    const struct mylite_connection_sql_mode_plan *plan = &stmt->connection_sql_mode;
    char *value = replace_sql_mode_text(stmt->database, mylite_connection_sql_mode(stmt->database),
                                        plan->replace_search, plan->replace_replacement);
    int status = MYLITE_OK;

    if (value == NULL) {
        return MYLITE_NOMEM;
    }
    status = mylite_connection_set_sql_mode(stmt->database, value);
    free(value);
    return status;
}

static char *replace_sql_mode_text(mylite_db *database, const char *value, const char *search,
                                   const char *replacement)
{
    size_t value_length = value == NULL ? 0U : strlen(value);
    size_t search_length = search == NULL ? 0U : strlen(search);
    size_t replacement_length = replacement == NULL ? 0U : strlen(replacement);
    size_t occurrence_count = 0U;
    size_t result_length = value_length;
    char *result = NULL;
    char *writer = NULL;
    const char *cursor = value;

    if (search_length == 0U) {
        result = mylite_copy_span_text(value == NULL ? "" : value, value_length);
        if (result == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return result;
    }
    while (cursor != NULL) {
        const char *match = strstr(cursor, search);

        if (match == NULL) {
            break;
        }
        ++occurrence_count;
        cursor = match + search_length;
    }
    if (replacement_length > search_length &&
        occurrence_count > (SIZE_MAX - value_length) / (replacement_length - search_length)) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return NULL;
    }
    if (replacement_length > search_length) {
        result_length += occurrence_count * (replacement_length - search_length);
    } else {
        result_length -= occurrence_count * (search_length - replacement_length);
    }

    result = malloc(result_length + 1U);
    if (result == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return NULL;
    }

    writer = result;
    cursor = value == NULL ? "" : value;
    for (;;) {
        const char *match = strstr(cursor, search);
        size_t prefix_length = 0U;

        if (match == NULL) {
            strcpy(writer, cursor);
            break;
        }
        prefix_length = (size_t)(match - cursor);
        memcpy(writer, cursor, prefix_length);
        writer += prefix_length;
        memcpy(writer, replacement, replacement_length);
        writer += replacement_length;
        cursor = match + search_length;
    }
    return result;
}
