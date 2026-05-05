#ifndef MYLITE_RUNTIME_MYLITE_DML_INSERT_TRANSACTION_FINISH_H
#define MYLITE_RUNTIME_MYLITE_DML_INSERT_TRANSACTION_FINISH_H

#include <mylite/mylite.h>

#include "mylite_dml_types.h"
#include "mylite_transaction_types.h"

int mylite_dml_finish_failed_insert_transaction(mylite_db *database, const char *schema_name,
                                                const char *table_name,
                                                const struct mylite_insert_table *table,
                                                const struct mylite_insert_execution_state *state,
                                                const struct mylite_statement_atomicity *atomicity,
                                                int original_status);
int mylite_dml_finish_successful_insert_transaction(
    mylite_db *database, const char *schema_name, const char *table_name,
    const struct mylite_insert_table *table, const struct mylite_insert_execution_state *state,
    struct mylite_statement_atomicity *atomicity, struct mylite_insert_transaction_result *result);
int mylite_dml_finish_successful_replace_transaction(
    mylite_db *database, const char *schema_name, const char *table_name,
    const struct mylite_insert_table *table, const struct mylite_insert_execution_state *state,
    struct mylite_statement_atomicity *atomicity, struct mylite_insert_transaction_result *result);

#endif
