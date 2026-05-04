#include "mylite_show.h"

#include "mylite_diagnostics.h"
#include "mylite_show_types.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "sqlite3.h"

#include <stdint.h>
#include <stdlib.h>

static bool show_diagnostics_query_from_statement(const struct mylite_sql_ast_node *statement,
                                                  struct mylite_show_diagnostics_query *out_query);
static char *copy_show_variables_like_pattern(const struct mylite_sql_ast_node *statement);
static char *copy_show_status_like_pattern(const struct mylite_sql_ast_node *statement);
static char *copy_show_character_set_like_pattern(const struct mylite_sql_ast_node *statement);
static char *copy_show_collation_like_pattern(const struct mylite_sql_ast_node *statement);
static bool decode_show_string_escape(char escaped, char *out_character);

int mylite_show_prepare_diagnostics_statement(mylite_db *database,
                                              const struct mylite_sql_ast_node *statement,
                                              mylite_stmt **out_stmt)
{
    struct mylite_show_diagnostics_query query = {0};
    char *sqlite_sql = NULL;
    int status = MYLITE_OK;

    if (!show_diagnostics_query_from_statement(statement, &query)) {
        return MYLITE_UNSUPPORTED;
    }

    sqlite_sql = mylite_show_diagnostics_sql(database, &query);
    if (sqlite_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    if (status == MYLITE_OK) {
        (*out_stmt)->preserve_prepare_warnings = true;
    }
    sqlite3_free(sqlite_sql);
    return status;
}

int mylite_show_prepare_diagnostics_count_statement(mylite_db *database,
                                                    const struct mylite_sql_ast_node *statement,
                                                    mylite_stmt **out_stmt)
{
    char *sqlite_sql =
        mylite_show_diagnostics_count_sql(database, statement->show_diagnostics_kind);
    int status = MYLITE_OK;

    if (sqlite_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    if (status == MYLITE_OK) {
        (*out_stmt)->preserve_prepare_warnings = true;
    }
    sqlite3_free(sqlite_sql);
    return status;
}

int mylite_show_prepare_variables_statement(mylite_db *database,
                                            const struct mylite_sql_ast_node *statement,
                                            mylite_stmt **out_stmt)
{
    char *like_pattern = NULL;
    char *sqlite_sql = NULL;
    int status = MYLITE_OK;

    if (mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_WHERE_CLAUSE) != NULL) {
        (void)mylite_diagnostics_set_error_message(database,
                                                   "SHOW VARIABLES WHERE is not supported");
        return MYLITE_UNSUPPORTED;
    }

    like_pattern = copy_show_variables_like_pattern(statement);
    if (mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LITERAL) != NULL &&
        like_pattern == NULL) {
        status = MYLITE_NOMEM;
    }
    if (status == MYLITE_OK) {
        status = mylite_show_variables_sql(database,
                                           &(const struct mylite_show_variables_query){
                                               .scope = statement->show_variables_scope,
                                               .like_pattern = like_pattern,
                                           },
                                           &sqlite_sql);
    }
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    free(like_pattern);
    sqlite3_free(sqlite_sql);
    return status;
}

int mylite_show_prepare_status_statement(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         mylite_stmt **out_stmt)
{
    char *like_pattern = NULL;
    char *sqlite_sql = NULL;
    int status = MYLITE_OK;

    if (mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_WHERE_CLAUSE) != NULL) {
        (void)mylite_diagnostics_set_error_message(database, "SHOW STATUS WHERE is not supported");
        return MYLITE_UNSUPPORTED;
    }

    like_pattern = copy_show_status_like_pattern(statement);
    if (mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LITERAL) != NULL &&
        like_pattern == NULL) {
        status = MYLITE_NOMEM;
    }
    if (status == MYLITE_OK) {
        status = mylite_show_status_sql(database,
                                        &(const struct mylite_show_status_query){
                                            .scope = statement->show_status_scope,
                                            .like_pattern = like_pattern,
                                        },
                                        &sqlite_sql);
    }
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    free(like_pattern);
    sqlite3_free(sqlite_sql);
    return status;
}

int mylite_show_prepare_character_set_statement(mylite_db *database,
                                                const struct mylite_sql_ast_node *statement,
                                                mylite_stmt **out_stmt)
{
    char *like_pattern = NULL;
    char *sqlite_sql = NULL;
    int status = MYLITE_OK;

    if (mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_WHERE_CLAUSE) != NULL) {
        (void)mylite_diagnostics_set_error_message(database,
                                                   "SHOW CHARACTER SET WHERE is not supported");
        return MYLITE_UNSUPPORTED;
    }

    like_pattern = copy_show_character_set_like_pattern(statement);
    if (mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LITERAL) != NULL &&
        like_pattern == NULL) {
        status = MYLITE_NOMEM;
    }
    if (status == MYLITE_OK) {
        status = mylite_show_character_set_sql(database,
                                               &(const struct mylite_show_character_set_query){
                                                   .like_pattern = like_pattern,
                                               },
                                               &sqlite_sql);
    }
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    free(like_pattern);
    sqlite3_free(sqlite_sql);
    return status;
}

