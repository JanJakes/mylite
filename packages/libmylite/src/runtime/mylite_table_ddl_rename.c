#include "mylite_table_ddl.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_transactions.h"
#include "sqlite3.h"

#include <mylite/mylite.h>

#include <stdlib.h>
#include <string.h>

static int validate_rename_table_plan(mylite_db *database, const char *selected_schema,
                                      struct mylite_rename_table_plan *plan);
static int resolve_rename_table_names(mylite_db *database, const char *selected_schema,
                                      struct mylite_rename_table_plan *plan);
static int validate_rename_table_target_schemas(mylite_db *database,
                                                const struct mylite_rename_table_target *target);
static int validate_rename_table_target(mylite_db *database,
                                        const struct mylite_rename_table_plan *plan,
                                        size_t target_index);
static int simulated_rename_table_exists_before_target(mylite_db *database,
                                                       const struct mylite_rename_table_plan *plan,
                                                       const char *schema_name,
                                                       const char *table_name, size_t target_index,
                                                       bool *out_exists);
static bool rename_table_target_source_matches(const struct mylite_rename_table_target *target,
                                               const char *schema_name, const char *table_name);
static bool rename_table_target_destination_matches(const struct mylite_rename_table_target *target,
                                                    const char *schema_name,
                                                    const char *table_name);
static int rename_table_transaction(mylite_db *database,
                                    const struct mylite_rename_table_plan *plan);
static int rename_table_target(mylite_db *database,
                               const struct mylite_rename_table_target *target);
static int rename_physical_table(mylite_db *database,
                                 const struct mylite_rename_table_target *target);
static int rewrite_rename_table_catalog(mylite_db *database,
                                        const struct mylite_rename_table_target *target);
static int rewrite_rename_table_catalog_row(mylite_db *database, const char *sql,
                                            const struct mylite_rename_table_target *target);
static int rewrite_rename_table_index_catalog(mylite_db *database,
                                              const struct mylite_rename_table_target *target);
static int set_rename_table_exists_error(mylite_db *database, const char *schema_name,
                                         const char *table_name);
static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_table_ddl_execute_rename_table_statement(mylite_db *database,
                                                    const char *selected_schema,
                                                    struct mylite_rename_table_plan *plan)
{
    int status = validate_rename_table_plan(database, selected_schema, plan);

    if (status == MYLITE_OK) {
        status = rename_table_transaction(database, plan);
    }
    return status;
}

static int validate_rename_table_plan(mylite_db *database, const char *selected_schema,
                                      struct mylite_rename_table_plan *plan)
{
    int status = MYLITE_OK;

