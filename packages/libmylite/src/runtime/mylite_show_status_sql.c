#include "mylite_show.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_show_types.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

static uint64_t show_status_uptime(const mylite_db *database);
static void append_show_status_row(sqlite3_str *sql, bool *first, const char *name,
                                   const char *value);
static void append_show_status_integer_row(sqlite3_str *sql, bool *first, const char *name,
                                           uint64_t value);

int mylite_show_status_sql(mylite_db *database, const struct mylite_show_status_query *query,
                           char **out_sql)
{
    static const char *const columns[] = {"Variable_name", "Value"};
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    uint64_t uptime = show_status_uptime(database);
    bool first = true;
    int status = MYLITE_OK;

    (void)query->scope;
    *out_sql = NULL;
    if (sql == NULL) {
        return MYLITE_NOMEM;
    }

    sqlite3_str_appendall(sql, "SELECT Variable_name, Value FROM (");
    append_show_status_row(sql, &first, "Com_begin", "0");
    append_show_status_row(sql, &first, "Com_commit", "0");
    append_show_status_row(sql, &first, "Com_create_db", "0");
    append_show_status_row(sql, &first, "Com_create_index", "0");
    append_show_status_row(sql, &first, "Com_create_table", "0");
    append_show_status_row(sql, &first, "Com_delete", "0");
    append_show_status_row(sql, &first, "Com_drop_db", "0");
    append_show_status_row(sql, &first, "Com_drop_index", "0");
    append_show_status_row(sql, &first, "Com_drop_table", "0");
    append_show_status_row(sql, &first, "Com_insert", "0");
    append_show_status_row(sql, &first, "Com_release_savepoint", "0");
    append_show_status_row(sql, &first, "Com_rename_table", "0");
    append_show_status_row(sql, &first, "Com_replace", "0");
    append_show_status_row(sql, &first, "Com_rollback", "0");
    append_show_status_row(sql, &first, "Com_rollback_to_savepoint", "0");
    append_show_status_row(sql, &first, "Com_savepoint", "0");
    append_show_status_row(sql, &first, "Com_select", "0");
    append_show_status_row(sql, &first, "Com_set_option", "0");
    append_show_status_row(sql, &first, "Com_show_errors", "0");
    append_show_status_row(sql, &first, "Com_show_fields", "0");
    append_show_status_row(sql, &first, "Com_show_keys", "0");
    append_show_status_row(sql, &first, "Com_show_status", "0");
    append_show_status_row(sql, &first, "Com_show_tables", "0");
    append_show_status_row(sql, &first, "Com_show_variables", "0");
    append_show_status_row(sql, &first, "Com_show_warnings", "0");
    append_show_status_row(sql, &first, "Com_truncate", "0");
    append_show_status_row(sql, &first, "Com_update", "0");
    append_show_status_row(sql, &first, "Connections", "1");
    append_show_status_row(sql, &first, "Questions", "0");
    append_show_status_row(sql, &first, "Threads_cached", "0");
    append_show_status_row(sql, &first, "Threads_connected", "1");
    append_show_status_row(sql, &first, "Threads_created", "1");
    append_show_status_row(sql, &first, "Threads_running", "1");
    append_show_status_integer_row(sql, &first, "Uptime", uptime);
    append_show_status_integer_row(sql, &first, "Uptime_since_flush_status", uptime);
    sqlite3_str_appendall(sql, ")");

    if (query->like_pattern != NULL) {
        sqlite3_str_appendf(sql, " WHERE Variable_name LIKE %Q ESCAPE '\\'", query->like_pattern);
    }
    if (query->where_expression != NULL) {
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
                database, "SHOW STATUS WHERE expression is not supported");
        }
        return status;
    }
    return *out_sql == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static uint64_t show_status_uptime(const mylite_db *database)
{
    time_t now = time(NULL);

    if (database == NULL || database->status_started_at == (time_t)-1 || now == (time_t)-1 ||
        now < database->status_started_at) {
        return 0U;
    }
    return (uint64_t)(now - database->status_started_at);
}

static void append_show_status_row(sqlite3_str *sql, bool *first, const char *name,
                                   const char *value)
{
    if (!*first) {
        sqlite3_str_appendall(sql, " UNION ALL ");
    }
    sqlite3_str_appendf(sql, "SELECT %Q AS \"Variable_name\", %Q AS \"Value\"", name,
                        value == NULL ? "" : value);
    *first = false;
}

static void append_show_status_integer_row(sqlite3_str *sql, bool *first, const char *name,
                                           uint64_t value)
{
    if (!*first) {
        sqlite3_str_appendall(sql, " UNION ALL ");
    }
    sqlite3_str_appendf(sql, "SELECT %Q AS \"Variable_name\", '%llu' AS \"Value\"", name,
                        (unsigned long long)value);
    *first = false;
}
