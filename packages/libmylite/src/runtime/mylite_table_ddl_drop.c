#include "mylite_table_ddl.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_transactions.h"
#include "sqlite3.h"

#include <mylite/mylite.h>

#include <stdlib.h>
#include <string.h>

static int validate_drop_table_plan(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_drop_table_plan *plan,
    bool if_exists
);

static int validate_drop_table_temporary_target(
    mylite_db *database,
    struct mylite_drop_table_target *target,
    bool if_exists
);

static int validate_drop_table_target(
    mylite_db *database,
    struct mylite_drop_table_target *target,
    bool if_exists
);

static bool drop_table_target_is_duplicate(
    const struct mylite_drop_table_plan *plan,
    size_t target_index
);

static int drop_table_transaction(mylite_db *database, const struct mylite_drop_table_plan *plan);

static int drop_physical_table(mylite_db *database, const struct mylite_drop_table_target *target);

static int set_unknown_table_error(
    mylite_db *database,
    const char *schema_name,
    const char *table_name
);

static int append_unknown_table_note(
    mylite_db *database,
    const char *schema_name,
    const char *table_name
);

int mylite_table_ddl_execute_drop_table_statement(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_drop_table_plan *plan,
    bool if_exists
) {
    int status = validate_drop_table_plan(database, selected_schema, plan, if_exists);

    if (status != MYLITE_OK) {
        return status;
    }

    return drop_table_transaction(database, plan);
}

