#include "mylite_show.h"

#include "mylite_charset.h"
#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_field_descriptor.h"
#include "mylite_metadata.h"
#include "mylite_runtime.h"
#include "mylite_show_types.h"
#include "sql/mylite_lexer.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

static int storage_engines_sql(mylite_db *database,
                               const struct mylite_storage_engine_columns *columns, char **out_sql);
static uint64_t show_status_uptime(const mylite_db *database);
static void append_show_variable_row(sqlite3_str *sql, bool *first, const char *name,
                                     const char *value);
static void append_show_status_row(sqlite3_str *sql, bool *first, const char *name,
                                   const char *value);
static void append_show_status_integer_row(sqlite3_str *sql, bool *first, const char *name,
                                           uint64_t value);
static struct mylite_field_descriptor show_engines_field_descriptor(uint64_t length, bool nullable);
static void append_show_character_set_row(sqlite3_str *sql, bool *first,
                                          const struct mylite_charset *character_set);
static void append_information_schema_character_set_row(sqlite3_str *sql, bool *first,
                                                        const struct mylite_charset *character_set);
static void append_show_collation_row(sqlite3_str *sql, bool *first,
                                      const struct mylite_collation *collation);
static void append_information_schema_collation_row(sqlite3_str *sql, bool *first,
                                                    const struct mylite_collation *collation);
static void append_information_schema_collation_character_set_applicability_row(
    sqlite3_str *sql, bool *first, const struct mylite_collation *collation);
static void append_information_schema_keyword_row(sqlite3_str *sql, bool *first, const char *word,
                                                  unsigned int flags);
static void append_storage_engine_row(sqlite3_str *sql, bool *first,
                                      const struct mylite_storage_engine_columns *columns,
                                      const struct mylite_storage_engine_row *engine);

static const unsigned int mylite_show_latin1_swedish_ci_charset_id = 8U;
static const unsigned int mylite_show_not_fixed_decimals = 31U;

static const char show_schemas_sql[] =
    "SELECT name AS \"Database\" FROM __mylite_schema_catalog ORDER BY name COLLATE BINARY";
static const char information_schema_schemata_sql[] =
    "SELECT 'def' AS CATALOG_NAME,"
    "name AS SCHEMA_NAME,"
    "default_character_set AS DEFAULT_CHARACTER_SET_NAME,"
    "default_collation AS DEFAULT_COLLATION_NAME,"
    "NULL AS SQL_PATH,"
    "CASE WHEN upper(default_encryption) = 'Y' THEN 'YES' ELSE 'NO' END AS DEFAULT_ENCRYPTION "
    "FROM __mylite_schema_catalog ORDER BY name COLLATE BINARY";

static const struct mylite_storage_engine_row mylite_storage_engine_registry[] = {
    {"InnoDB", "DEFAULT", "MyLite SQLite-backed transactional engine facade", "YES", "NO", "YES"},
    {"MEMORY", "NO", "In-memory tables are not supported by MyLite", NULL, NULL, NULL},
    {"MyISAM", "NO", "MyISAM tables are not supported by MyLite", NULL, NULL, NULL},
    {"FEDERATED", "NO", "Federated tables are not supported by MyLite", NULL, NULL, NULL},
    {"MRG_MYISAM", "NO", "Merge MyISAM tables are not supported by MyLite", NULL, NULL, NULL},
    {"BLACKHOLE", "NO", "Blackhole tables are not supported by MyLite", NULL, NULL, NULL},
    {"CSV", "NO", "CSV-backed tables are not supported by MyLite", NULL, NULL, NULL},
    {"ARCHIVE", "NO", "Archive tables are not supported by MyLite", NULL, NULL, NULL},
};

int mylite_show_engines_sql(mylite_db *database, char **out_sql)
{
    static const struct mylite_storage_engine_columns columns = {
        "Engine", "Support", "Comment", "Transactions", "XA", "Savepoints",
    };

    return storage_engines_sql(database, &columns, out_sql);
}

