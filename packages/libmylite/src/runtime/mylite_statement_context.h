#ifndef MYLITE_RUNTIME_MYLITE_STATEMENT_CONTEXT_H
#define MYLITE_RUNTIME_MYLITE_STATEMENT_CONTEXT_H

#include <mylite/mylite.h>

#include "mylite_result_metadata.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

enum mylite_statement_wrapper_transaction_state {
    MYLITE_STATEMENT_WRAPPER_TRANSACTION_NONE = 0,
    MYLITE_STATEMENT_WRAPPER_TRANSACTION_ACTIVE = 1,
    MYLITE_STATEMENT_WRAPPER_TRANSACTION_COMMITTED = 2,
    MYLITE_STATEMENT_WRAPPER_TRANSACTION_ROLLED_BACK = 3,
};

enum mylite_statement_backend_status {
    MYLITE_STATEMENT_BACKEND_NOT_STARTED = 0,
    MYLITE_STATEMENT_BACKEND_RUNNING = 1,
    MYLITE_STATEMENT_BACKEND_DONE = 2,
    MYLITE_STATEMENT_BACKEND_FAILED = 3,
};

struct mylite_db;

struct mylite_statement_context {
    struct mylite_db *database;
    const char *sql;
    size_t sql_size;
    time_t statement_time;
    int64_t affected_rows;
    int64_t previous_row_count;
    uint64_t previous_found_rows;
    uint64_t first_insert_id;
    bool has_first_insert_id;
    bool active;
    enum mylite_statement_wrapper_transaction_state wrapper_transaction_state;
    enum mylite_statement_backend_status backend_status;
    struct mylite_result_metadata result_metadata;
};

void mylite_statement_context_init(struct mylite_statement_context *context);
void mylite_statement_context_deinit(struct mylite_statement_context *context);

int mylite_statement_context_begin(
    struct mylite_statement_context *context,
    struct mylite_db *database,
    const char *sql,
    size_t sql_size
);
int mylite_statement_context_end(struct mylite_statement_context *context, int completion_code);

const char *mylite_statement_context_sql(const struct mylite_statement_context *context);
size_t mylite_statement_context_sql_size(const struct mylite_statement_context *context);
time_t mylite_statement_context_time(const struct mylite_statement_context *context);
bool mylite_statement_context_is_active(const struct mylite_statement_context *context);

void mylite_statement_context_set_affected_rows(
    struct mylite_statement_context *context,
    int64_t affected_rows
);
int64_t mylite_statement_context_affected_rows(const struct mylite_statement_context *context);
void mylite_statement_context_set_previous_row_count(
    struct mylite_statement_context *context,
    int64_t previous_row_count
);
int64_t mylite_statement_context_previous_row_count(const struct mylite_statement_context *context);
void mylite_statement_context_set_previous_found_rows(
    struct mylite_statement_context *context,
    uint64_t previous_found_rows
);
uint64_t mylite_statement_context_previous_found_rows(
    const struct mylite_statement_context *context
);

void mylite_statement_context_set_first_insert_id(
    struct mylite_statement_context *context,
    uint64_t first_insert_id
);
bool mylite_statement_context_has_first_insert_id(const struct mylite_statement_context *context);
uint64_t mylite_statement_context_first_insert_id(const struct mylite_statement_context *context);

void mylite_statement_context_set_wrapper_transaction_state(
    struct mylite_statement_context *context,
    enum mylite_statement_wrapper_transaction_state state
);
enum mylite_statement_wrapper_transaction_state mylite_statement_context_wrapper_transaction_state(
    const struct mylite_statement_context *context
);

void mylite_statement_context_set_backend_status(
    struct mylite_statement_context *context,
    enum mylite_statement_backend_status status
);
enum mylite_statement_backend_status mylite_statement_context_backend_status(
    const struct mylite_statement_context *context
);

struct mylite_result_metadata *mylite_statement_context_result_metadata(
    struct mylite_statement_context *context
);

#endif
