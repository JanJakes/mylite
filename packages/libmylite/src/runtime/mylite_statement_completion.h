#ifndef MYLITE_RUNTIME_MYLITE_STATEMENT_COMPLETION_H
#define MYLITE_RUNTIME_MYLITE_STATEMENT_COMPLETION_H

#include "mylite_diagnostics.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_db;

struct mylite_statement_completion {
    struct mylite_diagnostics diagnostics;
    int status;
    int64_t row_count;
    int64_t affected_rows;
    uint64_t found_rows;
    uint64_t insert_id;
    size_t warning_count;
    bool updates_found_rows;
    bool preserves_diagnostics_snapshot;
    bool captured;
    bool published;
};

void mylite_statement_completion_init(struct mylite_statement_completion *completion);
void mylite_statement_completion_deinit(struct mylite_statement_completion *completion);
void mylite_statement_completion_reset(struct mylite_statement_completion *completion);
int mylite_statement_completion_capture(
    struct mylite_statement_completion *completion,
    const struct mylite_db *database,
    int status,
    int64_t row_count,
    int64_t affected_rows,
    uint64_t found_rows,
    uint64_t insert_id,
    size_t warning_count,
    bool updates_found_rows,
    bool preserves_diagnostics_snapshot
);
int mylite_statement_completion_publish(
    struct mylite_statement_completion *completion,
    struct mylite_db *database
);
void mylite_statement_completion_publish_failure_fallback(struct mylite_db *database);

#endif
