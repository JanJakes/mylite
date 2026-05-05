#include "mylite_select_catalog_descriptor_source.h"

#include "mylite_span.h"

#include <ctype.h>
#include <string.h>

static bool catalog_column_descriptor_source_is_nullable(const char *is_nullable);

struct mylite_catalog_column_descriptor_source
mylite_select_catalog_column_descriptor_source(sqlite3_stmt *select)
{
    enum {
        select_extra = 1,
        select_is_nullable = 2,
        select_data_type = 3,
        select_character_octet_length = 4,
        select_numeric_precision = 5,
        select_numeric_scale = 6,
        select_datetime_precision = 7,
        select_collation_name = 8,
        select_column_type = 9,
        select_column_key = 10,
        select_column_default = 11,
    };
    struct mylite_catalog_column_descriptor_source source = {
        .select = select,
        .extra = (const char *)sqlite3_column_text(select, select_extra),
        .is_nullable = (const char *)sqlite3_column_text(select, select_is_nullable),
        .data_type = (const char *)sqlite3_column_text(select, select_data_type),
        .collation_name = (const char *)sqlite3_column_text(select, select_collation_name),
        .column_type = (const char *)sqlite3_column_text(select, select_column_type),
        .column_key = (const char *)sqlite3_column_text(select, select_column_key),
        .column_default_index = select_column_default,
        .character_octet_length_index = select_character_octet_length,
        .numeric_precision_index = select_numeric_precision,
        .numeric_scale_index = select_numeric_scale,
        .datetime_precision_index = select_datetime_precision,
    };

    source.nullable = catalog_column_descriptor_source_is_nullable(source.is_nullable);
    source.is_unsigned = mylite_select_catalog_text_contains_word(
        (struct mylite_catalog_text_match){.text = source.column_type, .word = "unsigned"});
    source.is_zerofill = mylite_select_catalog_text_contains_word(
        (struct mylite_catalog_text_match){.text = source.column_type, .word = "zerofill"});
    source.auto_increment = mylite_select_catalog_text_contains_word(
        (struct mylite_catalog_text_match){.text = source.extra, .word = "auto_increment"});
    return source;
}

bool mylite_select_catalog_text_contains_word(struct mylite_catalog_text_match match)
{
    size_t word_length = match.word == NULL ? 0U : strlen(match.word);

    if (match.text == NULL || word_length == 0U) {
        return false;
    }
    for (const char *cursor = match.text; *cursor != '\0'; ++cursor) {
        size_t index = 0U;

        while (index < word_length && cursor[index] != '\0' &&
               tolower((unsigned char)cursor[index]) == tolower((unsigned char)match.word[index])) {
            ++index;
        }
        if (index == word_length) {
            return true;
        }
    }
    return false;
}

static bool catalog_column_descriptor_source_is_nullable(const char *is_nullable)
{
    if (is_nullable == NULL) {
        return true;
    }
    if (mylite_ascii_case_equal(is_nullable, "YES")) {
        return true;
    }
    return false;
}
