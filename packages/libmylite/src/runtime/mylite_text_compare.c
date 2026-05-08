#include "mylite_text_compare.h"

#include "mylite_charset.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"

#include <string.h>

static bool column_data_type_is_nonbinary_text(const char *data_type);
static const struct mylite_collation *column_text_metadata_collation(
    const char *character_set_name,
    const char *collation_name
);
static bool collation_uses_pad_space(const struct mylite_collation *collation);
static bool collation_name_is_binary(const char *collation_name);
static bool ascii_case_suffix_equal(const char *text, const char *suffix);

bool mylite_column_definition_uses_case_insensitive_text_compare(
    const char *data_type,
    const char *character_set_name,
    const char *collation_name
) {
    return column_data_type_is_nonbinary_text(data_type) &&
           !mylite_column_definition_uses_binary_text_compare(
               data_type,
               character_set_name,
               collation_name
           );
}

bool mylite_column_definition_uses_pad_space_text_compare(
    const char *data_type,
    const char *character_set_name,
    const char *collation_name
) {
    return column_data_type_is_nonbinary_text(data_type) &&
           collation_uses_pad_space(
               column_text_metadata_collation(character_set_name, collation_name)
           );
}

bool mylite_column_definition_uses_binary_text_compare(
    const char *data_type,
    const char *character_set_name,
    const char *collation_name
) {
    return column_data_type_is_nonbinary_text(data_type) &&
           (mylite_ascii_case_equal(character_set_name, mylite_mysql_binary_charset_name) ||
            collation_name_is_binary(collation_name));
}

static bool column_data_type_is_nonbinary_text(const char *data_type) {
    if (data_type == NULL) {
        return false;
    }
    return mylite_ascii_case_equal(data_type, "char") ||
           mylite_ascii_case_equal(data_type, "varchar") ||
           mylite_ascii_case_equal(data_type, "tinytext") ||
           mylite_ascii_case_equal(data_type, "text") ||
           mylite_ascii_case_equal(data_type, "mediumtext") ||
           mylite_ascii_case_equal(data_type, "longtext") ||
           mylite_ascii_case_equal(data_type, "enum") || mylite_ascii_case_equal(data_type, "set");
}

static const struct mylite_collation *column_text_metadata_collation(
    const char *character_set_name,
    const char *collation_name
) {
    const struct mylite_collation *collation = mylite_collation_lookup(collation_name);
    const struct mylite_charset *character_set = NULL;

    if (collation != NULL) {
        return collation;
    }
    character_set = mylite_charset_lookup(character_set_name);
    if (character_set == NULL) {
        return NULL;
    }
    return mylite_collation_lookup(character_set->default_collation);
}

static bool collation_uses_pad_space(const struct mylite_collation *collation) {
    return collation != NULL && mylite_ascii_case_equal(collation->pad_attribute, "PAD SPACE");
}

static bool collation_name_is_binary(const char *collation_name) {
    return mylite_ascii_case_equal(collation_name, mylite_mysql_binary_charset_name) ||
           ascii_case_suffix_equal(collation_name, "_bin");
}

static bool ascii_case_suffix_equal(const char *text, const char *suffix) {
    size_t text_length = text == NULL ? 0U : strlen(text);
    size_t suffix_length = suffix == NULL ? 0U : strlen(suffix);

    if (suffix_length == 0U || text_length < suffix_length) {
        return false;
    }
    return mylite_ascii_case_equal(text + text_length - suffix_length, suffix);
}
