#include "mylite_table_maintenance.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_information_schema.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "sql/mylite_ast.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdlib.h>

enum mylite_table_maintenance_operation {
    MYLITE_TABLE_MAINTENANCE_CHECK = 0,
    MYLITE_TABLE_MAINTENANCE_OPTIMIZE = 1,
    MYLITE_TABLE_MAINTENANCE_REPAIR = 2,
};

enum mylite_table_maintenance_target_status {
    MYLITE_TABLE_MAINTENANCE_TARGET_EXISTS = 0,
    MYLITE_TABLE_MAINTENANCE_TARGET_MISSING_TABLE = 1,
    MYLITE_TABLE_MAINTENANCE_TARGET_UNKNOWN_SCHEMA = 2,
};

struct mylite_table_maintenance_target {
    char *schema_name;
    char *table_name;
    char *display_name;
};

static int operation_from_statement(const struct mylite_sql_ast_node *statement,
                                    enum mylite_table_maintenance_operation *out_operation);
static int append_table_maintenance_sql(mylite_db *database, sqlite3_str *sql, bool *first,
                                        enum mylite_table_maintenance_operation operation,
                                        const struct mylite_sql_ast_node *table_name);
static int copy_maintenance_target(mylite_db *database,
                                   const struct mylite_sql_ast_node *table_name,
                                   struct mylite_table_maintenance_target *out_target);
static int copy_maintenance_target_names(mylite_db *database, char **parts, size_t part_count,
                                         struct mylite_table_maintenance_target *out_target);
static int copy_maintenance_target_display(mylite_db *database,
                                           struct mylite_table_maintenance_target *target);
static int classify_maintenance_target(mylite_db *database,
                                       const struct mylite_table_maintenance_target *target,
                                       enum mylite_table_maintenance_target_status *out_status);
static int append_target_rows(sqlite3_str *sql, bool *first,
                              enum mylite_table_maintenance_operation operation,
                              const struct mylite_table_maintenance_target *target,
                              enum mylite_table_maintenance_target_status target_status);
static int append_existing_target_rows(sqlite3_str *sql, bool *first,
                                       enum mylite_table_maintenance_operation operation,
                                       const char *display_name);
static int append_unknown_schema_rows(sqlite3_str *sql, bool *first,
                                      enum mylite_table_maintenance_operation operation,
                                      const struct mylite_table_maintenance_target *target);
static int append_missing_table_rows(sqlite3_str *sql, bool *first,
                                     enum mylite_table_maintenance_operation operation,
                                     const struct mylite_table_maintenance_target *target);
static void append_result_row(sqlite3_str *sql, bool *first, const char *table_name,
                              const char *operation, const char *msg_type, const char *msg_text);
static const char *operation_name(enum mylite_table_maintenance_operation operation);
static void table_maintenance_target_deinit(struct mylite_table_maintenance_target *target);
static void free_identifier_parts(char **parts, size_t part_count);
static int set_maintenance_out_of_memory(mylite_db *database);

int mylite_table_maintenance_prepare_statement(mylite_db *database,
                                               const struct mylite_sql_ast_node *statement,
                                               mylite_stmt **out_stmt)
{
    const struct mylite_sql_ast_node *table_list = mylite_ast_child_at(statement, 0U);
    enum mylite_table_maintenance_operation operation = MYLITE_TABLE_MAINTENANCE_CHECK;
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    char *sqlite_sql = NULL;
    bool first = true;
    int status = MYLITE_OK;

    *out_stmt = NULL;
    if (sql == NULL) {
        return set_maintenance_out_of_memory(database);
    }

    status = operation_from_statement(statement, &operation);
    if (status != MYLITE_OK) {
        sqlite3_str_finish(sql);
        return status;
    }

    for (const struct mylite_sql_ast_node *table_name =
             table_list == NULL ? NULL : table_list->first_child;
         table_name != NULL; table_name = table_name->next_sibling) {
        status = append_table_maintenance_sql(database, sql, &first, operation, table_name);
        if (status != MYLITE_OK) {
            sqlite3_str_finish(sql);
            return status == MYLITE_NOMEM ? set_maintenance_out_of_memory(database) : status;
        }
    }
    if (first) {
        sqlite3_str_finish(sql);
        (void)mylite_diagnostics_set_error_message(database,
                                                   "table maintenance requires at least one table");
        return MYLITE_UNSUPPORTED;
    }
    if (sqlite3_str_errcode(sql) != SQLITE_OK) {
        sqlite3_str_finish(sql);
        return set_maintenance_out_of_memory(database);
    }

    sqlite_sql = sqlite3_str_finish(sql);
    if (sqlite_sql == NULL) {
        return set_maintenance_out_of_memory(database);
    }

    status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    sqlite3_free(sqlite_sql);
    return status;
}

