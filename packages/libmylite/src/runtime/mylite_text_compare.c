#include "mylite_text_compare.h"

#include "mylite_metadata_constants.h"
#include "mylite_span.h"

#include <string.h>

static bool column_data_type_is_nonbinary_text(const char *data_type);
static bool column_text_metadata_uses_binary_compare(
    const char *character_set_name,
    const char *collation_name
);
static bool collation_name_is_binary(const char *collation_name);
static bool ascii_case_suffix_equal(const char *text, const char *suffix);

bool mylite_column_definition_uses_case_insensitive_text_compare(
    const char *data_type,
    const char *character_set_name,
    const char *collation_name
) {
    return column_data_type_is_nonbinary_text(data_type) &&
           !column_text_metadata_uses_binary_compare(character_set_name, collation_name);
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

static bool column_text_metadata_uses_binary_compare(
    const char *character_set_name,
    const char *collation_name
) {
    return mylite_ascii_case_equal(character_set_name, mylite_mysql_binary_charset_name) ||
           collation_name_is_binary(collation_name);
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
