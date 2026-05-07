#include "mylite_table_maintenance.h"

#include <mylite/mylite.h>

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_field_descriptor.h"
#include "mylite_information_schema.h"
#include "mylite_metadata.h"
#include "mylite_metadata_constants.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "sql/mylite_ast.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

enum mylite_table_maintenance_operation {
    MYLITE_TABLE_MAINTENANCE_ANALYZE = 0,
    MYLITE_TABLE_MAINTENANCE_CHECK = 1,
    MYLITE_TABLE_MAINTENANCE_CHECKSUM = 2,
    MYLITE_TABLE_MAINTENANCE_CHECKSUM_QUICK = 3,
    MYLITE_TABLE_MAINTENANCE_OPTIMIZE = 4,
    MYLITE_TABLE_MAINTENANCE_REPAIR = 5,
};

enum mylite_table_maintenance_target_status {
    MYLITE_TABLE_MAINTENANCE_TARGET_EXISTS = 0,
    MYLITE_TABLE_MAINTENANCE_TARGET_MISSING_TABLE = 1,
    MYLITE_TABLE_MAINTENANCE_TARGET_UNKNOWN_SCHEMA = 2,
    MYLITE_TABLE_MAINTENANCE_TARGET_NON_BASE_TABLE = 3,
    MYLITE_TABLE_MAINTENANCE_TARGET_UNKNOWN_SYSTEM_TABLE = 4,
};

enum mylite_table_maintenance_metadata_length {
    MYLITE_CHECKSUM_TABLE_TABLE_LENGTH = 384,
    MYLITE_CHECKSUM_TABLE_CHECKSUM_LENGTH = 22,
};

struct mylite_table_maintenance_target {
    char *schema_name;
    char *table_name;
    char *display_name;
};

static int operation_from_statement(
    const struct mylite_sql_ast_node *statement,
    enum mylite_table_maintenance_operation *out_operation
);

static int append_table_maintenance_sql(
    mylite_db *database,
    sqlite3_str *sql,
    bool *first,
    enum mylite_table_maintenance_operation operation,
    const struct mylite_sql_ast_node *table_name
);

static int copy_maintenance_target(
    mylite_db *database,
    const struct mylite_sql_ast_node *table_name,
    struct mylite_table_maintenance_target *out_target
);

static int copy_maintenance_target_names(
    mylite_db *database,
    char **parts,
    size_t part_count,
    struct mylite_table_maintenance_target *out_target
);

static int copy_maintenance_target_display(
    mylite_db *database,
    struct mylite_table_maintenance_target *target
);

static int classify_maintenance_target(
    mylite_db *database,
    enum mylite_table_maintenance_operation operation,
    const struct mylite_table_maintenance_target *target,
    enum mylite_table_maintenance_target_status *out_status
);

static int append_target_rows(
    mylite_db *database,
    sqlite3_str *sql,
    bool *first,
    enum mylite_table_maintenance_operation operation,
    const struct mylite_table_maintenance_target *target,
    enum mylite_table_maintenance_target_status target_status
);

static int append_existing_target_rows(
    sqlite3_str *sql,
    bool *first,
    enum mylite_table_maintenance_operation operation,
    const char *display_name
);

static int append_unknown_schema_rows(
    sqlite3_str *sql,
    bool *first,
    enum mylite_table_maintenance_operation operation,
    const struct mylite_table_maintenance_target *target
);

static int append_missing_table_rows(
    sqlite3_str *sql,
    bool *first,
    enum mylite_table_maintenance_operation operation,
    const struct mylite_table_maintenance_target *target
);

static int append_checksum_target_row(
    mylite_db *database,
    sqlite3_str *sql,
    bool *first,
    enum mylite_table_maintenance_operation operation,
    const struct mylite_table_maintenance_target *target,
    enum mylite_table_maintenance_target_status target_status
);