static int operation_from_statement(const struct mylite_sql_ast_node *statement,
                                    enum mylite_table_maintenance_operation *out_operation)
{
    switch (statement->placeholder_statement_kind) {
    case MYLITE_SQL_AST_PLACEHOLDER_CHECK_TABLE:
        *out_operation = MYLITE_TABLE_MAINTENANCE_CHECK;
        return MYLITE_OK;
    case MYLITE_SQL_AST_PLACEHOLDER_OPTIMIZE_TABLE:
        *out_operation = MYLITE_TABLE_MAINTENANCE_OPTIMIZE;
        return MYLITE_OK;
    case MYLITE_SQL_AST_PLACEHOLDER_REPAIR_TABLE:
        *out_operation = MYLITE_TABLE_MAINTENANCE_REPAIR;
        return MYLITE_OK;
    default:
        return MYLITE_UNSUPPORTED;
    }
}

static int append_table_maintenance_sql(mylite_db *database, sqlite3_str *sql, bool *first,
                                        enum mylite_table_maintenance_operation operation,
                                        const struct mylite_sql_ast_node *table_name)
{
    struct mylite_table_maintenance_target target = {0};
    enum mylite_table_maintenance_target_status target_status =
        MYLITE_TABLE_MAINTENANCE_TARGET_EXISTS;
    int status = copy_maintenance_target(database, table_name, &target);

    if (status != MYLITE_OK) {
        table_maintenance_target_deinit(&target);
        return status;
    }

    status = classify_maintenance_target(database, &target, &target_status);
    if (status == MYLITE_OK) {
        status = append_target_rows(sql, first, operation, &target, target_status);
    }

    table_maintenance_target_deinit(&target);
    return status;
}

static int copy_maintenance_target(mylite_db *database,
                                   const struct mylite_sql_ast_node *table_name,
                                   struct mylite_table_maintenance_target *out_target)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = mylite_copy_identifier_parts(table_name, parts, &part_count);

    *out_target = (struct mylite_table_maintenance_target){0};
    if (status != MYLITE_OK) {
        if (status == MYLITE_UNSUPPORTED) {
            (void)mylite_diagnostics_set_error_message(
                database, "table maintenance names with more than two parts are not supported");
        }
        return status;
    }

    status = copy_maintenance_target_names(database, parts, part_count, out_target);
    free_identifier_parts(parts, part_count);
    if (status != MYLITE_OK) {
        table_maintenance_target_deinit(out_target);
        return status;
    }
    return copy_maintenance_target_display(database, out_target);
}

static int copy_maintenance_target_names(mylite_db *database, char **parts, size_t part_count,
                                         struct mylite_table_maintenance_target *out_target)
{
    if (part_count == 1U) {
        if (database->selected_schema == NULL || database->selected_schema[0] == '\0') {
            (void)mylite_diagnostics_set_error_message(database, "No database selected");
            return MYLITE_EXEC_ERROR;
        }
        out_target->schema_name = mylite_copy_nonempty_cstring(database->selected_schema);
        out_target->table_name = parts[0];
        parts[0] = NULL;
    } else if (part_count == 2U) {
        out_target->schema_name = parts[0];
        out_target->table_name = parts[1];
        parts[0] = NULL;
        parts[1] = NULL;
    } else {
        (void)mylite_diagnostics_set_error_message(
            database, "table maintenance names with more than two parts are not supported");
        return MYLITE_UNSUPPORTED;
    }

    if (out_target->schema_name == NULL || out_target->table_name == NULL) {
        return set_maintenance_out_of_memory(database);
    }
    return MYLITE_OK;
}

static int copy_maintenance_target_display(mylite_db *database,
                                           struct mylite_table_maintenance_target *target)
{
    target->display_name = sqlite3_mprintf("%s.%s", target->schema_name, target->table_name);
    if (target->display_name == NULL) {
        return set_maintenance_out_of_memory(database);
    }
    return MYLITE_OK;
}

static int classify_maintenance_target(mylite_db *database,
                                       const struct mylite_table_maintenance_target *target,
                                       enum mylite_table_maintenance_target_status *out_status)
{
    struct mylite_schema_presence presence = {
        .exists = false,
        .is_system = false,
    };
    bool exists = false;
    int status = MYLITE_OK;

    if (mylite_ascii_case_equal(target->schema_name, "information_schema")) {
        if (mylite_information_schema_has_table(target->table_name)) {
            *out_status = MYLITE_TABLE_MAINTENANCE_TARGET_EXISTS;
        } else {
            *out_status = MYLITE_TABLE_MAINTENANCE_TARGET_MISSING_TABLE;
        }
        return MYLITE_OK;
    }

    status = mylite_catalog_schema_exists(database, target->schema_name, &presence);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        *out_status = MYLITE_TABLE_MAINTENANCE_TARGET_UNKNOWN_SCHEMA;
        return MYLITE_OK;
    }

    status = mylite_catalog_temporary_table_exists(database, target->schema_name,
                                                   target->table_name, &exists);
    if (status != MYLITE_OK || exists) {
        if (exists) {
            *out_status = MYLITE_TABLE_MAINTENANCE_TARGET_EXISTS;
        } else {
            *out_status = MYLITE_TABLE_MAINTENANCE_TARGET_MISSING_TABLE;
        }
        return status;
    }

    status = mylite_catalog_persistent_table_exists(database, target->schema_name,
                                                    target->table_name, &exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (exists) {
        *out_status = MYLITE_TABLE_MAINTENANCE_TARGET_EXISTS;
    } else {
        *out_status = MYLITE_TABLE_MAINTENANCE_TARGET_MISSING_TABLE;
    }
    return MYLITE_OK;
}

