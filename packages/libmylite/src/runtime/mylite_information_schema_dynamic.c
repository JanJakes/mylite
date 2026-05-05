#include "mylite_information_schema_dynamic.h"

#include "mylite_charset.h"
#include "mylite_runtime.h"
#include "sql/mylite_lexer.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

static void append_information_schema_character_set_row(sqlite3_str *sql, bool *first,
                                                        const struct mylite_charset *character_set);
static void append_information_schema_collation_row(sqlite3_str *sql, bool *first,
                                                    const struct mylite_collation *collation);
static void append_information_schema_collation_character_set_applicability_row(
    sqlite3_str *sql, bool *first, const struct mylite_collation *collation);
static void append_information_schema_keyword_row(sqlite3_str *sql, bool *first, const char *word,
                                                  unsigned int flags);

int mylite_information_schema_character_sets_sql(mylite_db *database, char **out_sql)
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

int mylite_information_schema_collations_sql(mylite_db *database, char **out_sql)
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

int mylite_information_schema_collation_character_set_applicability_sql(mylite_db *database,
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

int mylite_information_schema_keywords_sql(mylite_db *database, char **out_sql)
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
