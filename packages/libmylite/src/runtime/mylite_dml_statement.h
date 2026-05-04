#ifndef MYLITE_RUNTIME_MYLITE_DML_STATEMENT_H
#define MYLITE_RUNTIME_MYLITE_DML_STATEMENT_H

#include <mylite/mylite.h>

int mylite_dml_execute_insert_values_statement(mylite_stmt *stmt);
int mylite_dml_execute_insert_set_statement(mylite_stmt *stmt);
int mylite_dml_execute_replace_values_statement(mylite_stmt *stmt);
int mylite_dml_execute_replace_set_statement(mylite_stmt *stmt);

#endif
