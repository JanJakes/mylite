#include "mylite_show.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_show_create_common.h"
#include "mylite_show_create_table_columns.h"
#include "mylite_show_create_table_indexes.h"
#include "mylite_show_create_table_info.h"
#include "mylite_show_create_table_options.h"
#include "mylite_show_create_table_target.h"
#include "mylite_statement.h"
#include "sqlite3.h"

#include <stdbool.h>

static int show_create_table_sql(mylite_db *database,
                                 const struct mylite_show_create_table_target *target,
                                 char **out_sql);

int mylite_show_prepare_create_table_statement(mylite_db *database,
                                               const struct mylite_sql_ast_node *statement,
                                               mylite_stmt **out_stmt)
{
    struct mylite_show_create_table_target target = {0};
    char *sqlite_sql = NULL;
    int status = mylite_show_create_table_copy_target(database, statement, &target);

    if (status == MYLITE_OK) {
        status = mylite_show_create_table_validate_target(database, &target);
    }
    if (status == MYLITE_OK) {
        status = show_create_table_sql(database, &target, &sqlite_sql);
    }
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    mylite_show_create_table_target_deinit(&target);
    sqlite3_free(sqlite_sql);
    return status;
}

static int show_create_table_sql(mylite_db *database,
                                 const struct mylite_show_create_table_target *target,
                                 char **out_sql)
{
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

    sqlite3_str_appendall(create_sql,
                          target->temporary ? "CREATE TEMPORARY TABLE " : "CREATE TABLE ");
    mylite_show_create_append_identifier(create_sql, target->table_name);
    sqlite3_str_appendall(create_sql, " (\n");
    status =
        mylite_show_create_table_append_columns(database, create_sql, target, &info, &first_line);
    if (status == MYLITE_OK) {
        status = mylite_show_create_table_append_indexes(database, create_sql, target, &first_line);
    }
    if (status == MYLITE_OK) {
        sqlite3_str_appendall(create_sql, "\n)");
        mylite_show_create_table_append_options(create_sql, &info);
    }
    create_text = sqlite3_str_finish(create_sql);

    if (status == MYLITE_OK && create_text != NULL) {
        *out_sql = sqlite3_mprintf("SELECT %Q AS \"Table\", %Q AS \"Create Table\"",
                                   target->table_name, create_text);
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
