#include "mylite_show.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_show_types.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "sqlite3.h"

#include <stdlib.h>
#include <string.h>

static int copy_show_tables_schema_name(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        char **out_schema_name);
static int validate_show_tables_schema(mylite_db *database, const char *schema_name);
static char *copy_show_tables_like_pattern(const struct mylite_sql_ast_node *statement);
static char *copy_show_tables_display_pattern(const char *like_pattern, bool uppercase_pattern);
static char *show_tables_column_name(const char *schema_name, const char *like_pattern);
static char *show_tables_glob_pattern(const char *like_pattern);
static int normalize_show_tables_schema_name(char **schema_name);
static void append_show_tables_glob_literal(sqlite3_str *glob, char character);

int mylite_show_prepare_tables_statement(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         mylite_stmt **out_stmt)
{
    char *schema_name = NULL;
    char *like_pattern = NULL;
    char *display_pattern = NULL;
    char *glob_pattern = NULL;
    char *column_name = NULL;
    char *sqlite_sql = NULL;
    int status = copy_show_tables_schema_name(database, statement, &schema_name);

    if (status == MYLITE_OK) {
        status = validate_show_tables_schema(database, schema_name);
    }
    if (status == MYLITE_OK &&
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_WHERE_CLAUSE) != NULL) {
        (void)mylite_diagnostics_set_error_message(database, "SHOW TABLES WHERE is not supported");
        status = MYLITE_UNSUPPORTED;
    }
    if (status == MYLITE_OK) {
        like_pattern = copy_show_tables_like_pattern(statement);
        if (mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LITERAL) != NULL &&
            like_pattern == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK && like_pattern != NULL) {
        display_pattern = copy_show_tables_display_pattern(
            like_pattern, mylite_ascii_case_equal(schema_name, "information_schema"));
        if (display_pattern == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK && display_pattern != NULL) {
        glob_pattern = show_tables_glob_pattern(display_pattern);
        if (glob_pattern == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK) {
        column_name = show_tables_column_name(schema_name, display_pattern);
        if (column_name == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK) {
        sqlite_sql = mylite_show_tables_sql(database, &(const struct mylite_show_tables_query){
                                                          .schema_name = schema_name,
                                                          .column_name = column_name,
                                                          .glob_pattern = glob_pattern,
                                                          .full = statement->show_tables_full,
                                                      });
        if (sqlite_sql == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    free(schema_name);
    free(like_pattern);
    free(display_pattern);
    sqlite3_free(column_name);
    sqlite3_free(glob_pattern);
    sqlite3_free(sqlite_sql);
    return status;
}

int mylite_show_prepare_table_status_statement(mylite_db *database,
                                               const struct mylite_sql_ast_node *statement,
                                               mylite_stmt **out_stmt)
{
    char *schema_name = NULL;
    char *like_pattern = NULL;
    char *display_pattern = NULL;
    char *glob_pattern = NULL;
    char *sqlite_sql = NULL;
    int status = copy_show_tables_schema_name(database, statement, &schema_name);

    if (status == MYLITE_OK) {
        status = validate_show_tables_schema(database, schema_name);
    }
    if (status == MYLITE_OK &&
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_WHERE_CLAUSE) != NULL) {
        (void)mylite_diagnostics_set_error_message(database,
                                                   "SHOW TABLE STATUS WHERE is not supported");
        status = MYLITE_UNSUPPORTED;
    }
    if (status == MYLITE_OK) {
        like_pattern = copy_show_tables_like_pattern(statement);
        if (mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LITERAL) != NULL &&
            like_pattern == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK && like_pattern != NULL) {
        display_pattern = copy_show_tables_display_pattern(
            like_pattern, mylite_ascii_case_equal(schema_name, "information_schema"));
        if (display_pattern == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK && display_pattern != NULL) {
        glob_pattern = show_tables_glob_pattern(display_pattern);
        if (glob_pattern == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK) {
        sqlite_sql =
            mylite_show_table_status_sql(database, &(const struct mylite_show_table_status_query){
                                                       .schema_name = schema_name,
                                                       .glob_pattern = glob_pattern,
                                                   });
        if (sqlite_sql == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    free(schema_name);
    free(like_pattern);
    free(display_pattern);
    sqlite3_free(glob_pattern);
    sqlite3_free(sqlite_sql);
    return status;
}

static int copy_show_tables_schema_name(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        char **out_schema_name)
{
    const struct mylite_sql_ast_node *schema_name =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_IDENTIFIER);

    *out_schema_name = NULL;
    if (schema_name != NULL) {
        *out_schema_name = mylite_copy_identifier_span(schema_name);
        if (*out_schema_name == NULL) {
            return MYLITE_NOMEM;
        }
        return normalize_show_tables_schema_name(out_schema_name);
    }
    if (database->selected_schema == NULL || database->selected_schema[0] == '\0') {
        (void)mylite_diagnostics_set_error_message(database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }

    *out_schema_name = mylite_copy_nonempty_cstring(database->selected_schema);
    if (*out_schema_name == NULL) {
        return MYLITE_NOMEM;
    }
    return normalize_show_tables_schema_name(out_schema_name);
}

static int validate_show_tables_schema(mylite_db *database, const char *schema_name)
{
    struct mylite_schema_presence presence;
    int status = mylite_catalog_schema_exists(database, schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(database, "Unknown database '",
                                                         schema_name, "'");
        return MYLITE_EXEC_ERROR;
    }
    return MYLITE_OK;
}

static char *copy_show_tables_like_pattern(const struct mylite_sql_ast_node *statement)
{
    const struct mylite_sql_ast_node *literal =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LITERAL);

    if (literal == NULL) {
        return NULL;
    }
    return mylite_show_copy_like_pattern_span(literal);
}

static char *copy_show_tables_display_pattern(const char *like_pattern, bool uppercase_pattern)
{
    char *display_pattern = mylite_copy_span_text(like_pattern, strlen(like_pattern));

    if (display_pattern == NULL) {
        return NULL;
    }
    if (uppercase_pattern) {
        mylite_uppercase_ascii_text(display_pattern);
    }
    return display_pattern;
}

static char *show_tables_column_name(const char *schema_name, const char *like_pattern)
{
    if (like_pattern == NULL) {
        return sqlite3_mprintf("Tables_in_%s", schema_name);
    }
    return sqlite3_mprintf("Tables_in_%s (%s)", schema_name, like_pattern);
}

static char *show_tables_glob_pattern(const char *like_pattern)
{
    sqlite3_str *glob = sqlite3_str_new(NULL);

    if (glob == NULL) {
        return NULL;
    }

    for (size_t index = 0U; like_pattern[index] != '\0'; ++index) {
        char character = like_pattern[index];

        if (character == '\\' && like_pattern[index + 1U] != '\0') {
            append_show_tables_glob_literal(glob, like_pattern[++index]);
        } else if (character == '%') {
            sqlite3_str_append(glob, "*", 1);
        } else if (character == '_') {
            sqlite3_str_append(glob, "?", 1);
        } else {
            append_show_tables_glob_literal(glob, character);
        }
    }
    return sqlite3_str_finish(glob);
}

static int normalize_show_tables_schema_name(char **schema_name)
{
    char *normalized = NULL;

    if (schema_name == NULL || *schema_name == NULL ||
        !mylite_ascii_case_equal(*schema_name, "information_schema") ||
        strcmp(*schema_name, "information_schema") == 0) {
        return MYLITE_OK;
    }

    normalized = mylite_copy_nonempty_cstring("information_schema");
    if (normalized == NULL) {
        return MYLITE_NOMEM;
    }

    free(*schema_name);
    *schema_name = normalized;
    return MYLITE_OK;
}

static void append_show_tables_glob_literal(sqlite3_str *glob, char character)
{
    char literal[1] = {character};

    switch (character) {
    case '*':
        sqlite3_str_append(glob, "[*]", 3);
        break;
    case '?':
        sqlite3_str_append(glob, "[?]", 3);
        break;
    case '[':
        sqlite3_str_append(glob, "[[]", 3);
        break;
    case ']':
        sqlite3_str_append(glob, "[]]", 3);
        break;
    default:
        sqlite3_str_append(glob, literal, 1);
        break;
    }
}
