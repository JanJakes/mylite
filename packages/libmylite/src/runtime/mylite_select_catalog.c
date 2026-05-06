#include "mylite_select_catalog.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_field_descriptor.h"
#include "mylite_metadata_constants.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_catalog_descriptor.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static int load_select_column_from_catalog_row(mylite_db *database,
                                               struct mylite_select_table *table,
                                               sqlite3_stmt *select);
static int load_information_schema_table_columns(mylite_db *database,
                                                 struct mylite_select_table *table);
static int load_information_schema_column(mylite_db *database, struct mylite_select_table *table,
                                          const char *name);
static struct mylite_field_descriptor information_schema_column_descriptor(const char *name);
static bool information_schema_column_is_integer(const char *name);
static bool information_schema_column_is_not_null_text(const char *name);
static int load_select_table_unique_not_null_keys(mylite_db *database,
                                                  struct mylite_select_table *table);
static int reset_select_table_unique_not_null_key(mylite_db *database,
                                                  struct mylite_select_table *table,
                                                  struct mylite_select_column_sequence *key,
                                                  char **current_name, bool *key_usable);
static int finish_select_table_unique_not_null_key(mylite_db *database,
                                                   struct mylite_select_table *table,
                                                   struct mylite_select_column_sequence *key,
                                                   bool key_usable);
static bool select_table_unique_not_null_catalog_row_usable(const struct mylite_select_table *table,
                                                            sqlite3_stmt *select,
                                                            const char *column_name,
                                                            const char *nullable,
                                                            size_t *out_column_index);
static int append_select_table_unique_not_null_key(mylite_db *database,
                                                   struct mylite_select_table *table,
                                                   struct mylite_select_column_sequence *key);
static int append_select_table_unique_key_column(mylite_db *database,
                                                 struct mylite_select_column_sequence *key,
                                                 size_t column_index);
static bool select_table_column_index_by_name(const struct mylite_select_table *table,
                                              const char *name, size_t *out_index);
static bool select_column_extra_is_visible(const char *extra);
static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_select_load_table_columns(mylite_db *database, struct mylite_select_table *table)
{
    sqlite3_stmt *select = NULL;
    char *sql = NULL;
    bool temporary = false;
    int rc = SQLITE_OK;
    int catalog_status = MYLITE_OK;

    if (mylite_ascii_case_equal(table->schema_name, "information_schema")) {
        return load_information_schema_table_columns(database, table);
    }

    catalog_status = mylite_catalog_temporary_table_exists(database, table->schema_name,
                                                           table->table_name, &temporary);
    if (catalog_status != MYLITE_OK) {
        return catalog_status;
    }
    sql = sqlite3_mprintf("SELECT column_name, extra, is_nullable, data_type, "
                          "character_octet_length, numeric_precision, numeric_scale, "
                          "datetime_precision, collation_name, column_type, column_key, "
                          "column_default FROM %s WHERE table_schema = ? "
                          "AND table_name = ? ORDER BY ordinal_position",
                          mylite_catalog_column_catalog_name(temporary));
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(select, 1, table->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, table->table_name, -1, sqlite_transient_destructor());
    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        int status = load_select_column_from_catalog_row(database, table, select);

        if (status != MYLITE_OK) {
            sqlite3_finalize(select);
            if (status == MYLITE_NOMEM) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
            }
            return status;
        }
    }
    sqlite3_finalize(select);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    if (table->column_count == 0U) {
        return mylite_diagnostics_set_table_doesnt_exist_error(database, table->schema_name,
                                                               table->table_name);
    }
    return load_select_table_unique_not_null_keys(database, table);
}

static int load_information_schema_table_columns(mylite_db *database,
                                                 struct mylite_select_table *table)
{
    sqlite3_stmt *select = NULL;
    char *sql = sqlite3_mprintf("SELECT * FROM \"%w\" LIMIT 0", table->physical_name);
    int rc = SQLITE_OK;
    int column_count = 0;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    column_count = sqlite3_column_count(select);
    for (int index = 0; index < column_count; ++index) {
        int status =
            load_information_schema_column(database, table, sqlite3_column_name(select, index));

        if (status != MYLITE_OK) {
            sqlite3_finalize(select);
            return status;
        }
    }

    sqlite3_finalize(select);
    if (table->column_count == 0U) {
        return mylite_diagnostics_set_table_doesnt_exist_error(database, table->schema_name,
                                                               table->table_name);
    }
    return MYLITE_OK;
}

