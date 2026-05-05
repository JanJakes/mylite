#include "mylite_strcmp_compare.h"

#include "mylite_charset.h"
#include "mylite_expression_collation_types.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"

#include <string.h>

static size_t trimmed_strcmp_length(const char *text, size_t length);
static unsigned char strcmp_compare_byte(unsigned char value,
                                         struct mylite_strcmp_compare_options options);
static bool strcmp_collation_ignores_trailing_spaces(const char *collation_name);
static bool strcmp_collation_is_case_sensitive(const struct mylite_charset_collation_info *info);

int mylite_strcmp_compare_texts(const char *left, size_t left_length, const char *right,
                                size_t right_length, struct mylite_strcmp_compare_options options)
{
    size_t compare_length = 0U;

    if (left == NULL) {
        left_length = 0U;
    }
    if (right == NULL) {
        right_length = 0U;
    }
    if (options.ignore_trailing_spaces) {
        left_length = trimmed_strcmp_length(left, left_length);
        right_length = trimmed_strcmp_length(right, right_length);
    }
    if (left == NULL || right == NULL) {
        size_t normalized_left_length = left == NULL ? 0U : left_length;
        size_t normalized_right_length = right == NULL ? 0U : right_length;

        return (normalized_left_length > normalized_right_length) -
               (normalized_left_length < normalized_right_length);
    }
    if (left_length == 0U || right_length == 0U) {
        return (left_length > right_length) - (left_length < right_length);
    }

    compare_length = left_length < right_length ? left_length : right_length;
    for (size_t index = 0U; index < compare_length; ++index) {
        unsigned char left_byte = strcmp_compare_byte((unsigned char)left[index], options);
        unsigned char right_byte = strcmp_compare_byte((unsigned char)right[index], options);

        if (left_byte != right_byte) {
            return left_byte > right_byte ? 1 : -1;
        }
    }
    return (left_length > right_length) - (left_length < right_length);
}

struct mylite_strcmp_compare_options
mylite_strcmp_compare_options_for_collation(const struct mylite_charset_collation_info *info)
{
    const char *collation_name = info == NULL || info->collation == NULL
                                     ? mylite_charset_default_collation_name()
                                     : info->collation;

    return (struct mylite_strcmp_compare_options){
        .ignore_trailing_spaces = strcmp_collation_ignores_trailing_spaces(collation_name),
        .case_sensitive = strcmp_collation_is_case_sensitive(info),
    };
}

static size_t trimmed_strcmp_length(const char *text, size_t length)
{
    if (text == NULL) {
        return 0U;
    }
    while (length > 0U && text[length - 1U] == ' ') {
        length -= 1U;
    }
    return length;
}

static unsigned char strcmp_compare_byte(unsigned char value,
                                         struct mylite_strcmp_compare_options options)
{
    if (!options.case_sensitive && value >= 'A' && value <= 'Z') {
        return (unsigned char)(value - 'A' + 'a');
    }
    return value;
}

static bool strcmp_collation_ignores_trailing_spaces(const char *collation_name)
{
    const struct mylite_collation *collation = mylite_collation_lookup(collation_name);

    if (collation == NULL) {
        return false;
    }
    return mylite_ascii_case_equal(collation->pad_attribute, "PAD SPACE");
}

static bool strcmp_collation_is_case_sensitive(const struct mylite_charset_collation_info *info)
{
    const char *collation_name = info == NULL || info->collation == NULL
                                     ? mylite_charset_default_collation_name()
                                     : info->collation;
    size_t collation_length = strlen(collation_name);

    if (info != NULL &&
        mylite_ascii_case_equal(info->character_set, mylite_mysql_binary_charset_name)) {
        return true;
    }
    if (mylite_ascii_case_equal(collation_name, mylite_mysql_binary_charset_name)) {
        return true;
    }
    if (collation_length < 4U) {
        return false;
    }
    return mylite_ascii_case_equal(collation_name + collation_length - 4U, "_bin");
}
