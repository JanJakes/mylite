#include "mylite_storage_engine.h"

#include "mylite_runtime.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdlib.h>

struct mylite_storage_engine_row {
    const char *engine;
    const char *support;
    const char *comment;
    const char *transactions;
    const char *xa;
    const char *savepoints;
};

struct mylite_storage_engine_columns {
    const char *engine;
    const char *support;
    const char *comment;
    const char *transactions;
    const char *xa;
    const char *savepoints;
};

static int storage_engines_sql(mylite_db *database,
                               const struct mylite_storage_engine_columns *columns, char **out_sql);
static void append_storage_engine_row(sqlite3_str *sql, bool *first,
                                      const struct mylite_storage_engine_columns *columns,
                                      const struct mylite_storage_engine_row *engine);

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

int mylite_storage_engine_show_sql(mylite_db *database, char **out_sql)
{
    static const struct mylite_storage_engine_columns columns = {
        "Engine", "Support", "Comment", "Transactions", "XA", "Savepoints",
    };

    return storage_engines_sql(database, &columns, out_sql);
}

int mylite_storage_engine_information_schema_sql(mylite_db *database, char **out_sql)
{
    static const struct mylite_storage_engine_columns columns = {
        "ENGINE", "SUPPORT", "COMMENT", "TRANSACTIONS", "XA", "SAVEPOINTS",
    };

    return storage_engines_sql(database, &columns, out_sql);
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