static void append_result_row(
    sqlite3_str *sql,
    bool *first,
    const char *table_name,
    const char *operation,
    const char *msg_type,
    const char *msg_text
);

static void append_checksum_result_row(
    sqlite3_str *sql,
    bool *first,
    const char *table_name,
    bool has_checksum
);

static bool operation_is_checksum(enum mylite_table_maintenance_operation operation);

static const char *operation_name(enum mylite_table_maintenance_operation operation);

static int append_checksum_unknown_schema_warning(
    mylite_db *database,
    const struct mylite_table_maintenance_target *target
);

static int append_checksum_missing_table_warning(
    mylite_db *database,
    const struct mylite_table_maintenance_target *target
);

static int append_checksum_non_base_table_warning(
    mylite_db *database,
    const struct mylite_table_maintenance_target *target
);

static int set_unknown_information_schema_table_error(
    mylite_db *database,
    const struct mylite_table_maintenance_target *target
);

static void table_maintenance_target_deinit(struct mylite_table_maintenance_target *target);

static void free_identifier_parts(char **parts, size_t part_count);

static int set_maintenance_out_of_memory(mylite_db *database);

static int attach_table_maintenance_result_metadata(mylite_db *database, mylite_stmt *stmt);

static int attach_checksum_table_result_metadata(mylite_db *database, mylite_stmt *stmt);

static struct mylite_field_descriptor table_maintenance_field_descriptor(int type, uint64_t length);

static struct mylite_field_descriptor checksum_table_field_descriptor(void);

int mylite_table_maintenance_prepare_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
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
         table_name != NULL;
         table_name = table_name->next_sibling) {
        status = append_table_maintenance_sql(database, sql, &first, operation, table_name);
        if (status != MYLITE_OK) {
            sqlite3_str_finish(sql);
            return status == MYLITE_NOMEM ? set_maintenance_out_of_memory(database) : status;
        }
    }
    if (first) {
        sqlite3_str_finish(sql);
        (void)mylite_diagnostics_set_error_message(
            database,
            "table maintenance requires at least one table"
        );
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
    if (status == MYLITE_OK) {
        if (operation_is_checksum(operation)) {
            status = attach_checksum_table_result_metadata(database, *out_stmt);
        } else {
            status = attach_table_maintenance_result_metadata(database, *out_stmt);
        }
    }
    if (status == MYLITE_OK && operation_is_checksum(operation)) {
        (*out_stmt)->preserve_prepare_warnings = true;
    }
    if (status != MYLITE_OK) {
        mylite_finalize(*out_stmt);
        *out_stmt = NULL;
    }
    sqlite3_free(sqlite_sql);
    return status;
}

static int attach_table_maintenance_result_metadata(mylite_db *database, mylite_stmt *stmt) {
    const struct mylite_result_column_metadata_spec columns[] = {
        {.name = "Table",
         .descriptor = table_maintenance_field_descriptor(MYLITE_FIELD_TYPE_VAR_STRING, 128U)},
        {.name = "Op",
         .descriptor = table_maintenance_field_descriptor(MYLITE_FIELD_TYPE_VAR_STRING, 10U)},
        {.name = "Msg_type",
         .descriptor = table_maintenance_field_descriptor(MYLITE_FIELD_TYPE_VAR_STRING, 10U)},
        {.name = "Msg_text",
         .descriptor =
             table_maintenance_field_descriptor(MYLITE_FIELD_TYPE_MEDIUM_BLOB, UINT64_C(393216))},
    };

    return mylite_result_metadata_attach_columns(
        database,
        stmt,
        columns,
        sizeof(columns) / sizeof(columns[0])
    );
}

static int attach_checksum_table_result_metadata(mylite_db *database, mylite_stmt *stmt) {
    const struct mylite_result_column_metadata_spec columns[] = {
        {.name = "Table",
         .descriptor = table_maintenance_field_descriptor(
             MYLITE_FIELD_TYPE_VAR_STRING,
             MYLITE_CHECKSUM_TABLE_TABLE_LENGTH
         )},
        {.name = "Checksum", .descriptor = checksum_table_field_descriptor()},
    };

    return mylite_result_metadata_attach_columns(
        database,
        stmt,
        columns,
        sizeof(columns) / sizeof(columns[0])
    );
}