int mylite_show_prepare_collation_statement(mylite_db *database,
                                            const struct mylite_sql_ast_node *statement,
                                            mylite_stmt **out_stmt)
{
    char *like_pattern = NULL;
    char *sqlite_sql = NULL;
    int status = MYLITE_OK;

    if (mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_WHERE_CLAUSE) != NULL) {
        (void)mylite_diagnostics_set_error_message(database,
                                                   "SHOW COLLATION WHERE is not supported");
        return MYLITE_UNSUPPORTED;
    }

    like_pattern = copy_show_collation_like_pattern(statement);
    if (mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LITERAL) != NULL &&
        like_pattern == NULL) {
        status = MYLITE_NOMEM;
    }
    if (status == MYLITE_OK) {
        status = mylite_show_collation_sql(database,
                                           &(const struct mylite_show_collation_query){
                                               .like_pattern = like_pattern,
                                           },
                                           &sqlite_sql);
    }
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    free(like_pattern);
    sqlite3_free(sqlite_sql);
    return status;
}

static bool show_diagnostics_query_from_statement(const struct mylite_sql_ast_node *statement,
                                                  struct mylite_show_diagnostics_query *out_query)
{
    const struct mylite_sql_ast_node *limit = mylite_ast_child_at(statement, 0U);

    *out_query = (struct mylite_show_diagnostics_query){
        .kind = statement->show_diagnostics_kind,
        .offset = 0U,
        .row_count = UINT64_MAX,
        .has_limit = false,
    };

    if (limit == NULL) {
        return true;
    }
    if (limit->kind != MYLITE_SQL_AST_LIMIT_CLAUSE ||
        mylite_sql_ast_node_child_count(limit) != 2U) {
        return false;
    }
    out_query->offset = mylite_ast_child_at(limit, 0U)->limit_bound_value;
    out_query->row_count = mylite_ast_child_at(limit, 1U)->limit_bound_value;
    out_query->has_limit = true;
    return true;
}

static char *copy_show_variables_like_pattern(const struct mylite_sql_ast_node *statement)
{
    const struct mylite_sql_ast_node *literal =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LITERAL);

    if (literal == NULL) {
        return NULL;
    }
    return mylite_show_copy_like_pattern_span(literal);
}

static char *copy_show_status_like_pattern(const struct mylite_sql_ast_node *statement)
{
    const struct mylite_sql_ast_node *literal =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LITERAL);

    if (literal == NULL) {
        return NULL;
    }
    return mylite_show_copy_like_pattern_span(literal);
}

static char *copy_show_character_set_like_pattern(const struct mylite_sql_ast_node *statement)
{
    const struct mylite_sql_ast_node *literal =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LITERAL);

    if (literal == NULL) {
        return NULL;
    }
    return mylite_show_copy_like_pattern_span(literal);
}

static char *copy_show_collation_like_pattern(const struct mylite_sql_ast_node *statement)
{
    const struct mylite_sql_ast_node *literal =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LITERAL);

    if (literal == NULL) {
        return NULL;
    }
    return mylite_show_copy_like_pattern_span(literal);
}

char *mylite_show_copy_like_pattern_span(const struct mylite_sql_ast_node *node)
{
    const char *text = node == NULL ? NULL : node->span.text;
    size_t length = node == NULL ? 0U : node->span.length;
    size_t start = 0U;
    size_t end = length;
    char *copy = NULL;
    size_t output = 0U;

    if (text == NULL) {
        return NULL;
    }
    if (length >= 2U && (text[0] == '\'' || text[0] == '"')) {
        start = 1U;
        end = length - 1U;
    } else if (length >= 3U && (text[0] == 'N' || text[0] == 'n') &&
               (text[1] == '\'' || text[1] == '"')) {
        start = 2U;
        end = length - 1U;
    }

    copy = malloc(end >= start ? end - start + 1U : 1U);
    if (copy == NULL) {
        return NULL;
    }

    for (size_t index = start; index < end; ++index) {
        if (text[index] == '\\' && index + 1U < end) {
            char escaped = '\0';

            if (decode_show_string_escape(text[index + 1U], &escaped)) {
                copy[output++] = escaped;
                ++index;
            } else {
                copy[output++] = text[index];
            }
        } else if ((text[index] == '\'' || text[index] == '"') && index + 1U < end &&
                   text[index + 1U] == text[index]) {
            copy[output++] = text[index++];
        } else {
            copy[output++] = text[index];
        }
    }
    copy[output] = '\0';
    return copy;
}

static bool decode_show_string_escape(char escaped, char *out_character)
{
    switch (escaped) {
    case '\'':
    case '"':
    case '\\':
        *out_character = escaped;
        return true;
    case 'b':
        *out_character = '\b';
        return true;
    case 'n':
        *out_character = '\n';
        return true;
    case 'r':
        *out_character = '\r';
        return true;
    case 't':
        *out_character = '\t';
        return true;
    case '0':
        *out_character = '\0';
        return true;
    case 'Z':
        *out_character = '\x1a';
        return true;
    default:
        return false;
    }
}
