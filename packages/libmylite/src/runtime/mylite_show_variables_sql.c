#include "mylite_show.h"

#include "mylite_catalog.h"
#include "mylite_charset.h"
#include "mylite_runtime.h"
#include "mylite_show_types.h"
#include "sqlite3.h"

#include <stdbool.h>

static void append_show_variable_row(sqlite3_str *sql, bool *first, const char *name,
                                     const char *value);

int mylite_show_variables_sql(mylite_db *database, const struct mylite_show_variables_query *query,
                              char **out_sql)
{
    static const char sql_mode[] =
        "ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,"
        "ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION";
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
    bool first = true;
    bool global = query->scope == MYLITE_SQL_AST_SHOW_VARIABLES_GLOBAL;
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
    }

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
    sqlite3_str_appendall(sql,
                          " ORDER BY Variable_name COLLATE NOCASE, Variable_name COLLATE BINARY");

    *out_sql = sqlite3_str_finish(sql);
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