int mylite_show_information_schema_engines_sql(mylite_db *database, char **out_sql)
{
    static const struct mylite_storage_engine_columns columns = {
        "ENGINE", "SUPPORT", "COMMENT", "TRANSACTIONS", "XA", "SAVEPOINTS",
    };

    return storage_engines_sql(database, &columns, out_sql);
}

int mylite_show_character_set_sql(mylite_db *database,
                                  const struct mylite_show_character_set_query *query,
                                  char **out_sql)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    bool first = true;

    *out_sql = NULL;
    if (sql == NULL) {
        return MYLITE_NOMEM;
    }

    sqlite3_str_appendall(sql, "SELECT Charset, Description, \"Default collation\", Maxlen FROM (");
    for (size_t index = 0U; index < mylite_charset_count(); ++index) {
        append_show_character_set_row(sql, &first, mylite_charset_at(index));
    }
    sqlite3_str_appendall(sql, ")");

    if (query->like_pattern != NULL) {
        sqlite3_str_appendf(sql, " WHERE Charset LIKE %Q ESCAPE '\\'", query->like_pattern);
    }
    sqlite3_str_appendall(sql, " ORDER BY Charset COLLATE NOCASE, Charset COLLATE BINARY");

    *out_sql = sqlite3_str_finish(sql);
    return *out_sql == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

int mylite_show_information_schema_character_sets_sql(mylite_db *database, char **out_sql)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    bool first = true;

    *out_sql = NULL;
    if (sql == NULL) {
        return MYLITE_NOMEM;
    }

    sqlite3_str_appendall(sql, "SELECT CHARACTER_SET_NAME, DEFAULT_COLLATE_NAME, DESCRIPTION, "
                               "MAXLEN FROM (");
    for (size_t index = 0U; index < mylite_charset_count(); ++index) {
        append_information_schema_character_set_row(sql, &first, mylite_charset_at(index));
    }
    sqlite3_str_appendall(sql, ")");

    *out_sql = sqlite3_str_finish(sql);
    return *out_sql == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

char *mylite_show_columns_sql(mylite_db *database, const struct mylite_show_columns_query *query)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "SELECT column_name AS \"Field\", column_type AS \"Type\"");
    if (query->full) {
        sqlite3_str_appendf(sql, ", collation_name AS \"Collation\"");
    }
    sqlite3_str_appendf(sql, ", is_nullable AS \"Null\", column_key AS \"Key\", "
                             "column_default AS \"Default\", extra AS \"Extra\"");
    if (query->full) {
        sqlite3_str_appendf(sql, ", privileges AS \"Privileges\", column_comment AS \"Comment\"");
    }
    sqlite3_str_appendf(sql,
                        " FROM __mylite_column_catalog WHERE table_schema = %Q AND table_name = %Q",
                        query->schema_name, query->table_name);
    if (query->like_pattern != NULL) {
        sqlite3_str_appendf(sql, " AND column_name LIKE %Q ESCAPE '\\'", query->like_pattern);
    }
    sqlite3_str_appendf(sql, " ORDER BY ordinal_position");
    return sqlite3_str_finish(sql);
}

int mylite_show_collation_sql(mylite_db *database, const struct mylite_show_collation_query *query,
                              char **out_sql)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    bool first = true;

    *out_sql = NULL;
    if (sql == NULL) {
        return MYLITE_NOMEM;
    }

    sqlite3_str_appendall(sql, "SELECT Collation, Charset, Id, \"Default\", Compiled, Sortlen, "
                               "Pad_attribute FROM (");
    for (size_t index = 0U; index < mylite_collation_count(); ++index) {
        append_show_collation_row(sql, &first, mylite_collation_at(index));
    }
    sqlite3_str_appendall(sql, ")");

    if (query->like_pattern != NULL) {
        sqlite3_str_appendf(sql, " WHERE Collation LIKE %Q ESCAPE '\\'", query->like_pattern);
    }
    sqlite3_str_appendall(sql, " ORDER BY Collation COLLATE NOCASE, Collation COLLATE BINARY");

    *out_sql = sqlite3_str_finish(sql);
    return *out_sql == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