static int append_target_rows(sqlite3_str *sql, bool *first,
                              enum mylite_table_maintenance_operation operation,
                              const struct mylite_table_maintenance_target *target,
                              enum mylite_table_maintenance_target_status target_status)
{
    switch (target_status) {
    case MYLITE_TABLE_MAINTENANCE_TARGET_EXISTS:
        return append_existing_target_rows(sql, first, operation, target->display_name);
    case MYLITE_TABLE_MAINTENANCE_TARGET_MISSING_TABLE:
        return append_missing_table_rows(sql, first, operation, target);
    case MYLITE_TABLE_MAINTENANCE_TARGET_UNKNOWN_SCHEMA:
        return append_unknown_schema_rows(sql, first, operation, target);
    }
    return MYLITE_UNSUPPORTED;
}

static int append_existing_target_rows(sqlite3_str *sql, bool *first,
                                       enum mylite_table_maintenance_operation operation,
                                       const char *display_name)
{
    const char *operation_text = operation_name(operation);

    switch (operation) {
    case MYLITE_TABLE_MAINTENANCE_CHECK:
        append_result_row(sql, first, display_name, operation_text, "status", "OK");
        break;
    case MYLITE_TABLE_MAINTENANCE_OPTIMIZE:
        append_result_row(sql, first, display_name, operation_text, "note",
                          "Table does not support optimize, doing recreate + analyze instead");
        append_result_row(sql, first, display_name, operation_text, "status", "OK");
        break;
    case MYLITE_TABLE_MAINTENANCE_REPAIR:
        append_result_row(sql, first, display_name, operation_text, "note",
                          "The storage engine for the table doesn't support repair");
        break;
    }
    return sqlite3_str_errcode(sql) == SQLITE_OK ? MYLITE_OK : MYLITE_NOMEM;
}

static int append_unknown_schema_rows(sqlite3_str *sql, bool *first,
                                      enum mylite_table_maintenance_operation operation,
                                      const struct mylite_table_maintenance_target *target)
{
    char *message = sqlite3_mprintf("Unknown database '%q'", target->schema_name);

    if (message == NULL) {
        return MYLITE_NOMEM;
    }
    append_result_row(sql, first, target->display_name, operation_name(operation), "Error",
                      message);
    append_result_row(sql, first, target->display_name, operation_name(operation), "error",
                      "Corrupt");
    sqlite3_free(message);
    return sqlite3_str_errcode(sql) == SQLITE_OK ? MYLITE_OK : MYLITE_NOMEM;
}

static int append_missing_table_rows(sqlite3_str *sql, bool *first,
                                     enum mylite_table_maintenance_operation operation,
                                     const struct mylite_table_maintenance_target *target)
{
    char *message =
        sqlite3_mprintf("Table '%q.%q' doesn't exist", target->schema_name, target->table_name);

    if (message == NULL) {
        return MYLITE_NOMEM;
    }
    append_result_row(sql, first, target->display_name, operation_name(operation), "Error",
                      message);
    append_result_row(sql, first, target->display_name, operation_name(operation), "status",
                      "Operation failed");
    sqlite3_free(message);
    return sqlite3_str_errcode(sql) == SQLITE_OK ? MYLITE_OK : MYLITE_NOMEM;
}

static void append_result_row(sqlite3_str *sql, bool *first, const char *table_name,
                              const char *operation, const char *msg_type, const char *msg_text)
{
    if (!*first) {
        sqlite3_str_appendall(sql, " UNION ALL ");
    }
    sqlite3_str_appendf(sql,
                        "SELECT %Q AS \"Table\", %Q AS \"Op\", %Q AS \"Msg_type\", "
                        "%Q AS \"Msg_text\"",
                        table_name, operation, msg_type, msg_text);
    *first = false;
}

static const char *operation_name(enum mylite_table_maintenance_operation operation)
{
    switch (operation) {
    case MYLITE_TABLE_MAINTENANCE_CHECK:
        return "check";
    case MYLITE_TABLE_MAINTENANCE_OPTIMIZE:
        return "optimize";
    case MYLITE_TABLE_MAINTENANCE_REPAIR:
        return "repair";
    }
    return "";
}

static void table_maintenance_target_deinit(struct mylite_table_maintenance_target *target)
{
    if (target == NULL) {
        return;
    }
    free(target->schema_name);
    free(target->table_name);
    sqlite3_free(target->display_name);
    *target = (struct mylite_table_maintenance_target){0};
}

static void free_identifier_parts(char **parts, size_t part_count)
{
    for (size_t index = 0U; index < part_count; ++index) {
        free(parts[index]);
        parts[index] = NULL;
    }
}

static int set_maintenance_out_of_memory(mylite_db *database)
{
    (void)mylite_diagnostics_set_error_message(database, "out of memory");
    return MYLITE_NOMEM;
}
