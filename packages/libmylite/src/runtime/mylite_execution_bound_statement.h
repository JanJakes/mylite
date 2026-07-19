#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_BOUND_STATEMENT_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_BOUND_STATEMENT_H

#include <mylite/mylite.h>

const char *mylite_execution_bound_statement_character_set_client(const mylite_stmt *stmt);
const char *mylite_execution_bound_statement_character_set_connection(const mylite_stmt *stmt);
const char *mylite_execution_bound_statement_collation_connection(const mylite_stmt *stmt);

#endif
