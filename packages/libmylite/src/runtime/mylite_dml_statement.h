#ifndef MYLITE_RUNTIME_MYLITE_DML_STATEMENT_H
#define MYLITE_RUNTIME_MYLITE_DML_STATEMENT_H

#include <mylite/mylite.h>

struct mylite_dml_expression_callbacks;

int mylite_dml_execute_insert_values_statement(
    mylite_stmt *stmt, const struct mylite_dml_expression_callbacks *expression_callbacks);
int mylite_dml_execute_insert_set_statement(
    mylite_stmt *stmt, const struct mylite_dml_expression_callbacks *expression_callbacks);
int mylite_dml_append_replace_delayed_warning(mylite_stmt *stmt);
int mylite_dml_execute_replace_values_statement(
    mylite_stmt *stmt, const struct mylite_dml_expression_callbacks *expression_callbacks);
int mylite_dml_execute_replace_set_statement(
    mylite_stmt *stmt, const struct mylite_dml_expression_callbacks *expression_callbacks);
int mylite_dml_execute_update_statement(
    mylite_stmt *stmt, const struct mylite_dml_expression_callbacks *expression_callbacks);
int mylite_dml_execute_delete_statement(
    mylite_stmt *stmt, const struct mylite_dml_expression_callbacks *expression_callbacks);

#endif