static int load_information_schema_column(mylite_db *database, struct mylite_select_table *table,
                                          const char *name)
{
    struct mylite_select_column column = {
        .visible = true,
    };
    struct mylite_select_column *columns = NULL;
    size_t name_length = name == NULL ? 0U : strlen(name);

    column.name = mylite_copy_span_text(name == NULL ? "" : name, name_length);
    if (column.name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    column.descriptor = information_schema_column_descriptor(column.name);

    columns = realloc(table->columns, (table->column_count + 1U) * sizeof(*table->columns));
    if (columns == NULL) {
        mylite_select_column_deinit(&column);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    table->columns = columns;
    table->columns[table->column_count++] = column;
    return MYLITE_OK;
}

static struct mylite_field_descriptor information_schema_column_descriptor(const char *name)
{
    bool integer = information_schema_column_is_integer(name);
    bool nullable = !information_schema_column_is_not_null_text(name);
    struct mylite_field_descriptor descriptor = integer
                                                    ? (struct mylite_field_descriptor){
                                                          .type = MYLITE_FIELD_TYPE_LONGLONG,
                                                          .flags = MYLITE_FIELD_FLAG_BINARY |
                                                                   MYLITE_FIELD_FLAG_NUM,
                                                          .length = mylite_mysql_signed_longlong_display_length,
                                                          .charset_id = mylite_mysql_binary_charset_id,
                                                          .nullable = nullable,
                                                      }
                                                    : (struct mylite_field_descriptor){
                                                          .type = MYLITE_FIELD_TYPE_VAR_STRING,
                                                          .length = mylite_mysql_text_length,
                                                          .decimals = mylite_mysql_not_fixed_decimals,
                                                          .charset_id = mylite_mysql_utf8mb4_0900_ai_ci_charset_id,
                                                          .nullable = nullable,
                                                      };

    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}

static bool information_schema_column_is_integer(const char *name)
{
    static const char *const names[] = {
        "VERSION",
        "TABLE_ROWS",
        "AVG_ROW_LENGTH",
        "DATA_LENGTH",
        "MAX_DATA_LENGTH",
        "INDEX_LENGTH",
        "DATA_FREE",
        "AUTO_INCREMENT",
        "CHECKSUM",
        "ORDINAL_POSITION",
        "CHARACTER_MAXIMUM_LENGTH",
        "CHARACTER_OCTET_LENGTH",
        "NUMERIC_PRECISION",
        "NUMERIC_SCALE",
        "DATETIME_PRECISION",
        "SRS_ID",
        "NON_UNIQUE",
        "SEQ_IN_INDEX",
        "CARDINALITY",
        "SUB_PART",
        "POSITION_IN_UNIQUE_CONSTRAINT",
    };

    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        if (mylite_ascii_case_equal(name, names[index])) {
            return true;
        }
    }
    return false;
}

static bool information_schema_column_is_not_null_text(const char *name)
{
    static const char *const names[] = {
        "TABLE_CATALOG",      "TABLE_SCHEMA",      "TABLE_NAME",      "TABLE_TYPE",
        "CATALOG_NAME",       "SCHEMA_NAME",       "COLUMN_NAME",     "ORDINAL_POSITION",
        "CONSTRAINT_CATALOG", "CONSTRAINT_SCHEMA", "CONSTRAINT_NAME", "INDEX_SCHEMA",
        "INDEX_NAME",         "SEQ_IN_INDEX",
    };

    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        if (mylite_ascii_case_equal(name, names[index])) {
            return true;
        }
    }
    return false;
}

static int load_select_column_from_catalog_row(mylite_db *database,
                                               struct mylite_select_table *table,
                                               sqlite3_stmt *select)
{
    const char *name = (const char *)sqlite3_column_text(select, 0);
    const char *extra = (const char *)sqlite3_column_text(select, 1);
    struct mylite_select_column column = {
        .visible = select_column_extra_is_visible(extra),
    };
    struct mylite_select_column *columns = NULL;

    column.name = mylite_copy_span_text(name, name == NULL ? 0U : strlen(name));
    if (column.name == NULL) {
        return MYLITE_NOMEM;
    }
    int status = mylite_select_catalog_load_column_descriptor(database, select, &column.descriptor);
    if (status != MYLITE_OK) {
        mylite_select_column_deinit(&column);
        return status;
    }

    columns = realloc(table->columns, (table->column_count + 1U) * sizeof(*table->columns));
    if (columns == NULL) {
        mylite_select_column_deinit(&column);
        return MYLITE_NOMEM;
    }

    table->columns = columns;
    table->columns[table->column_count++] = column;
    return MYLITE_OK;
}

static int load_select_table_unique_not_null_keys(mylite_db *database,
                                                  struct mylite_select_table *table)
{
    char *sql = NULL;
    sqlite3_stmt *select = NULL;
    struct mylite_select_column_sequence key = {0};
    char *current_name = NULL;
    bool key_usable = true;
    bool temporary = false;
    int rc = SQLITE_OK;
    int catalog_status = mylite_catalog_temporary_table_exists(database, table->schema_name,
                                                               table->table_name, &temporary);

    if (catalog_status != MYLITE_OK) {
        return catalog_status;
    }
    sql = sqlite3_mprintf("SELECT index_name, column_name, nullable, sub_part, expression "
                          "FROM %s WHERE table_schema = ? AND table_name = ? "
                          "AND non_unique = 0 ORDER BY index_name, seq_in_index",
                          mylite_catalog_index_catalog_name(temporary));
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(select, 1, table->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, table->table_name, -1, sqlite_transient_destructor());
    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        const char *index_name = (const char *)sqlite3_column_text(select, 0);
        const char *safe_index_name = index_name == NULL ? "" : index_name;
        const char *column_name = (const char *)sqlite3_column_text(select, 1);
        const char *nullable = (const char *)sqlite3_column_text(select, 2);
        size_t column_index = 0U;

