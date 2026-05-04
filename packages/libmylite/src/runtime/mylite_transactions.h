#ifndef MYLITE_RUNTIME_MYLITE_TRANSACTIONS_H
#define MYLITE_RUNTIME_MYLITE_TRANSACTIONS_H

#include "mylite_runtime.h"

int mylite_transaction_begin_explicit(mylite_db *database,
                                      enum mylite_transaction_access_mode access_mode,
                                      bool consistent_snapshot);
int mylite_transaction_commit_explicit(mylite_db *database);
int mylite_transaction_rollback_explicit(mylite_db *database);
int mylite_transaction_begin_storage(mylite_db *database);
int mylite_transaction_commit_storage(mylite_db *database);
void mylite_transaction_rollback_storage(mylite_db *database);
int mylite_transaction_create_savepoint(mylite_db *database, const char *name,
                                        const char *normalized_name);
size_t mylite_transaction_find_savepoint(const mylite_db *database, const char *normalized_name);
int mylite_transaction_rollback_to_savepoint(mylite_db *database, size_t index);
int mylite_transaction_release_savepoint(mylite_db *database, size_t index);
void mylite_transaction_savepoint_state_deinit(struct mylite_savepoint_state *state);
int mylite_transaction_record_pending_auto_increment(mylite_db *database, const char *schema_name,
                                                     const char *table_name,
                                                     uint64_t next_auto_increment);
void mylite_transaction_clear_pending_auto_increments(mylite_db *database);
int mylite_transaction_begin_statement_atomicity(mylite_db *database,
                                                 struct mylite_statement_atomicity *atomicity);
int mylite_transaction_commit_statement_atomicity(mylite_db *database,
                                                  struct mylite_statement_atomicity *atomicity);
void mylite_transaction_rollback_statement_atomicity(
    mylite_db *database, const struct mylite_statement_atomicity *atomicity);

#endif
