#include "mylite_show.h"

#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_field_descriptor.h"
#include "mylite_metadata.h"
#include "mylite_runtime.h"
#include "mylite_show_types.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

static int storage_engines_sql(mylite_db *database,
                               const struct mylite_storage_engine_columns *columns, char **out_sql);
static uint64_t show_status_uptime(const mylite_db *database);
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