int mylite_show_information_schema_collations_sql(mylite_db *database, char **out_sql)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    bool first = true;

    *out_sql = NULL;
    if (sql == NULL) {
        return MYLITE_NOMEM;
    }

    sqlite3_str_appendall(sql, "SELECT COLLATION_NAME, CHARACTER_SET_NAME, ID, IS_DEFAULT, "
                               "IS_COMPILED, SORTLEN, PAD_ATTRIBUTE FROM (");
    for (size_t index = 0U; index < mylite_collation_count(); ++index) {
        append_information_schema_collation_row(sql, &first, mylite_collation_at(index));
    }
    sqlite3_str_appendall(sql, ") ORDER BY COLLATION_NAME COLLATE NOCASE, "
                               "COLLATION_NAME COLLATE BINARY");

    *out_sql = sqlite3_str_finish(sql);
    return *out_sql == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

int mylite_show_information_schema_collation_character_set_applicability_sql(mylite_db *database,
                                                                             char **out_sql)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    bool first = true;

    *out_sql = NULL;
    if (sql == NULL) {
        return MYLITE_NOMEM;
    }

    sqlite3_str_appendall(sql, "SELECT COLLATION_NAME, CHARACTER_SET_NAME FROM (");
    for (size_t index = 0U; index < mylite_collation_count(); ++index) {
        append_information_schema_collation_character_set_applicability_row(
            sql, &first, mylite_collation_at(index));
    }
    sqlite3_str_appendall(sql, ") ORDER BY COLLATION_NAME COLLATE NOCASE, "
                               "COLLATION_NAME COLLATE BINARY");

    *out_sql = sqlite3_str_finish(sql);
    return *out_sql == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

int mylite_show_information_schema_keywords_sql(mylite_db *database, char **out_sql)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    bool first = true;

    *out_sql = NULL;
    if (sql == NULL) {
        return MYLITE_NOMEM;
    }

    sqlite3_str_appendall(sql, "WITH keywords(WORD, RESERVED) AS (VALUES ");
    for (size_t index = 0U; index < mylite_sql_keyword_catalog_count(); ++index) {
        const char *word = NULL;
        unsigned int flags = 0U;

        if (mylite_sql_keyword_catalog_at(index, &word, &flags)) {
            append_information_schema_keyword_row(sql, &first, word, flags);
        }
    }
    sqlite3_str_appendall(sql, ") SELECT WORD, RESERVED FROM keywords");

    *out_sql = sqlite3_str_finish(sql);
    return *out_sql == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

char *mylite_show_index_sql(mylite_db *database, const struct mylite_show_index_query *query)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql,
                        "SELECT table_name AS \"Table\", non_unique AS \"Non_unique\", "
                        "index_name AS \"Key_name\", seq_in_index AS \"Seq_in_index\", "
                        "column_name AS \"Column_name\", collation AS \"Collation\", "
                        "cardinality AS \"Cardinality\", sub_part AS \"Sub_part\", "
                        "packed AS \"Packed\", nullable AS \"Null\", index_type AS \"Index_type\", "
                        "comment AS \"Comment\", index_comment AS \"Index_comment\", "
                        "is_visible AS \"Visible\", expression AS \"Expression\" "
                        "FROM __mylite_index_catalog "
                        "WHERE table_schema = %Q AND table_name = %Q "
                        "ORDER BY rowid",
                        query->schema_name, query->table_name);
    return sqlite3_str_finish(sql);
}

int mylite_show_status_sql(mylite_db *database, const struct mylite_show_status_query *query,
                           char **out_sql)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    uint64_t uptime = show_status_uptime(database);
    bool first = true;

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
    sqlite3_str_appendall(sql,
                          " ORDER BY Variable_name COLLATE NOCASE, Variable_name COLLATE BINARY");

    *out_sql = sqlite3_str_finish(sql);
    return *out_sql == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

