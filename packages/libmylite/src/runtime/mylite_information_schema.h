#ifndef MYLITE_RUNTIME_MYLITE_INFORMATION_SCHEMA_H
#define MYLITE_RUNTIME_MYLITE_INFORMATION_SCHEMA_H

#include <mylite/mylite.h>

#include "sql/mylite_ast.h"

#include <stdbool.h>

bool mylite_information_schema_has_table(const char *name);
int mylite_information_schema_prepare_select_statement(mylite_db *database,
                                                       const struct mylite_sql_ast_node *statement,
                                                       mylite_stmt **out_stmt);

#endif
