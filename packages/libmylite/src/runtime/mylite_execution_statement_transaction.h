#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_STATEMENT_TRANSACTION_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_STATEMENT_TRANSACTION_H

#include <stdbool.h>

struct mylite_db;

enum mylite_statement_transaction_kind {
    MYLITE_STATEMENT_TRANSACTION_NONE = 0,
    MYLITE_STATEMENT_TRANSACTION_DIRECT = 1,
    MYLITE_STATEMENT_TRANSACTION_SAVEPOINT = 2,
};

struct mylite_statement_transaction {
    enum mylite_statement_transaction_kind kind;
    bool active;
};

int mylite_execution_begin_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
);
int mylite_execution_begin_read_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
);
int mylite_execution_begin_autocommit_disabled_transaction(struct mylite_db *database);
int mylite_execution_commit_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
);
int mylite_execution_rollback_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction,
    int primary_rc
);
int mylite_execution_normalize_sqlite_control_rc(struct mylite_db *database, int rc);

static inline int begin_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
) {
    return mylite_execution_begin_statement_transaction(database, transaction);
}

static inline int begin_read_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
) {
    return mylite_execution_begin_read_statement_transaction(database, transaction);
}

static inline int commit_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
) {
    return mylite_execution_commit_statement_transaction(database, transaction);
}

static inline int rollback_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction,
    int primary_rc
) {
    return mylite_execution_rollback_statement_transaction(database, transaction, primary_rc);
}

static inline int normalize_sqlite_control_rc(struct mylite_db *database, int rc) {
    return mylite_execution_normalize_sqlite_control_rc(database, rc);
}

#endif