char *mylite_show_tables_sql(mylite_db *database, const struct mylite_show_tables_query *query)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "SELECT TABLE_NAME AS \"%w\"", query->column_name);
    if (query->full) {
        sqlite3_str_appendf(sql, ", TABLE_TYPE AS \"Table_type\"");
    }
    sqlite3_str_appendf(
        sql,
        " FROM ("
        "SELECT 'information_schema' AS TABLE_SCHEMA, table_name AS TABLE_NAME, "
        "'SYSTEM VIEW' AS TABLE_TYPE FROM ("
        "SELECT 'CHARACTER_SETS' AS table_name "
        "UNION ALL SELECT 'CHECK_CONSTRAINTS' "
        "UNION ALL SELECT 'COLLATION_CHARACTER_SET_APPLICABILITY' "
        "UNION ALL SELECT 'COLLATIONS' "
        "UNION ALL SELECT 'SCHEMATA' "
        "UNION ALL SELECT 'TABLES' "
        "UNION ALL SELECT 'COLUMNS' "
        "UNION ALL SELECT 'ENGINES' "
        "UNION ALL SELECT 'KEYWORDS' "
        "UNION ALL SELECT 'KEY_COLUMN_USAGE' "
        "UNION ALL SELECT 'REFERENTIAL_CONSTRAINTS' "
        "UNION ALL SELECT 'STATISTICS' "
        "UNION ALL SELECT 'TABLE_CONSTRAINTS') "
        "UNION ALL "
        "SELECT table_schema AS TABLE_SCHEMA, table_name AS TABLE_NAME, table_type AS TABLE_TYPE "
        "FROM __mylite_table_catalog) "
        "WHERE TABLE_SCHEMA = %Q",
        query->schema_name);
    if (query->glob_pattern != NULL) {
        sqlite3_str_appendf(sql, " AND TABLE_NAME GLOB %Q", query->glob_pattern);
    }
    sqlite3_str_appendf(sql, " ORDER BY TABLE_NAME COLLATE BINARY");
    return sqlite3_str_finish(sql);
}

char *mylite_show_table_status_sql(mylite_db *database,
                                   const struct mylite_show_table_status_query *query)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendall(
        sql,
        "SELECT Name, Engine, Version, Row_format, Rows, Avg_row_length, Data_length, "
        "Max_data_length, Index_length, Data_free, Auto_increment, Create_time, Update_time, "
        "Check_time, Collation, Checksum, Create_options, Comment FROM ("
        "SELECT 'information_schema' AS TABLE_SCHEMA, table_name AS Name, NULL AS Engine, "
        "10 AS Version, NULL AS Row_format, 0 AS Rows, 0 AS Avg_row_length, 0 AS Data_length, "
        "0 AS Max_data_length, 0 AS Index_length, 0 AS Data_free, NULL AS Auto_increment, "
        "'1970-01-01 00:00:00' AS Create_time, NULL AS Update_time, NULL AS Check_time, "
        "NULL AS Collation, NULL AS Checksum, '' AS Create_options, '' AS Comment FROM ("
        "SELECT 'CHARACTER_SETS' AS table_name "
        "UNION ALL SELECT 'CHECK_CONSTRAINTS' "
        "UNION ALL SELECT 'COLLATION_CHARACTER_SET_APPLICABILITY' "
        "UNION ALL SELECT 'COLLATIONS' "
        "UNION ALL SELECT 'SCHEMATA' "
        "UNION ALL SELECT 'TABLES' "
        "UNION ALL SELECT 'COLUMNS' "
        "UNION ALL SELECT 'ENGINES' "
        "UNION ALL SELECT 'KEYWORDS' "
        "UNION ALL SELECT 'KEY_COLUMN_USAGE' "
        "UNION ALL SELECT 'REFERENTIAL_CONSTRAINTS' "
        "UNION ALL SELECT 'STATISTICS' "
        "UNION ALL SELECT 'TABLE_CONSTRAINTS') "
        "UNION ALL "
        "SELECT table_schema AS TABLE_SCHEMA, table_name AS Name, engine AS Engine, "
        "version AS Version, COALESCE(row_format, 'Dynamic') AS Row_format, "
        "COALESCE(table_rows, 0) AS Rows, COALESCE(avg_row_length, 0) AS Avg_row_length, "
        "COALESCE(data_length, 0) AS Data_length, COALESCE(max_data_length, 0) AS "
        "Max_data_length, COALESCE(index_length, 0) AS Index_length, "
        "COALESCE(data_free, 0) AS Data_free, auto_increment AS Auto_increment, "
        "create_time AS Create_time, update_time AS Update_time, check_time AS Check_time, "
        "table_collation AS Collation, checksum AS Checksum, create_options AS Create_options, "
        "table_comment AS Comment FROM __mylite_table_catalog) ");
    sqlite3_str_appendf(sql, "WHERE TABLE_SCHEMA = %Q", query->schema_name);
    if (query->glob_pattern != NULL) {
        sqlite3_str_appendf(sql, " AND Name GLOB %Q", query->glob_pattern);
    }
    sqlite3_str_appendall(sql, " ORDER BY Name COLLATE BINARY");
    return sqlite3_str_finish(sql);
}

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

