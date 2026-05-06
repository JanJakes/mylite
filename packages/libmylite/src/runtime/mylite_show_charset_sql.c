#include "mylite_show.h"

#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_show_types.h"
#include "sqlite3.h"

#include <stdbool.h>

static void append_show_character_set_row(sqlite3_str *sql, bool *first,
                                          const struct mylite_charset *character_set);
static void append_show_collation_row(sqlite3_str *sql, bool *first,
                                      const struct mylite_collation *collation);

int mylite_show_character_set_sql(mylite_db *database,
                                  const struct mylite_show_character_set_query *query,
                                  char **out_sql)
{
    static const char *const columns[] = {"Charset", "Description", "Default collation", "Maxlen"};
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    bool first = true;
    int status = MYLITE_OK;

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
    if (query->where_expression != NULL) {
        sqlite3_str_appendall(sql, query->like_pattern == NULL ? " WHERE " : " AND ");
        status = mylite_show_append_where_expression(database, sql, query->where_expression,
                                                     columns, sizeof(columns) / sizeof(columns[0]));
    }
    sqlite3_str_appendall(sql, " ORDER BY Charset COLLATE NOCASE, Charset COLLATE BINARY");

    *out_sql = sqlite3_str_finish(sql);
    if (status != MYLITE_OK) {
        sqlite3_free(*out_sql);
        *out_sql = NULL;
        if (status == MYLITE_UNSUPPORTED) {
            (void)mylite_diagnostics_set_error_message(
                database, "SHOW CHARACTER SET WHERE expression is not supported");
        }
        return status;
    }
    return *out_sql == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

int mylite_show_collation_sql(mylite_db *database, const struct mylite_show_collation_query *query,
                              char **out_sql)
{
    static const char *const columns[] = {"Collation", "Charset", "Id",           "Default",
                                          "Compiled",  "Sortlen", "Pad_attribute"};
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    bool first = true;
    int status = MYLITE_OK;

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
    if (query->where_expression != NULL) {
        sqlite3_str_appendall(sql, query->like_pattern == NULL ? " WHERE " : " AND ");
        status = mylite_show_append_where_expression(database, sql, query->where_expression,
                                                     columns, sizeof(columns) / sizeof(columns[0]));
    }
    sqlite3_str_appendall(sql, " ORDER BY Collation COLLATE NOCASE, Collation COLLATE BINARY");

    *out_sql = sqlite3_str_finish(sql);
    if (status != MYLITE_OK) {
        sqlite3_free(*out_sql);
        *out_sql = NULL;
        if (status == MYLITE_UNSUPPORTED) {
            (void)mylite_diagnostics_set_error_message(
                database, "SHOW COLLATION WHERE expression is not supported");
        }
        return status;
    }
    return *out_sql == NULL ? MYLITE_NOMEM : MYLITE_OK;
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
