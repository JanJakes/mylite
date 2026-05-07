#include "mylite_show.h"

#include "mylite_catalog.h"
#include "mylite_charset.h"
#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_metadata_constants.h"
#include "mylite_runtime.h"
#include "mylite_show_types.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdio.h>

static void append_show_variable_row(
    sqlite3_str *sql,
    bool *first,
    const char *name,
    const char *value
);

static const char *show_variable_bool(bool value);

int mylite_show_variables_sql(
    mylite_db *database,
    const struct mylite_show_variables_query *query,
    char **out_sql
) {
    enum { integer_buffer_size = 32 };

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
    const char *default_storage_engine = mylite_connection_default_storage_engine();
    const char *time_zone = mylite_connection_default_time_zone();
    char group_concat_max_len[integer_buffer_size] = {0};
    char last_insert_id[integer_buffer_size] = {0};
    char max_allowed_packet[integer_buffer_size] = {0};
    char max_connections[integer_buffer_size] = {0};
    char wait_timeout[integer_buffer_size] = {0};
    bool first = true;
    bool global = query->scope == MYLITE_SQL_AST_SHOW_VARIABLES_GLOBAL;
    bool foreign_key_checks = mylite_connection_default_foreign_key_checks();
    bool sql_log_bin = mylite_connection_default_sql_log_bin();
    bool sql_notes = mylite_connection_default_sql_notes();
    const char *sql_mode = mylite_connection_default_sql_mode();
    bool unique_checks = mylite_connection_default_unique_checks();
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
        default_storage_engine = mylite_connection_storage_engine(database);
        foreign_key_checks = mylite_connection_foreign_key_checks(database);
        sql_log_bin = mylite_connection_sql_log_bin(database);
        sql_notes = mylite_connection_sql_notes(database);
        sql_mode = mylite_connection_sql_mode(database);
        time_zone = mylite_connection_time_zone(database);
        unique_checks = mylite_connection_unique_checks(database);
    }
    snprintf(
        group_concat_max_len,
        sizeof(group_concat_max_len),
        "%llu",
        (unsigned long long)(global ? mylite_connection_default_group_concat_max_len()
                                    : mylite_connection_group_concat_max_len(database))
    );
    snprintf(
        last_insert_id,
        sizeof(last_insert_id),
        "%llu",
        (unsigned long long)(database == NULL ? 0U : database->last_insert_id)
    );
    snprintf(
        max_allowed_packet,
        sizeof(max_allowed_packet),
        "%llu",
        (unsigned long long)mylite_connection_default_max_allowed_packet()
    );
    snprintf(
        max_connections,
        sizeof(max_connections),
        "%llu",
        (unsigned long long)mylite_connection_default_max_connections()
    );
    snprintf(
        wait_timeout,
        sizeof(wait_timeout),
        "%llu",
        (unsigned long long)(global ? mylite_connection_default_wait_timeout()
                                    : mylite_connection_wait_timeout(database))
    );

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
    append_show_variable_row(
        sql,
        &first,
        "collation_server",
        mylite_charset_default_collation_name()
    );
    append_show_variable_row(sql, &first, "default_storage_engine", default_storage_engine);
    if (!global) {
        append_show_variable_row(sql, &first, "error_count", "0");
    }
    append_show_variable_row(
        sql,
        &first,
        "foreign_key_checks",
        show_variable_bool(foreign_key_checks)
    );
    append_show_variable_row(sql, &first, "gtid_purged", "");
    append_show_variable_row(sql, &first, "group_concat_max_len", group_concat_max_len);
    if (!global) {
        append_show_variable_row(sql, &first, "last_insert_id", last_insert_id);
    }
    append_show_variable_row(sql, &first, "log_bin", "OFF");
    append_show_variable_row(sql, &first, "log_bin_trust_function_creators", "OFF");
    append_show_variable_row(sql, &first, "lower_case_table_names", "0");
    append_show_variable_row(sql, &first, "max_allowed_packet", max_allowed_packet);
    append_show_variable_row(sql, &first, "max_connections", max_connections);
    append_show_variable_row(sql, &first, "max_error_count", "1024");
    if (!global) {
        append_show_variable_row(sql, &first, "sql_log_bin", show_variable_bool(sql_log_bin));
    }
    append_show_variable_row(sql, &first, "sql_mode", sql_mode);
    append_show_variable_row(sql, &first, "sql_notes", show_variable_bool(sql_notes));
    append_show_variable_row(sql, &first, "time_zone", time_zone);
    append_show_variable_row(sql, &first, "transaction_isolation", "REPEATABLE-READ");
    append_show_variable_row(sql, &first, "transaction_read_only", "OFF");
    append_show_variable_row(sql, &first, "unique_checks", show_variable_bool(unique_checks));
    append_show_variable_row(sql, &first, "version", mylite_mysql_compatibility_version);
    append_show_variable_row(sql, &first, "version_comment", "MyLite");
    append_show_variable_row(sql, &first, "version_compile_machine", "");
    append_show_variable_row(sql, &first, "version_compile_os", "");
    append_show_variable_row(sql, &first, "version_compile_zlib", "");
    if (!global) {
        append_show_variable_row(sql, &first, "warning_count", "0");
    }
    append_show_variable_row(sql, &first, "wait_timeout", wait_timeout);
    sqlite3_str_appendall(sql, ")");

    if (query->like_pattern != NULL) {
        sqlite3_str_appendf(sql, " WHERE Variable_name LIKE %Q", query->like_pattern);
        if (query->like_escape_backslash) {
            sqlite3_str_appendall(sql, " ESCAPE '\\'");
        }
    }
    if (status == MYLITE_OK && query->where_expression != NULL) {
        sqlite3_str_appendall(sql, query->like_pattern == NULL ? " WHERE " : " AND ");
        status = mylite_show_append_where_expression(
            database,
            sql,
            query->where_expression,
            columns,
            sizeof(columns) / sizeof(columns[0])
        );
    }
    sqlite3_str_appendall(
        sql,
        " ORDER BY Variable_name COLLATE NOCASE, Variable_name COLLATE BINARY"
    );

    *out_sql = sqlite3_str_finish(sql);
    if (status != MYLITE_OK) {
        sqlite3_free(*out_sql);
        *out_sql = NULL;
        if (status == MYLITE_UNSUPPORTED) {
            (void)mylite_diagnostics_set_error_message(
                database,
                "SHOW VARIABLES WHERE expression is not supported"
            );
        }
        return status;
    }
    return *out_sql == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static void append_show_variable_row(
    sqlite3_str *sql,
    bool *first,
    const char *name,
    const char *value
) {
    if (!*first) {
        sqlite3_str_appendall(sql, " UNION ALL ");
    }
    sqlite3_str_appendf(
        sql,
        "SELECT %Q AS \"Variable_name\", %Q AS \"Value\"",
        name,
        value == NULL ? "" : value
    );
    *first = false;
}

static const char *show_variable_bool(bool value) {
    if (value) {
        return "ON";
    }
    return "OFF";
}