const char *mylite_show_schemas_sql(void)
{
    return show_schemas_sql;
}

const char *mylite_show_information_schema_schemata_sql(void)
{
    return information_schema_schemata_sql;
}

int mylite_show_attach_engines_result_metadata(mylite_db *database, mylite_stmt *stmt)
{
    static const struct mylite_show_engines_metadata_column columns[] = {
        {"Engine", 64U, false},     {"Support", 8U, false}, {"Comment", 80U, false},
        {"Transactions", 3U, true}, {"XA", 3U, true},       {"Savepoints", 3U, true},
    };
    struct mylite_result_metadata metadata = {0};

    metadata.columns = calloc(sizeof(columns) / sizeof(columns[0]), sizeof(*metadata.columns));
    if (metadata.columns == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    metadata.column_count = sizeof(columns) / sizeof(columns[0]);

    for (size_t index = 0U; index < metadata.column_count; ++index) {
        int status = mylite_result_metadata_copy_text(database, &metadata.columns[index].name,
                                                      columns[index].name);

        if (status != MYLITE_OK) {
            mylite_result_metadata_deinit(&metadata);
            return status;
        }
        metadata.columns[index].descriptor =
            show_engines_field_descriptor(columns[index].length, columns[index].nullable);
    }

    mylite_result_metadata_deinit(&stmt->result_metadata);
    stmt->result_metadata = metadata;
    return MYLITE_OK;
}

static int storage_engines_sql(mylite_db *database,
                               const struct mylite_storage_engine_columns *columns, char **out_sql)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    bool first = true;

    *out_sql = NULL;
    if (sql == NULL) {
        return MYLITE_NOMEM;
    }

    sqlite3_str_appendf(sql, "SELECT \"%w\", \"%w\", \"%w\", \"%w\", \"%w\", \"%w\" FROM (",
                        columns->engine, columns->support, columns->comment, columns->transactions,
                        columns->xa, columns->savepoints);
    for (size_t index = 0U;
         index < sizeof(mylite_storage_engine_registry) / sizeof(mylite_storage_engine_registry[0]);
         ++index) {
        append_storage_engine_row(sql, &first, columns, &mylite_storage_engine_registry[index]);
    }
    sqlite3_str_appendall(sql, ")");

    *out_sql = sqlite3_str_finish(sql);
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

static struct mylite_field_descriptor show_engines_field_descriptor(uint64_t length, bool nullable)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .length = length,
        .decimals = mylite_show_not_fixed_decimals,
        .charset_id = mylite_show_latin1_swedish_ci_charset_id,
        .nullable = nullable,
    };

    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}

static void append_show_character_set_row(sqlite3_str *sql, bool *first,
                                          const struct mylite_charset *character_set)
{
    if (!*first) {
        sqlite3_str_appendall(sql, " UNION ALL ");
    }
    sqlite3_str_appendf(sql,
                        "SELECT %Q AS \"Charset\", %Q AS \"Description\", "
                        "%Q AS \"Default collation\", %d AS \"Maxlen\"",
                        character_set->name, character_set->description,
                        character_set->default_collation, character_set->max_length);
    *first = false;
}

static void append_information_schema_character_set_row(sqlite3_str *sql, bool *first,
                                                        const struct mylite_charset *character_set)
{
    if (!*first) {
        sqlite3_str_appendall(sql, " UNION ALL ");
    }
    sqlite3_str_appendf(sql,
                        "SELECT %Q AS \"CHARACTER_SET_NAME\", "
                        "%Q AS \"DEFAULT_COLLATE_NAME\", %Q AS \"DESCRIPTION\", %d AS \"MAXLEN\"",
                        character_set->name, character_set->default_collation,
                        character_set->description, character_set->max_length);
    *first = false;
}

