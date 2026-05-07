#include "mylite_show.h"

#include "mylite_diagnostics.h"
#include "mylite_field_descriptor.h"
#include "mylite_metadata.h"
#include "mylite_metadata_constants.h"
#include "mylite_runtime.h"
#include "mylite_show_create_common.h"
#include "mylite_show_create_table_checks.h"
#include "mylite_show_create_table_columns.h"
#include "mylite_show_create_table_foreign_keys.h"
#include "mylite_show_create_table_indexes.h"
#include "mylite_show_create_table_info.h"
#include "mylite_show_create_table_options.h"
#include "mylite_show_create_table_target.h"
#include "mylite_statement.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>

static int show_create_table_sql(
    mylite_db *database,
    const struct mylite_show_create_table_target *target,
    char **out_sql
);

static int attach_show_create_table_result_metadata(mylite_db *database, mylite_stmt *stmt);

static struct mylite_field_descriptor show_create_table_descriptor(uint64_t length);

int mylite_show_prepare_create_table_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    struct mylite_show_create_table_target target = {0};
    char *sqlite_sql = NULL;
    int status = mylite_show_create_table_copy_target(database, statement, &target);

    *out_stmt = NULL;
    if (status == MYLITE_OK) {
        status = mylite_show_create_table_validate_target(database, &target);
    }
    if (status == MYLITE_OK) {
        status = show_create_table_sql(database, &target, &sqlite_sql);
    }
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }
    if (status == MYLITE_OK) {
        status = attach_show_create_table_result_metadata(database, *out_stmt);
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    if (status != MYLITE_OK) {
        mylite_finalize(*out_stmt);
        *out_stmt = NULL;
    }
    mylite_show_create_table_target_deinit(&target);
    sqlite3_free(sqlite_sql);
    return status;
}

static int show_create_table_sql(
    mylite_db *database,
    const struct mylite_show_create_table_target *target,
    char **out_sql
) {
    struct mylite_show_create_table_info info = {0};
    sqlite3_str *create_sql = NULL;
    char *create_text = NULL;
    bool first_line = true;
    int status = mylite_show_create_table_read_info(database, target, &info);

    *out_sql = NULL;
    if (status != MYLITE_OK) {
        return status;
    }

    create_sql = sqlite3_str_new(database->sqlite);
    if (create_sql == NULL) {
        mylite_show_create_table_info_deinit(&info);
        return MYLITE_NOMEM;
    }

    if (target->temporary) {
        sqlite3_str_appendall(create_sql, "CREATE TEMPORARY TABLE ");
    } else {
        sqlite3_str_appendall(create_sql, "CREATE TABLE ");
    }
    mylite_show_create_append_identifier(create_sql, target->table_name);
    sqlite3_str_appendall(create_sql, " (\n");
    status =
        mylite_show_create_table_append_columns(database, create_sql, target, &info, &first_line);
    if (status == MYLITE_OK) {
        status = mylite_show_create_table_append_indexes(database, create_sql, target, &first_line);
    }
    if (status == MYLITE_OK) {
        status =
            mylite_show_create_table_append_foreign_keys(database, create_sql, target, &first_line);
    }
    if (status == MYLITE_OK) {
        status = mylite_show_create_table_append_checks(database, create_sql, target, &first_line);
    }
    if (status == MYLITE_OK) {
        sqlite3_str_appendall(create_sql, "\n)");
        mylite_show_create_table_append_options(create_sql, &info);
    }
    create_text = sqlite3_str_finish(create_sql);

    if (status == MYLITE_OK && create_text != NULL) {
        *out_sql = sqlite3_mprintf(
            "SELECT %Q AS \"Table\", %Q AS \"Create Table\"",
            target->table_name,
            create_text
        );
        if (*out_sql == NULL) {
            status = MYLITE_NOMEM;
        }
    } else if (status == MYLITE_OK) {
        status = MYLITE_NOMEM;
    }

    sqlite3_free(create_text);
    mylite_show_create_table_info_deinit(&info);
    return status;
}

static int attach_show_create_table_result_metadata(mylite_db *database, mylite_stmt *stmt) {
    const struct mylite_result_column_metadata_spec columns[] = {
        {.name = "Table", .descriptor = show_create_table_descriptor(64U)},
        {.name = "Create Table", .descriptor = show_create_table_descriptor(1024U)},
    };

    return mylite_result_metadata_attach_columns(
        database,
        stmt,
        columns,
        sizeof(columns) / sizeof(columns[0])
    );
}

static struct mylite_field_descriptor show_create_table_descriptor(uint64_t length) {
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = MYLITE_FIELD_FLAG_NOT_NULL,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_latin1_swedish_ci_charset_id,
        .nullable = false,
    };

    mylite_field_descriptor_set_nullable(&descriptor, false);
    return descriptor;
}