        if (current_name != NULL && !mylite_ascii_case_equal(current_name, safe_index_name)) {
            int status = reset_select_table_unique_not_null_key(database, table, &key,
                                                                &current_name, &key_usable);

            if (status != MYLITE_OK) {
                sqlite3_finalize(select);
                return status;
            }
        }

        if (current_name == NULL) {
            current_name = mylite_copy_span_text(safe_index_name, strlen(safe_index_name));
            if (current_name == NULL) {
                sqlite3_finalize(select);
                mylite_select_column_sequence_deinit(&key);
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
        }
        if (!select_table_unique_not_null_catalog_row_usable(table, select, column_name, nullable,
                                                             &column_index)) {
            key_usable = false;
            continue;
        }

        if (key_usable &&
            append_select_table_unique_key_column(
                database, &key, table->first_column_index + column_index) != MYLITE_OK) {
            sqlite3_finalize(select);
            mylite_select_column_sequence_deinit(&key);
            return MYLITE_NOMEM;
        }
    }
    sqlite3_finalize(select);
    if (rc != SQLITE_DONE) {
        free(current_name);
        mylite_select_column_sequence_deinit(&key);
        return mylite_diagnostics_set_sqlite_error(database);
    }
    if (current_name != NULL) {
        int status = finish_select_table_unique_not_null_key(database, table, &key, key_usable);

        if (status != MYLITE_OK) {
            free(current_name);
            mylite_select_column_sequence_deinit(&key);
            return status;
        }
    }
    free(current_name);
    mylite_select_column_sequence_deinit(&key);
    return MYLITE_OK;
}

static int reset_select_table_unique_not_null_key(mylite_db *database,
                                                  struct mylite_select_table *table,
                                                  struct mylite_select_column_sequence *key,
                                                  char **current_name, bool *key_usable)
{
    int status = finish_select_table_unique_not_null_key(database, table, key, *key_usable);

    if (status != MYLITE_OK) {
        mylite_select_column_sequence_deinit(key);
        free(*current_name);
        *current_name = NULL;
        return status;
    }
    mylite_select_column_sequence_deinit(key);
    free(*current_name);
    *current_name = NULL;
    *key_usable = true;
    return MYLITE_OK;
}

static int finish_select_table_unique_not_null_key(mylite_db *database,
                                                   struct mylite_select_table *table,
                                                   struct mylite_select_column_sequence *key,
                                                   bool key_usable)
{
    if (!key_usable || key->column_count == 0U) {
        return MYLITE_OK;
    }
    return append_select_table_unique_not_null_key(database, table, key);
}

static bool select_table_unique_not_null_catalog_row_usable(const struct mylite_select_table *table,
                                                            sqlite3_stmt *select,
                                                            const char *column_name,
                                                            const char *nullable,
                                                            size_t *out_column_index)
{
    if (column_name == NULL || mylite_ascii_case_equal(nullable, "YES")) {
        return false;
    }
    if (sqlite3_column_type(select, 3) != SQLITE_NULL ||
        sqlite3_column_type(select, 4) != SQLITE_NULL) {
        return false;
    }
    return select_table_column_index_by_name(table, column_name, out_column_index);
}

static int append_select_table_unique_not_null_key(mylite_db *database,
                                                   struct mylite_select_table *table,
                                                   struct mylite_select_column_sequence *key)
{
    struct mylite_select_column_sequence *keys = NULL;

    keys = realloc(table->unique_not_null_keys,
                   (table->unique_not_null_key_count + 1U) * sizeof(*keys));
    if (keys == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    table->unique_not_null_keys = keys;
    table->unique_not_null_keys[table->unique_not_null_key_count++] = *key;
    *key = (struct mylite_select_column_sequence){0};
    return MYLITE_OK;
}

static int append_select_table_unique_key_column(mylite_db *database,
                                                 struct mylite_select_column_sequence *key,
                                                 size_t column_index)
{
    size_t *columns =
        realloc(key->column_indexes, (key->column_count + 1U) * sizeof(*key->column_indexes));

    if (columns == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    key->column_indexes = columns;
    key->column_indexes[key->column_count++] = column_index;
    return MYLITE_OK;
}

static bool select_table_column_index_by_name(const struct mylite_select_table *table,
                                              const char *name, size_t *out_index)
{
    if (table == NULL || name == NULL || out_index == NULL) {
        return false;
    }
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (mylite_ascii_case_equal(table->columns[index].name, name)) {
            *out_index = index;
            return true;
        }
    }
    return false;
}

static bool select_column_extra_is_visible(const char *extra)
{
    if (extra == NULL) {
        return true;
    }
    if (strstr(extra, "INVISIBLE") == NULL) {
        return true;
    }
    return false;
}

static sqlite3_destructor_type sqlite_transient_destructor(void)
{
    // SQLite's public macro intentionally uses this sentinel pointer value.
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
