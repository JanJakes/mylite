#include "mylite_connection_statement.h"

#include "mylite_charset.h"
#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_statement.h"

#include <stdlib.h>

static int copy_connection_charset_statement(const struct mylite_sql_ast_node *statement,
                                             struct mylite_connection_charset_plan *plan);

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

void mylite_connection_charset_plan_deinit(struct mylite_connection_charset_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    free(plan->character_set_name);
    free(plan->collation_name);
    *plan = (struct mylite_connection_charset_plan){0};
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
