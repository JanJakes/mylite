#include "mylite_show_create_table_foreign_keys.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_show_create_common.h"
#include "mylite_show_create_table_body.h"
#include "mylite_span.h"
#include "sqlite3.h"

static int append_show_create_table_foreign_key(
    mylite_db *database,
    sqlite3_str *create_sql,
    sqlite3_stmt *select,
    bool temporary
);

static int append_show_create_table_foreign_key_columns(
    mylite_db *database,
    sqlite3_str *create_sql,
    sqlite3_stmt *select,
    const char *constraint_name,
    bool referenced,
    bool temporary
);

static void append_show_create_table_reference_action(
    sqlite3_str *create_sql,
    const char *clause,
    const char *action
);

int mylite_show_create_table_append_foreign_keys(
    mylite_db *database,
    sqlite3_str *create_sql,
    const struct mylite_show_create_table_target *target,
    bool *first_line
) {
    sqlite3_stmt *select = NULL;
    char *sql = sqlite3_mprintf(
        "SELECT constraint_schema, table_name, constraint_name, referenced_table_schema, "
        "referenced_table_name, delete_rule, update_rule "
        "FROM %s "
        "WHERE table_schema = ? AND table_name = ? AND ordinal_position = 1 "
        "ORDER BY rowid",
        mylite_catalog_foreign_key_catalog_name(target->temporary)
    );
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(
        select,
        1,
        target->schema_name,
        -1,
        mylite_show_sqlite_transient_destructor()
    );
    sqlite3_bind_text(select, 2, target->table_name, -1, mylite_show_sqlite_transient_destructor());

    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        int status = MYLITE_OK;

        mylite_show_create_table_append_line_prefix(create_sql, first_line);
        status =
            append_show_create_table_foreign_key(database, create_sql, select, target->temporary);
        if (status != MYLITE_OK) {
            sqlite3_finalize(select);
            return status;
        }
    }

    sqlite3_finalize(select);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int append_show_create_table_foreign_key(
    mylite_db *database,
    sqlite3_str *create_sql,
    sqlite3_stmt *select,
    bool temporary
) {
    enum {
        constraint_name_column = 2,
        referenced_table_schema_column = 3,
        referenced_table_name_column = 4,
        delete_rule_column = 5,
        update_rule_column = 6,
    };

    const char *constraint_name = (const char *)sqlite3_column_text(select, constraint_name_column);
    const char *constraint_schema = (const char *)sqlite3_column_text(select, 0);
    const char *referenced_schema =
        (const char *)sqlite3_column_text(select, referenced_table_schema_column);
    const char *referenced_table =
        (const char *)sqlite3_column_text(select, referenced_table_name_column);
    const char *delete_rule = (const char *)sqlite3_column_text(select, delete_rule_column);
    const char *update_rule = (const char *)sqlite3_column_text(select, update_rule_column);
    int status = MYLITE_OK;

    sqlite3_str_appendall(create_sql, "CONSTRAINT ");
    mylite_show_create_append_identifier(create_sql, constraint_name);
    sqlite3_str_appendall(create_sql, " FOREIGN KEY (");
    status = append_show_create_table_foreign_key_columns(
        database,
        create_sql,
        select,
        constraint_name,
        false,
        temporary
    );
    if (status != MYLITE_OK) {
        return status;
    }
    sqlite3_str_appendall(create_sql, ") REFERENCES ");
    if (referenced_schema != NULL && constraint_schema != NULL &&
        !mylite_ascii_case_equal(referenced_schema, constraint_schema)) {
        mylite_show_create_append_identifier(create_sql, referenced_schema);
        sqlite3_str_appendall(create_sql, ".");
    }
    mylite_show_create_append_identifier(create_sql, referenced_table);
    sqlite3_str_appendall(create_sql, " (");
    status = append_show_create_table_foreign_key_columns(
        database,
        create_sql,
        select,
        constraint_name,
        true,
        temporary
    );
    if (status != MYLITE_OK) {
        return status;
    }
    sqlite3_str_appendall(create_sql, ")");
    append_show_create_table_reference_action(create_sql, " ON DELETE ", delete_rule);
    append_show_create_table_reference_action(create_sql, " ON UPDATE ", update_rule);
    return sqlite3_str_errcode(create_sql) == SQLITE_OK ? MYLITE_OK : MYLITE_NOMEM;
}

static int append_show_create_table_foreign_key_columns(
    mylite_db *database,
    sqlite3_str *create_sql,
    sqlite3_stmt *select,
    const char *constraint_name,
    bool referenced,
    bool temporary
) {
    sqlite3_stmt *columns = NULL;
    sqlite3 *sqlite = sqlite3_db_handle(select);
    char *sql = sqlite3_mprintf(
        "SELECT column_name, referenced_column_name FROM %s "
        "WHERE constraint_schema = ? AND table_name = ? AND constraint_name = ? "
        "ORDER BY ordinal_position",
        mylite_catalog_foreign_key_catalog_name(temporary)
    );
    const char *schema_name = (const char *)sqlite3_column_text(select, 0);
    const char *table_name = (const char *)sqlite3_column_text(select, 1);
    bool first = true;
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &columns, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(columns, 1, schema_name, -1, mylite_show_sqlite_transient_destructor());
    sqlite3_bind_text(columns, 2, table_name, -1, mylite_show_sqlite_transient_destructor());
    sqlite3_bind_text(columns, 3, constraint_name, -1, mylite_show_sqlite_transient_destructor());

    while ((rc = sqlite3_step(columns)) == SQLITE_ROW) {
        const char *column_name = (const char *)sqlite3_column_text(columns, referenced ? 1 : 0);

        if (!first) {
            sqlite3_str_appendall(create_sql, ",");
        }
        first = false;
        mylite_show_create_append_identifier(create_sql, column_name);
    }
    sqlite3_finalize(columns);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static void append_show_create_table_reference_action(
    sqlite3_str *create_sql,
    const char *clause,
    const char *action
) {
    if (action == NULL || mylite_ascii_case_equal(action, "NO ACTION")) {
        return;
    }
    sqlite3_str_appendall(create_sql, clause);
    sqlite3_str_appendall(create_sql, action);
}
