#include "mylite_show.h"

#include "mylite_diagnostics.h"
#include "mylite_field_descriptor.h"
#include "mylite_metadata.h"
#include "mylite_runtime.h"
#include "mylite_show_types.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

static int storage_engines_sql(mylite_db *database,
                               const struct mylite_storage_engine_columns *columns, char **out_sql);
static struct mylite_field_descriptor show_engines_field_descriptor(uint64_t length, bool nullable);
static void append_storage_engine_row(sqlite3_str *sql, bool *first,
                                      const struct mylite_storage_engine_columns *columns,
                                      const struct mylite_storage_engine_row *engine);

static const unsigned int mylite_show_latin1_swedish_ci_charset_id = 8U;
static const unsigned int mylite_show_not_fixed_decimals = 31U;

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