static struct mylite_field_descriptor table_maintenance_field_descriptor(
    int type,
    uint64_t length
) {
    struct mylite_field_descriptor descriptor = {
        .type = type,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_latin1_swedish_ci_charset_id,
        .nullable = true,
    };

    mylite_field_descriptor_set_nullable(&descriptor, true);
    return descriptor;
}

static struct mylite_field_descriptor checksum_table_field_descriptor(void) {
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_LONGLONG,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = MYLITE_CHECKSUM_TABLE_CHECKSUM_LENGTH,
        .decimals = 0U,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };

    mylite_field_descriptor_set_nullable(&descriptor, true);
    return descriptor;
}

static int operation_from_statement(
    const struct mylite_sql_ast_node *statement,
    enum mylite_table_maintenance_operation *out_operation
) {
    switch (statement->placeholder_statement_kind) {
    case MYLITE_SQL_AST_PLACEHOLDER_ANALYZE_TABLE:
        *out_operation = MYLITE_TABLE_MAINTENANCE_ANALYZE;
        return MYLITE_OK;
    case MYLITE_SQL_AST_PLACEHOLDER_CHECK_TABLE:
        *out_operation = MYLITE_TABLE_MAINTENANCE_CHECK;
        return MYLITE_OK;
    case MYLITE_SQL_AST_PLACEHOLDER_CHECKSUM_TABLE:
        *out_operation = MYLITE_TABLE_MAINTENANCE_CHECKSUM;
        return MYLITE_OK;
    case MYLITE_SQL_AST_PLACEHOLDER_CHECKSUM_TABLE_QUICK:
        *out_operation = MYLITE_TABLE_MAINTENANCE_CHECKSUM_QUICK;
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

static int append_table_maintenance_sql(
    mylite_db *database,
    sqlite3_str *sql,
    bool *first,
    enum mylite_table_maintenance_operation operation,
    const struct mylite_sql_ast_node *table_name
) {
    struct mylite_table_maintenance_target target = {0};
    enum mylite_table_maintenance_target_status target_status =
        MYLITE_TABLE_MAINTENANCE_TARGET_EXISTS;
    int status = copy_maintenance_target(database, table_name, &target);

    if (status != MYLITE_OK) {
        table_maintenance_target_deinit(&target);
        return status;
    }

    status = classify_maintenance_target(database, operation, &target, &target_status);
    if (status == MYLITE_OK) {
        status = append_target_rows(database, sql, first, operation, &target, target_status);
    }

    table_maintenance_target_deinit(&target);
    return status;
}

static int copy_maintenance_target(
    mylite_db *database,
    const struct mylite_sql_ast_node *table_name,
    struct mylite_table_maintenance_target *out_target
) {
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = mylite_copy_identifier_parts(table_name, parts, &part_count);

    *out_target = (struct mylite_table_maintenance_target){0};
    if (status != MYLITE_OK) {
        if (status == MYLITE_UNSUPPORTED) {
            (void)mylite_diagnostics_set_error_message(
                database,
                "table maintenance names with more than two parts are not supported"
            );
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

static int copy_maintenance_target_names(
    mylite_db *database,
    char **parts,
    size_t part_count,
    struct mylite_table_maintenance_target *out_target
) {
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
            database,
            "table maintenance names with more than two parts are not supported"
        );
        return MYLITE_UNSUPPORTED;
    }

    if (out_target->schema_name == NULL || out_target->table_name == NULL) {
        return set_maintenance_out_of_memory(database);
    }
    return MYLITE_OK;
}

static int copy_maintenance_target_display(
    mylite_db *database,
    struct mylite_table_maintenance_target *target
) {
    target->display_name = sqlite3_mprintf("%s.%s", target->schema_name, target->table_name);
    if (target->display_name == NULL) {
        return set_maintenance_out_of_memory(database);
    }
    return MYLITE_OK;
}

static int classify_maintenance_target(
    mylite_db *database,
    enum mylite_table_maintenance_operation operation,
    const struct mylite_table_maintenance_target *target,
    enum mylite_table_maintenance_target_status *out_status
) {
    struct mylite_schema_presence presence = {
        .exists = false,
        .is_system = false,
    };
    bool exists = false;
    int status = MYLITE_OK;

    if (mylite_ascii_case_equal(target->schema_name, "information_schema")) {
        if (operation == MYLITE_TABLE_MAINTENANCE_ANALYZE) {
            return mylite_diagnostics_set_schema_access_denied_error(database, target->schema_name);
        }
        if (operation_is_checksum(operation) &&
            mylite_information_schema_has_table(target->table_name)) {
            *out_status = MYLITE_TABLE_MAINTENANCE_TARGET_NON_BASE_TABLE;
        } else if (operation_is_checksum(operation)) {
            *out_status = MYLITE_TABLE_MAINTENANCE_TARGET_UNKNOWN_SYSTEM_TABLE;
        } else if (mylite_information_schema_has_table(target->table_name)) {
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

    status = mylite_catalog_temporary_table_exists(
        database,
        target->schema_name,
        target->table_name,
        &exists
    );
    if (status != MYLITE_OK || exists) {
        if (exists) {
            *out_status = MYLITE_TABLE_MAINTENANCE_TARGET_EXISTS;
        } else {
            *out_status = MYLITE_TABLE_MAINTENANCE_TARGET_MISSING_TABLE;
        }
        return status;
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
    if (exists) {
        *out_status = MYLITE_TABLE_MAINTENANCE_TARGET_EXISTS;
    } else {
        *out_status = MYLITE_TABLE_MAINTENANCE_TARGET_MISSING_TABLE;
    }
    return MYLITE_OK;
}

static int append_target_rows(
    mylite_db *database,
    sqlite3_str *sql,
    bool *first,
    enum mylite_table_maintenance_operation operation,
    const struct mylite_table_maintenance_target *target,
    enum mylite_table_maintenance_target_status target_status
) {
    if (operation_is_checksum(operation)) {
        return append_checksum_target_row(database, sql, first, operation, target, target_status);
    }

    switch (target_status) {
    case MYLITE_TABLE_MAINTENANCE_TARGET_EXISTS:
        return append_existing_target_rows(sql, first, operation, target->display_name);
    case MYLITE_TABLE_MAINTENANCE_TARGET_MISSING_TABLE:
        return append_missing_table_rows(sql, first, operation, target);
    case MYLITE_TABLE_MAINTENANCE_TARGET_UNKNOWN_SCHEMA:
        return append_unknown_schema_rows(sql, first, operation, target);
    case MYLITE_TABLE_MAINTENANCE_TARGET_NON_BASE_TABLE:
    case MYLITE_TABLE_MAINTENANCE_TARGET_UNKNOWN_SYSTEM_TABLE:
        break;
    }
    return MYLITE_UNSUPPORTED;
}

static int append_existing_target_rows(
    sqlite3_str *sql,
    bool *first,
    enum mylite_table_maintenance_operation operation,
    const char *display_name
) {
    const char *operation_text = operation_name(operation);

    switch (operation) {
    case MYLITE_TABLE_MAINTENANCE_ANALYZE:
    case MYLITE_TABLE_MAINTENANCE_CHECK:
        append_result_row(sql, first, display_name, operation_text, "status", "OK");
        break;
    case MYLITE_TABLE_MAINTENANCE_CHECKSUM:
    case MYLITE_TABLE_MAINTENANCE_CHECKSUM_QUICK:
        return MYLITE_UNSUPPORTED;
    case MYLITE_TABLE_MAINTENANCE_OPTIMIZE:
        append_result_row(
            sql,
            first,
            display_name,
            operation_text,
            "note",
            "Table does not support optimize, doing recreate + analyze instead"
        );
        append_result_row(sql, first, display_name, operation_text, "status", "OK");
        break;
    case MYLITE_TABLE_MAINTENANCE_REPAIR:
        append_result_row(
            sql,
            first,
            display_name,
            operation_text,
            "note",
            "The storage engine for the table doesn't support repair"
        );
        break;
    }
    return sqlite3_str_errcode(sql) == SQLITE_OK ? MYLITE_OK : MYLITE_NOMEM;
}

static int append_unknown_schema_rows(
    sqlite3_str *sql,
    bool *first,
    enum mylite_table_maintenance_operation operation,
    const struct mylite_table_maintenance_target *target
) {
    char *message = sqlite3_mprintf("Unknown database '%q'", target->schema_name);

    if (message == NULL) {
        return MYLITE_NOMEM;
    }
    append_result_row(
        sql,
        first,
        target->display_name,
        operation_name(operation),
        "Error",
        message
    );
    append_result_row(
        sql,
        first,
        target->display_name,
        operation_name(operation),
        "error",
        "Corrupt"
    );
    sqlite3_free(message);
    return sqlite3_str_errcode(sql) == SQLITE_OK ? MYLITE_OK : MYLITE_NOMEM;
}

static int append_missing_table_rows(
    sqlite3_str *sql,
    bool *first,
    enum mylite_table_maintenance_operation operation,
    const struct mylite_table_maintenance_target *target
) {
    char *message =
        sqlite3_mprintf("Table '%q.%q' doesn't exist", target->schema_name, target->table_name);

    if (message == NULL) {
        return MYLITE_NOMEM;
    }
    append_result_row(
        sql,
        first,
        target->display_name,
        operation_name(operation),
        "Error",
        message
    );
    append_result_row(
        sql,
        first,
        target->display_name,
        operation_name(operation),
        "status",
        "Operation failed"
    );
    sqlite3_free(message);
    return sqlite3_str_errcode(sql) == SQLITE_OK ? MYLITE_OK : MYLITE_NOMEM;
}

static int append_checksum_target_row(
    mylite_db *database,
    sqlite3_str *sql,
    bool *first,
    enum mylite_table_maintenance_operation operation,
    const struct mylite_table_maintenance_target *target,
    enum mylite_table_maintenance_target_status target_status
) {
    int status = MYLITE_OK;

    switch (target_status) {
    case MYLITE_TABLE_MAINTENANCE_TARGET_EXISTS:
        append_checksum_result_row(
            sql,
            first,
            target->display_name,
            operation != MYLITE_TABLE_MAINTENANCE_CHECKSUM_QUICK
        );
        break;
    case MYLITE_TABLE_MAINTENANCE_TARGET_MISSING_TABLE:
        append_checksum_result_row(sql, first, target->display_name, false);
        status = append_checksum_missing_table_warning(database, target);
        break;
    case MYLITE_TABLE_MAINTENANCE_TARGET_UNKNOWN_SCHEMA:
        append_checksum_result_row(sql, first, target->display_name, false);
        status = append_checksum_unknown_schema_warning(database, target);
        break;
    case MYLITE_TABLE_MAINTENANCE_TARGET_NON_BASE_TABLE:
        append_checksum_result_row(sql, first, target->display_name, false);
        status = append_checksum_non_base_table_warning(database, target);
        break;
    case MYLITE_TABLE_MAINTENANCE_TARGET_UNKNOWN_SYSTEM_TABLE:
        return set_unknown_information_schema_table_error(database, target);
    }
    if (status != MYLITE_OK) {
        return status;
    }
    return sqlite3_str_errcode(sql) == SQLITE_OK ? MYLITE_OK : MYLITE_NOMEM;
}

static void append_result_row(
    sqlite3_str *sql,
    bool *first,
    const char *table_name,
    const char *operation,
    const char *msg_type,
    const char *msg_text
) {
    if (!*first) {
        sqlite3_str_appendall(sql, " UNION ALL ");
    }
    sqlite3_str_appendf(
        sql,
        "SELECT %Q AS \"Table\", %Q AS \"Op\", %Q AS \"Msg_type\", "
        "%Q AS \"Msg_text\"",
        table_name,
        operation,
        msg_type,
        msg_text
    );
    *first = false;
}

static void append_checksum_result_row(
    sqlite3_str *sql,
    bool *first,
    const char *table_name,
    bool has_checksum
) {
    const char *checksum_sql = "NULL";

    if (!*first) {
        sqlite3_str_appendall(sql, " UNION ALL ");
    }
    if ((int)has_checksum != 0) {
        checksum_sql = "0";
    }
    sqlite3_str_appendf(
        sql,
        "SELECT %Q AS \"Table\", %s AS \"Checksum\"",
        table_name,
        checksum_sql
    );
    *first = false;
}

static bool operation_is_checksum(enum mylite_table_maintenance_operation operation) {
    if (operation == MYLITE_TABLE_MAINTENANCE_CHECKSUM) {
        return true;
    }
    return operation == MYLITE_TABLE_MAINTENANCE_CHECKSUM_QUICK;
}

static const char *operation_name(enum mylite_table_maintenance_operation operation) {
    switch (operation) {
    case MYLITE_TABLE_MAINTENANCE_ANALYZE:
        return "analyze";
    case MYLITE_TABLE_MAINTENANCE_CHECK:
        return "check";
    case MYLITE_TABLE_MAINTENANCE_CHECKSUM:
    case MYLITE_TABLE_MAINTENANCE_CHECKSUM_QUICK:
        return "checksum";
    case MYLITE_TABLE_MAINTENANCE_OPTIMIZE:
        return "optimize";
    case MYLITE_TABLE_MAINTENANCE_REPAIR:
        return "repair";
    }
    return "";
}

static int append_checksum_unknown_schema_warning(
    mylite_db *database,
    const struct mylite_table_maintenance_target *target
) {
    char *message = sqlite3_mprintf("Unknown database '%q'", target->schema_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_BAD_DB_ERROR, message);
    sqlite3_free(message);
    return status;
}

static int append_checksum_missing_table_warning(
    mylite_db *database,
    const struct mylite_table_maintenance_target *target
) {
    char *message =
        sqlite3_mprintf("Table '%q.%q' doesn't exist", target->schema_name, target->table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_NO_SUCH_TABLE, message);
    sqlite3_free(message);
    return status;
}

static int append_checksum_non_base_table_warning(
    mylite_db *database,
    const struct mylite_table_maintenance_target *target
) {
    char *message =
        sqlite3_mprintf("'%q.%q' is not BASE TABLE", target->schema_name, target->table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_WRONG_OBJECT, message);
    sqlite3_free(message);
    return status;
}

static int set_unknown_information_schema_table_error(
    mylite_db *database,
    const struct mylite_table_maintenance_target *target
) {
    char *message = sqlite3_mprintf("Unknown table '%q' in information_schema", target->table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_UNKNOWN_TABLE, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static void table_maintenance_target_deinit(struct mylite_table_maintenance_target *target) {
    if (target == NULL) {
        return;
    }
    free(target->schema_name);
    free(target->table_name);
    sqlite3_free(target->display_name);
    *target = (struct mylite_table_maintenance_target){0};
}

static void free_identifier_parts(char **parts, size_t part_count) {
    for (size_t index = 0U; index < part_count; ++index) {
        free(parts[index]);
        parts[index] = NULL;
    }
}

static int set_maintenance_out_of_memory(mylite_db *database) {
    (void)mylite_diagnostics_set_error_message(database, "out of memory");
    return MYLITE_NOMEM;
}
