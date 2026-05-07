#ifndef MYLITE_RUNTIME_MYLITE_STATEMENT_PREPARE_H
#define MYLITE_RUNTIME_MYLITE_STATEMENT_PREPARE_H

#include <mylite/mylite.h>

#include "mylite_parser.h"
#include "mylite_sqlite_translator.h"
#include "mylite_statement_types.h"

struct mylite_select_prepare_callbacks;
struct mylite_select_scalar_eval_callbacks;
struct mylite_select_union_prepare_callbacks;

struct mylite_statement_prepare_callbacks {
    const struct mylite_select_prepare_callbacks *select_callbacks;
    const struct mylite_select_scalar_eval_callbacks *scalar_callbacks;
    const struct mylite_select_union_prepare_callbacks *union_callbacks;
};

int mylite_statement_prepare_with_callbacks(
    mylite_db *database,
    const char *sql,
    size_t length,
    mylite_stmt **out_stmt,
    const struct mylite_statement_prepare_callbacks *callbacks
);
int mylite_statement_prepare_with_callbacks_and_modes(
    mylite_db *database,
    const char *sql,
    size_t length,
    mylite_stmt **out_stmt,
    const struct mylite_statement_prepare_callbacks *callbacks,
    unsigned int modes
);
int mylite_statement_map_parse_status(mylite_db *database, enum mylite_sql_parse_status status);
int mylite_statement_map_translate_status(
    mylite_db *database,
    enum mylite_sqlite_translate_status status
);
int mylite_statement_prepare_schema_lifecycle_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt,
    const struct mylite_statement_prepare_callbacks *callbacks
);
int mylite_statement_prepare_transaction_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt,
    const struct mylite_statement_prepare_callbacks *callbacks
);
int mylite_statement_prepare_custom_statement(
    mylite_db *database,
    enum mylite_stmt_kind kind,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt,
    const struct mylite_statement_prepare_callbacks *callbacks
);

#endif
