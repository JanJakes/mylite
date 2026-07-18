#include "mylite_statement_completion.h"

#include "mylite_connection.h"

#include <string.h>

void mylite_statement_completion_init(struct mylite_statement_completion *completion) {
    if (completion == NULL) {
        return;
    }

    *completion = (struct mylite_statement_completion){0};
    mylite_diagnostics_init(&completion->diagnostics);
    completion->status = MYLITE_OK;
    completion->row_count = -1;
    completion->affected_rows = -1;
}

void mylite_statement_completion_deinit(struct mylite_statement_completion *completion) {
    if (completion == NULL) {
        return;
    }

    mylite_diagnostics_deinit(&completion->diagnostics);
    memset(completion, 0, sizeof(*completion));
}

void mylite_statement_completion_reset(struct mylite_statement_completion *completion) {
    mylite_statement_completion_deinit(completion);
    mylite_statement_completion_init(completion);
}

int mylite_statement_completion_capture(
    struct mylite_statement_completion *completion,
    const struct mylite_db *database,
    int status,
    int64_t row_count,
    int64_t affected_rows,
    uint64_t found_rows,
    uint64_t insert_id,
    bool updates_found_rows,
    bool preserves_diagnostics_snapshot
) {
    int rc = MYLITE_OK;

    if (completion == NULL || database == NULL) {
        return MYLITE_MISUSE;
    }

    mylite_statement_completion_reset(completion);
    rc = mylite_diagnostics_replace(&completion->diagnostics, &database->diagnostics);
    if (rc != MYLITE_OK) {
        return rc;
    }

    completion->status = status;
    completion->row_count = row_count;
    completion->affected_rows = affected_rows;
    completion->found_rows = found_rows;
    completion->insert_id = insert_id;
    completion->warning_count = mylite_diagnostics_warning_total_count(&completion->diagnostics);
    completion->updates_found_rows = updates_found_rows;
    completion->preserves_diagnostics_snapshot = preserves_diagnostics_snapshot;
    completion->captured = true;
    return MYLITE_OK;
}

int mylite_statement_completion_publish(
    struct mylite_statement_completion *completion,
    struct mylite_db *database
) {
    int rc = MYLITE_OK;

    if (completion == NULL || database == NULL || !completion->captured) {
        return MYLITE_MISUSE;
    }
    if (completion->published) {
        return MYLITE_OK;
    }

    if (!completion->preserves_diagnostics_snapshot) {
        rc = mylite_diagnostics_replace(&database->previous_diagnostics, &completion->diagnostics);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    database->session.previous_row_count = completion->row_count;
    if (completion->updates_found_rows) {
        database->session.found_rows = completion->found_rows;
    }
    completion->published = true;
    return MYLITE_OK;
}

void mylite_statement_completion_publish_failure_fallback(struct mylite_db *database) {
    if (database != NULL) {
        database->session.previous_row_count = -1;
    }
}
