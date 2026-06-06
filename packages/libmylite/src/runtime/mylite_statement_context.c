#include "mylite_statement_context.h"

#include "mylite_connection.h"

#include <string.h>

static void reset_statement_fields(
    struct mylite_statement_context *context,
    struct mylite_db *database,
    const char *sql,
    size_t sql_size
);

void mylite_statement_context_init(struct mylite_statement_context *context) {
    if (context == NULL) {
        return;
    }

    *context = (struct mylite_statement_context){0};
    mylite_result_metadata_init(&context->result_metadata);
}

void mylite_statement_context_deinit(struct mylite_statement_context *context) {
    if (context == NULL) {
        return;
    }

    mylite_result_metadata_deinit(&context->result_metadata);
    memset(context, 0, sizeof(*context));
}

int mylite_statement_context_begin(
    struct mylite_statement_context *context,
    struct mylite_db *database,
    const char *sql,
    size_t sql_size
) {
    if (context == NULL || database == NULL) {
        return MYLITE_MISUSE;
    }

    mylite_statement_context_deinit(context);
    reset_statement_fields(context, database, sql, sql_size);
    mylite_diagnostics_reset(mylite_connection_diagnostics(database));

    return MYLITE_OK;
}

int mylite_statement_context_end(struct mylite_statement_context *context, int completion_code) {
    if (context == NULL || context->database == NULL) {
        return MYLITE_MISUSE;
    }

    if (completion_code == MYLITE_OK &&
        context->backend_status == MYLITE_STATEMENT_BACKEND_RUNNING) {
        context->backend_status = MYLITE_STATEMENT_BACKEND_DONE;
    } else if (completion_code != MYLITE_OK) {
        context->backend_status = MYLITE_STATEMENT_BACKEND_FAILED;
    }
    context->active = false;

    return completion_code;
}

const char *mylite_statement_context_sql(const struct mylite_statement_context *context) {
    if (context == NULL) {
        return NULL;
    }

    return context->sql;
}

size_t mylite_statement_context_sql_size(const struct mylite_statement_context *context) {
    if (context == NULL) {
        return 0U;
    }

    return context->sql_size;
}

time_t mylite_statement_context_time(const struct mylite_statement_context *context) {
    if (context == NULL) {
        return (time_t)0;
    }

    return context->statement_time;
}

bool mylite_statement_context_is_active(const struct mylite_statement_context *context) {
    if (context == NULL) {
        return false;
    }

    return context->active;
}

void mylite_statement_context_set_affected_rows(
    struct mylite_statement_context *context,
    int64_t affected_rows
) {
    if (context == NULL) {
        return;
    }

    context->affected_rows = affected_rows;
}

int64_t mylite_statement_context_affected_rows(const struct mylite_statement_context *context) {
    if (context == NULL) {
        return 0;
    }

    return context->affected_rows;
}

void mylite_statement_context_set_previous_row_count(
    struct mylite_statement_context *context,
    int64_t previous_row_count
) {
    if (context == NULL) {
        return;
    }

    context->previous_row_count = previous_row_count;
}

int64_t mylite_statement_context_previous_row_count(const struct mylite_statement_context *context
) {
    if (context == NULL) {
        return 0;
    }

    return context->previous_row_count;
}

void mylite_statement_context_set_previous_found_rows(
    struct mylite_statement_context *context,
    uint64_t previous_found_rows
) {
    if (context == NULL) {
        return;
    }

    context->previous_found_rows = previous_found_rows;
}

uint64_t mylite_statement_context_previous_found_rows(const struct mylite_statement_context *context
) {
    if (context == NULL) {
        return 0U;
    }

    return context->previous_found_rows;
}

void mylite_statement_context_set_first_insert_id(
    struct mylite_statement_context *context,
    uint64_t first_insert_id
) {
    if (context == NULL) {
        return;
    }

    context->first_insert_id = first_insert_id;
    context->has_first_insert_id = true;
}

bool mylite_statement_context_has_first_insert_id(const struct mylite_statement_context *context) {
    if (context == NULL) {
        return false;
    }

    return context->has_first_insert_id;
}

uint64_t mylite_statement_context_first_insert_id(const struct mylite_statement_context *context) {
    if (context == NULL) {
        return 0U;
    }

    return context->first_insert_id;
}

void mylite_statement_context_set_wrapper_transaction_state(
    struct mylite_statement_context *context,
    enum mylite_statement_wrapper_transaction_state state
) {
    if (context == NULL) {
        return;
    }

    context->wrapper_transaction_state = state;
}

enum mylite_statement_wrapper_transaction_state mylite_statement_context_wrapper_transaction_state(
    const struct mylite_statement_context *context
) {
    if (context == NULL) {
        return MYLITE_STATEMENT_WRAPPER_TRANSACTION_NONE;
    }

    return context->wrapper_transaction_state;
}

void mylite_statement_context_set_backend_status(
    struct mylite_statement_context *context,
    enum mylite_statement_backend_status status
) {
    if (context == NULL) {
        return;
    }

    context->backend_status = status;
}

enum mylite_statement_backend_status mylite_statement_context_backend_status(
    const struct mylite_statement_context *context
) {
    if (context == NULL) {
        return MYLITE_STATEMENT_BACKEND_NOT_STARTED;
    }

    return context->backend_status;
}

struct mylite_result_metadata *mylite_statement_context_result_metadata(
    struct mylite_statement_context *context
) {
    if (context == NULL) {
        return NULL;
    }

    return &context->result_metadata;
}

static void reset_statement_fields(
    struct mylite_statement_context *context,
    struct mylite_db *database,
    const char *sql,
    size_t sql_size
) {
    mylite_result_metadata_init(&context->result_metadata);
    context->database = database;
    context->sql = sql;
    context->sql_size = sql == NULL ? 0U : sql_size;
    context->statement_time = time(NULL);
    context->affected_rows = 0;
    context->previous_row_count = 0;
    context->previous_found_rows = 0U;
    context->first_insert_id = 0U;
    context->has_first_insert_id = false;
    context->active = true;
    context->wrapper_transaction_state = MYLITE_STATEMENT_WRAPPER_TRANSACTION_NONE;
    context->backend_status = MYLITE_STATEMENT_BACKEND_RUNNING;
}
