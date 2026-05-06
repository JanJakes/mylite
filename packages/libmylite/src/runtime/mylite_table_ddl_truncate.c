#include "mylite_table_ddl.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_transactions.h"
#include "sqlite3.h"

#include <mylite/mylite.h>

#include <stdlib.h>

static int validate_truncate_table_plan(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_truncate_table_plan *plan
);

static int resolve_truncate_table_name(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_truncate_table_plan *plan
);

static int validate_truncate_table_target(
    mylite_db *database,
    const struct mylite_truncate_table_plan *plan
);

static int truncate_table_transaction(
    mylite_db *database,
    const struct mylite_truncate_table_plan *plan
);

static int delete_truncate_table_rows(
    mylite_db *database,
    const struct mylite_truncate_table_plan *plan
);

static int reset_truncate_table_auto_increment(
    mylite_db *database,
    const struct mylite_truncate_table_plan *plan
);

static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_table_ddl_execute_truncate_table_statement(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_truncate_table_plan *plan
) {
    int status = validate_truncate_table_plan(database, selected_schema, plan);

    if (status == MYLITE_OK) {
        status = truncate_table_transaction(database, plan);
    }
    return status;
}

static int validate_truncate_table_plan(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_truncate_table_plan *plan
) {
    int status = resolve_truncate_table_name(database, selected_schema, plan);

    if (status != MYLITE_OK) {
        return status;
    }
    return validate_truncate_table_target(database, plan);
}

static int resolve_truncate_table_name(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_truncate_table_plan *plan
) {
    if (plan->table_name == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (plan->schema_name != NULL) {
        return MYLITE_OK;
    }
    if (selected_schema == NULL || selected_schema[0] == '\0') {
        (void)mylite_diagnostics_set_error_message(database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }

    plan->schema_name = mylite_copy_nonempty_cstring(selected_schema);
    if (plan->schema_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int validate_truncate_table_target(
    mylite_db *database,
    const struct mylite_truncate_table_plan *plan
) {
    struct mylite_schema_presence presence;
    bool exists = false;
    int status = mylite_catalog_schema_exists(database, plan->schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (presence.is_system) {
        (void)mylite_diagnostics_set_error_message_parts(
            database,
            "Access to system schema '",
            plan->schema_name,
            "' is rejected."
        );
        return MYLITE_EXEC_ERROR;
    }
    if (!presence.exists) {
        return mylite_diagnostics_set_table_doesnt_exist_error(
            database,
            plan->schema_name,
            plan->table_name
        );
    }

    status = mylite_catalog_table_exists(database, plan->schema_name, plan->table_name, &exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        return mylite_diagnostics_set_table_doesnt_exist_error(
            database,
            plan->schema_name,
            plan->table_name
        );
    }
    return MYLITE_OK;
}

static int truncate_table_transaction(
    mylite_db *database,
    const struct mylite_truncate_table_plan *plan
) {
    struct mylite_statement_atomicity atomicity = {0};
    int status = mylite_transaction_begin_statement_atomicity(database, &atomicity);

    if (status == MYLITE_OK) {
        status = delete_truncate_table_rows(database, plan);
    }
    if (status == MYLITE_OK) {
        status = reset_truncate_table_auto_increment(database, plan);
    }
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_statement_atomicity(database, &atomicity);
        if (status == MYLITE_OK) {
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_statement_atomicity(database, &atomicity);
    return status;
}

static int delete_truncate_table_rows(
    mylite_db *database,
    const struct mylite_truncate_table_plan *plan
) {
    char *physical_name = mylite_catalog_physical_table_name(plan->schema_name, plan->table_name);
    char *sql = NULL;
    int rc = SQLITE_OK;

    if (physical_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    sql = sqlite3_mprintf("DELETE FROM \"%w\"", physical_name);
    free(physical_name);
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int reset_truncate_table_auto_increment(
    mylite_db *database,
    const struct mylite_truncate_table_plan *plan
) {
    sqlite3_stmt *update = NULL;
    static const char sql[] = "UPDATE __mylite_table_catalog SET auto_increment = NULL "
                              "WHERE table_schema = ? AND table_name = ?";
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(update, 1, plan->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 2, plan->table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(update);
    sqlite3_finalize(update);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    // SQLite's public macro intentionally uses this sentinel pointer value.
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