static int validate_drop_table_plan(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_drop_table_plan *plan,
    bool if_exists
) {
    for (size_t index = 0U; index < plan->target_count; ++index) {
        struct mylite_drop_table_target *target = &plan->targets[index];

        if (target->schema_name == NULL) {
            if (selected_schema == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "No database selected");
                return MYLITE_EXEC_ERROR;
            }
            target->schema_name = mylite_copy_span_text(selected_schema, strlen(selected_schema));
            if (target->schema_name == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
        }

        if (drop_table_target_is_duplicate(plan, index)) {
            (void)mylite_diagnostics_set_error_message_parts(
                database,
                "Not unique table/alias: '",
                target->table_name,
                "'"
            );
            return MYLITE_EXEC_ERROR;
        }
    }

    if (plan->temporary) {
        for (size_t index = 0U; index < plan->target_count; ++index) {
            int status =
                validate_drop_table_temporary_target(database, &plan->targets[index], if_exists);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        return MYLITE_OK;
    }

    for (size_t index = 0U; index < plan->target_count; ++index) {
        struct mylite_drop_table_target *target = &plan->targets[index];
        int status = validate_drop_table_target(database, target, if_exists);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int validate_drop_table_temporary_target(
    mylite_db *database,
    struct mylite_drop_table_target *target,
    bool if_exists
) {
    struct mylite_schema_presence presence;
    bool exists = false;
    int status = mylite_catalog_schema_exists(database, target->schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (presence.is_system) {
        return mylite_diagnostics_set_schema_access_denied_error(database, target->schema_name);
    }
    if (!presence.exists) {
        if (if_exists) {
            return append_unknown_table_note(database, target->schema_name, target->table_name);
        }
        return set_unknown_table_error(database, target->schema_name, target->table_name);
    }

    status = mylite_catalog_temporary_table_exists(
        database,
        target->schema_name,
        target->table_name,
        &exists
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        if (if_exists) {
            return append_unknown_table_note(database, target->schema_name, target->table_name);
        }
        return set_unknown_table_error(database, target->schema_name, target->table_name);
    }
    target->exists = true;
    target->temporary = true;
    return MYLITE_OK;
}

static int validate_drop_table_target(
    mylite_db *database,
    struct mylite_drop_table_target *target,
    bool if_exists
) {
    struct mylite_schema_presence presence;
    bool exists = false;
    int status = mylite_catalog_schema_exists(database, target->schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (presence.is_system) {
        return mylite_diagnostics_set_schema_access_denied_error(database, target->schema_name);
    }
    if (!presence.exists) {
        if (if_exists) {
            return append_unknown_table_note(database, target->schema_name, target->table_name);
        }
        return set_unknown_table_error(database, target->schema_name, target->table_name);
    }

    status = mylite_catalog_temporary_table_exists(
        database,
        target->schema_name,
        target->table_name,
        &exists
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (exists) {
        target->exists = true;
        target->temporary = true;
        return MYLITE_OK;
    }

    status = mylite_catalog_persistent_table_exists(
        database,
        target->schema_name,
        target->table_name,
        &exists
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        if (if_exists) {
            return append_unknown_table_note(database, target->schema_name, target->table_name);
        }
        return set_unknown_table_error(database, target->schema_name, target->table_name);
    }
    target->exists = exists;
    target->temporary = false;
    return MYLITE_OK;
}

static bool drop_table_target_is_duplicate(
    const struct mylite_drop_table_plan *plan,
    size_t target_index
) {
    const struct mylite_drop_table_target *target = &plan->targets[target_index];

    for (size_t index = 0U; index < target_index; ++index) {
        if (strcmp(plan->targets[index].schema_name, target->schema_name) == 0 &&
            strcmp(plan->targets[index].table_name, target->table_name) == 0) {
            return true;
        }
    }
    return false;
}

static int drop_table_transaction(mylite_db *database, const struct mylite_drop_table_plan *plan) {
    int status = mylite_transaction_begin_storage(database);

    if (status != MYLITE_OK) {
        return status;
    }

    for (size_t index = 0U; index < plan->target_count && status == MYLITE_OK; ++index) {
        if (!plan->targets[index].exists) {
            continue;
        }
        status = drop_physical_table(database, &plan->targets[index]);
        if (status == MYLITE_OK) {
            unsigned int flags = MYLITE_CATALOG_DELETE_TABLE_INDEXES |
                                 MYLITE_CATALOG_DELETE_TABLE_COLUMNS |
                                 MYLITE_CATALOG_DELETE_TABLE_ROW;

            if (plan->targets[index].temporary) {
                status = mylite_catalog_delete_temporary_table_rows(
                    database,
                    plan->targets[index].schema_name,
                    plan->targets[index].table_name,
                    flags
                );
            } else {
                status = mylite_catalog_delete_table_rows(
                    database,
                    plan->targets[index].schema_name,
                    plan->targets[index].table_name,
                    flags
                );
            }
        }
    }

    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_storage(database);
        if (status == MYLITE_OK) {
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_storage(database);
    return status;
}

static int drop_physical_table(mylite_db *database, const struct mylite_drop_table_target *target) {
    char *physical_name =
        mylite_catalog_physical_table_name(target->schema_name, target->table_name);
    char *drop_sql = NULL;
    sqlite3_str *sql = NULL;
    int rc = SQLITE_OK;

    if (physical_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    sql = sqlite3_str_new(database->sqlite);
    if (sql == NULL) {
        free(physical_name);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    sqlite3_str_appendf(sql, "DROP TABLE \"%w\"", physical_name);
    free(physical_name);

    drop_sql = sqlite3_str_finish(sql);
    if (drop_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_exec(database->sqlite, drop_sql, NULL, NULL, NULL);
    sqlite3_free(drop_sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static int set_unknown_table_error(
    mylite_db *database,
    const char *schema_name,
    const char *table_name
) {
    char *message = sqlite3_mprintf("Unknown table '%q.%q'", schema_name, table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status =
            mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_BAD_TABLE_ERROR, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int append_unknown_table_note(
    mylite_db *database,
    const char *schema_name,
    const char *table_name
) {
    char *message = sqlite3_mprintf("Unknown table '%q.%q'", schema_name, table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_append_note(database, MYLITE_MYSQL_ER_BAD_TABLE_ERROR, message);
    sqlite3_free(message);
    return status;
}