    if (plan->target_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    status = resolve_rename_table_names(database, selected_schema, plan);
    if (status != MYLITE_OK) {
        return status;
    }

    for (size_t index = 0U; index < plan->target_count; ++index) {
        status = validate_rename_table_target_schemas(database, &plan->targets[index]);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    for (size_t index = 0U; index < plan->target_count; ++index) {
        status = validate_rename_table_target(database, plan, index);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int resolve_rename_table_names(mylite_db *database, const char *selected_schema,
                                      struct mylite_rename_table_plan *plan)
{
    for (size_t index = 0U; index < plan->target_count; ++index) {
        struct mylite_rename_table_target *target = &plan->targets[index];

        if ((target->source_schema_name == NULL || target->target_schema_name == NULL) &&
            (selected_schema == NULL || selected_schema[0] == '\0')) {
            (void)mylite_diagnostics_set_error_message(database, "No database selected");
            return MYLITE_EXEC_ERROR;
        }
        if (target->source_schema_name == NULL) {
            target->source_schema_name = mylite_copy_nonempty_cstring(selected_schema);
            if (target->source_schema_name == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
        }
        if (target->target_schema_name == NULL) {
            target->target_schema_name = mylite_copy_nonempty_cstring(selected_schema);
            if (target->target_schema_name == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
        }
    }
    return MYLITE_OK;
}

static int validate_rename_table_target_schemas(mylite_db *database,
                                                const struct mylite_rename_table_target *target)
{
    struct mylite_schema_presence source_presence;
    struct mylite_schema_presence target_presence;
    int status =
        mylite_catalog_schema_exists(database, target->source_schema_name, &source_presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!source_presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(database, "Unknown database '",
                                                         target->source_schema_name, "'");
        return MYLITE_EXEC_ERROR;
    }
    if (source_presence.is_system) {
        (void)mylite_diagnostics_set_error_message_parts(
            database, "Access to system schema '", target->source_schema_name, "' is rejected.");
        return MYLITE_EXEC_ERROR;
    }

    status = mylite_catalog_schema_exists(database, target->target_schema_name, &target_presence);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!target_presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(database, "Unknown database '",
                                                         target->target_schema_name, "'");
        return MYLITE_EXEC_ERROR;
    }
    if (target_presence.is_system) {
        (void)mylite_diagnostics_set_error_message_parts(
            database, "Access to system schema '", target->target_schema_name, "' is rejected.");
        return MYLITE_EXEC_ERROR;
    }
    return MYLITE_OK;
}

static int validate_rename_table_target(mylite_db *database,
                                        const struct mylite_rename_table_plan *plan,
                                        size_t target_index)
{
    const struct mylite_rename_table_target *target = &plan->targets[target_index];
    bool source_exists = false;
    bool target_exists = false;
    int status = simulated_rename_table_exists_before_target(
        database, plan, target->source_schema_name, target->source_table_name, target_index,
        &source_exists);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!source_exists) {
        return mylite_diagnostics_set_table_doesnt_exist_error(database, target->source_schema_name,
                                                               target->source_table_name);
    }

    status = simulated_rename_table_exists_before_target(database, plan, target->target_schema_name,
                                                         target->target_table_name, target_index,
                                                         &target_exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (target_exists) {
        return set_rename_table_exists_error(database, target->target_schema_name,
                                             target->target_table_name);
    }
    return MYLITE_OK;
}

static int simulated_rename_table_exists_before_target(mylite_db *database,
                                                       const struct mylite_rename_table_plan *plan,
                                                       const char *schema_name,
                                                       const char *table_name, size_t target_index,
                                                       bool *out_exists)
{
    int status = mylite_catalog_table_exists(database, schema_name, table_name, out_exists);

    if (status != MYLITE_OK) {
        return status;
    }
    for (size_t index = 0U; index < target_index; ++index) {
        const struct mylite_rename_table_target *target = &plan->targets[index];

        if (rename_table_target_source_matches(target, schema_name, table_name)) {
            *out_exists = false;
        }
        if (rename_table_target_destination_matches(target, schema_name, table_name)) {
            *out_exists = true;
        }
    }
    return MYLITE_OK;
}

static bool rename_table_target_source_matches(const struct mylite_rename_table_target *target,
                                               const char *schema_name, const char *table_name)
{
    const int schema_comparison = strcmp(target->source_schema_name, schema_name);
    const int table_comparison = strcmp(target->source_table_name, table_name);

    if (schema_comparison != 0) {
        return false;
    }
    if (table_comparison != 0) {
        return false;
    }
    return true;
}

static bool rename_table_target_destination_matches(const struct mylite_rename_table_target *target,
                                                    const char *schema_name, const char *table_name)
{
    const int schema_comparison = strcmp(target->target_schema_name, schema_name);
    const int table_comparison = strcmp(target->target_table_name, table_name);

    if (schema_comparison != 0) {
        return false;
    }
    if (table_comparison != 0) {
        return false;
    }
    return true;
}

static int rename_table_transaction(mylite_db *database,
                                    const struct mylite_rename_table_plan *plan)
{
    struct mylite_statement_atomicity atomicity = {0};
    int status = mylite_transaction_begin_statement_atomicity(database, &atomicity);

    for (size_t index = 0U; status == MYLITE_OK && index < plan->target_count; ++index) {
        status = rename_table_target(database, &plan->targets[index]);
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

static int rename_table_target(mylite_db *database, const struct mylite_rename_table_target *target)
{
    int status = rename_physical_table(database, target);

    if (status == MYLITE_OK) {
        status = rewrite_rename_table_catalog(database, target);
    }
    return status;
}

static int rename_physical_table(mylite_db *database,
                                 const struct mylite_rename_table_target *target)
{
    char *source_physical_name =
        mylite_catalog_physical_table_name(target->source_schema_name, target->source_table_name);
    char *target_physical_name =
        mylite_catalog_physical_table_name(target->target_schema_name, target->target_table_name);
    char *sql = NULL;
    int rc = SQLITE_OK;

    if (source_physical_name == NULL || target_physical_name == NULL) {
        free(source_physical_name);
        free(target_physical_name);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    sql = sqlite3_mprintf("ALTER TABLE \"%w\" RENAME TO \"%w\"", source_physical_name,
                          target_physical_name);
    free(source_physical_name);
    free(target_physical_name);
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int rewrite_rename_table_catalog(mylite_db *database,
                                        const struct mylite_rename_table_target *target)
{
    static const char update_tables[] =
        "UPDATE __mylite_table_catalog SET table_schema = ?, table_name = ? "
        "WHERE table_schema = ? AND table_name = ?";
    static const char update_columns[] =
        "UPDATE __mylite_column_catalog SET table_schema = ?, table_name = ? "
        "WHERE table_schema = ? AND table_name = ?";
    int status = rewrite_rename_table_catalog_row(database, update_tables, target);

    if (status == MYLITE_OK) {
        status = rewrite_rename_table_catalog_row(database, update_columns, target);
    }
    if (status == MYLITE_OK) {
        status = rewrite_rename_table_index_catalog(database, target);
    }
    return status;
}

static int rewrite_rename_table_catalog_row(mylite_db *database, const char *sql,
                                            const struct mylite_rename_table_target *target)
{
    sqlite3_stmt *update = NULL;
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(update, 1, target->target_schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 2, target->target_table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 3, target->source_schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 4, target->source_table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(update);
    sqlite3_finalize(update);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int rewrite_rename_table_index_catalog(mylite_db *database,
                                              const struct mylite_rename_table_target *target)
{
    enum {
        bind_target_schema = 1,
        bind_target_table = 2,
        bind_target_index_schema = 3,
        bind_source_schema = 4,
        bind_source_table = 5,
    };
    sqlite3_stmt *update = NULL;
    static const char sql[] = "UPDATE __mylite_index_catalog "
                              "SET table_schema = ?, table_name = ?, index_schema = ? "
                              "WHERE table_schema = ? AND table_name = ?";
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(update, bind_target_schema, target->target_schema_name, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(update, bind_target_table, target->target_table_name, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(update, bind_target_index_schema, target->target_schema_name, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(update, bind_source_schema, target->source_schema_name, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(update, bind_source_table, target->source_table_name, -1,
                      sqlite_transient_destructor());
    rc = sqlite3_step(update);
    sqlite3_finalize(update);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int set_rename_table_exists_error(mylite_db *database, const char *schema_name,
                                         const char *table_name)
{
    char *message = sqlite3_mprintf("Table '%q.%q' already exists", schema_name, table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static sqlite3_destructor_type sqlite_transient_destructor(void)
{
    // SQLite's public macro intentionally uses this sentinel pointer value.
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