static void append_show_collation_row(sqlite3_str *sql, bool *first,
                                      const struct mylite_collation *collation)
{
    const char *default_text = (int)collation->is_default != 0 ? "Yes" : "";

    if (!*first) {
        sqlite3_str_appendall(sql, " UNION ALL ");
    }
    sqlite3_str_appendf(sql,
                        "SELECT %Q AS \"Collation\", %Q AS \"Charset\", %d AS \"Id\", "
                        "%Q AS \"Default\", 'Yes' AS \"Compiled\", %d AS \"Sortlen\", "
                        "%Q AS \"Pad_attribute\"",
                        collation->name, collation->character_set, collation->id, default_text,
                        collation->sort_length, collation->pad_attribute);
    *first = false;
}

static void append_information_schema_collation_row(sqlite3_str *sql, bool *first,
                                                    const struct mylite_collation *collation)
{
    const char *default_text = (int)collation->is_default != 0 ? "Yes" : "";

    if (!*first) {
        sqlite3_str_appendall(sql, " UNION ALL ");
    }
    sqlite3_str_appendf(sql,
                        "SELECT %Q AS \"COLLATION_NAME\", %Q AS \"CHARACTER_SET_NAME\", "
                        "%d AS \"ID\", %Q AS \"IS_DEFAULT\", 'Yes' AS \"IS_COMPILED\", "
                        "%d AS \"SORTLEN\", %Q AS \"PAD_ATTRIBUTE\"",
                        collation->name, collation->character_set, collation->id, default_text,
                        collation->sort_length, collation->pad_attribute);
    *first = false;
}

static void append_information_schema_collation_character_set_applicability_row(
    sqlite3_str *sql, bool *first, const struct mylite_collation *collation)
{
    if (!*first) {
        sqlite3_str_appendall(sql, " UNION ALL ");
    }
    sqlite3_str_appendf(sql, "SELECT %Q AS \"COLLATION_NAME\", %Q AS \"CHARACTER_SET_NAME\"",
                        collation->name, collation->character_set);
    *first = false;
}

static void append_information_schema_keyword_row(sqlite3_str *sql, bool *first, const char *word,
                                                  unsigned int flags)
{
    int reserved = (flags & MYLITE_SQL_KEYWORD_RESERVED) != 0U ? 1 : 0;

    if (!*first) {
        sqlite3_str_appendall(sql, ", ");
    }
    sqlite3_str_appendf(sql, "(%Q, %d)", word, reserved);
    *first = false;
}

static void append_storage_engine_row(sqlite3_str *sql, bool *first,
                                      const struct mylite_storage_engine_columns *columns,
                                      const struct mylite_storage_engine_row *engine)
{
    if (!*first) {
        sqlite3_str_appendall(sql, " UNION ALL ");
    }

    sqlite3_str_appendf(sql, "SELECT %Q AS \"%w\", %Q AS \"%w\", %Q AS \"%w\", ", engine->engine,
                        columns->engine, engine->support, columns->support, engine->comment,
                        columns->comment);
    if (engine->transactions == NULL) {
        sqlite3_str_appendf(sql, "NULL AS \"%w\", ", columns->transactions);
    } else {
        sqlite3_str_appendf(sql, "%Q AS \"%w\", ", engine->transactions, columns->transactions);
    }
    if (engine->xa == NULL) {
        sqlite3_str_appendf(sql, "NULL AS \"%w\", ", columns->xa);
    } else {
        sqlite3_str_appendf(sql, "%Q AS \"%w\", ", engine->xa, columns->xa);
    }
    if (engine->savepoints == NULL) {
        sqlite3_str_appendf(sql, "NULL AS \"%w\"", columns->savepoints);
    } else {
        sqlite3_str_appendf(sql, "%Q AS \"%w\"", engine->savepoints, columns->savepoints);
    }
    *first = false;
}
