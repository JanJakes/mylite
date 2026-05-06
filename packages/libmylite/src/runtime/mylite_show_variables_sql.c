#include "mylite_show.h"

#include "mylite_catalog.h"
#include "mylite_charset.h"
#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_show_types.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdio.h>

static void append_show_variable_row(sqlite3_str *sql, bool *first, const char *name,
                                     const char *value);

int mylite_show_variables_sql(mylite_db *database, const struct mylite_show_variables_query *query,
                              char **out_sql)
{
    static const char *const columns[] = {"Variable_name", "Value"};
    struct mylite_schema_default schema_default = {
        .character_set = mylite_charset_default_name(),
        .collation = mylite_charset_default_collation_name(),
    };
    sqlite3_str *sql = NULL;
    const char *character_set_client = mylite_charset_default_name();
    const char *character_set_connection = mylite_charset_default_name();
    const char *character_set_database = mylite_charset_default_name();
    const char *character_set_results = mylite_charset_default_name();
    const char *collation_connection = mylite_charset_default_collation_name();
    const char *collation_database = mylite_charset_default_collation_name();
    char group_concat_max_len[32] = {0};
    bool first = true;
    bool global = query->scope == MYLITE_SQL_AST_SHOW_VARIABLES_GLOBAL;
    const char *sql_mode = mylite_connection_default_sql_mode();
    int status = MYLITE_OK;

    *out_sql = NULL;
    if (!global) {
        status = mylite_catalog_selected_schema_default(database, &schema_default);
        if (status != MYLITE_OK) {
            return status;
        }
        character_set_client = database->character_set_client;
        character_set_connection = database->character_set_connection;
        character_set_database = schema_default.character_set;
        character_set_results = database->character_set_results;
        collation_connection = database->collation_connection;
        collation_database = schema_default.collation;
        sql_mode = mylite_connection_sql_mode(database);
    }
    snprintf(group_concat_max_len, sizeof(group_concat_max_len), "%llu",
             (unsigned long long)(global ? mylite_connection_default_group_concat_max_len()
                                         : mylite_connection_group_concat_max_len(database)));

    sql = sqlite3_str_new(database->sqlite);
    if (sql == NULL) {
        return MYLITE_NOMEM;
    }

    sqlite3_str_appendall(sql, "SELECT Variable_name, Value FROM (");
    append_show_variable_row(sql, &first, "autocommit", "ON");
    append_show_variable_row(sql, &first, "character_set_client", character_set_client);
    append_show_variable_row(sql, &first, "character_set_connection", character_set_connection);
    append_show_variable_row(sql, &first, "character_set_database", character_set_database);
    append_show_variable_row(sql, &first, "character_set_filesystem", "binary");
    append_show_variable_row(sql, &first, "character_set_results", character_set_results);
    append_show_variable_row(sql, &first, "character_set_server", mylite_charset_default_name());
    append_show_variable_row(sql, &first, "character_set_system", "utf8mb3");
    append_show_variable_row(sql, &first, "character_sets_dir", "");
    append_show_variable_row(sql, &first, "collation_connection", collation_connection);
    append_show_variable_row(sql, &first, "collation_database", collation_database);
    append_show_variable_row(sql, &first, "collation_server",
                             mylite_charset_default_collation_name());
    if (!global) {
        append_show_variable_row(sql, &first, "error_count", "0");
    }
    append_show_variable_row(sql, &first, "group_concat_max_len", group_concat_max_len);
    append_show_variable_row(sql, &first, "max_error_count", "1024");
    append_show_variable_row(sql, &first, "sql_mode", sql_mode);
    append_show_variable_row(sql, &first, "sql_notes", "ON");
    append_show_variable_row(sql, &first, "transaction_isolation", "REPEATABLE-READ");
    append_show_variable_row(sql, &first, "transaction_read_only", "OFF");
    append_show_variable_row(sql, &first, "version", mylite_version());
    append_show_variable_row(sql, &first, "version_comment", "MyLite");
    append_show_variable_row(sql, &first, "version_compile_machine", "");
    append_show_variable_row(sql, &first, "version_compile_os", "");
    append_show_variable_row(sql, &first, "version_compile_zlib", "");
    if (!global) {
        append_show_variable_row(sql, &first, "warning_count", "0");
    }
    sqlite3_str_appendall(sql, ")");

    if (query->like_pattern != NULL) {
        sqlite3_str_appendf(sql, " WHERE Variable_name LIKE %Q ESCAPE '\\'", query->like_pattern);
    }
    if (status == MYLITE_OK && query->where_expression != NULL) {
        sqlite3_str_appendall(sql, query->like_pattern == NULL ? " WHERE " : " AND ");
        status = mylite_show_append_where_expression(database, sql, query->where_expression,
                                                     columns, sizeof(columns) / sizeof(columns[0]));
    }
    sqlite3_str_appendall(sql,
                          " ORDER BY Variable_name COLLATE NOCASE, Variable_name COLLATE BINARY");

    *out_sql = sqlite3_str_finish(sql);
    if (status != MYLITE_OK) {
        sqlite3_free(*out_sql);
        *out_sql = NULL;
        if (status == MYLITE_UNSUPPORTED) {
            (void)mylite_diagnostics_set_error_message(
                database, "SHOW VARIABLES WHERE expression is not supported");
        }
        return status;
    }
    return *out_sql == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static void append_show_variable_row(sqlite3_str *sql, bool *first, const char *name,
                                     const char *value)
{
    if (!*first) {
        sqlite3_str_appendall(sql, " UNION ALL ");
    }
    sqlite3_str_appendf(sql, "SELECT %Q AS \"Variable_name\", %Q AS \"Value\"", name,
                        value == NULL ? "" : value);
    *first = false;
}
