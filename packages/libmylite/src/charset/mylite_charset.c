#include "mylite_charset.h"

#include <stddef.h>

static const struct mylite_charset *lookup_registered_charset(const char *name);
static bool ascii_case_equal(const char *left, const char *right);

static const struct mylite_charset character_sets[] = {
    {.name = "binary",
     .description = "Binary pseudo charset",
     .default_collation = "binary",
     .max_length = 1},
    {.name = "latin1",
     .description = "cp1252 West European",
     .default_collation = "latin1_swedish_ci",
     .max_length = 1},
    {.name = "utf8mb3",
     .description = "UTF-8 Unicode",
     .default_collation = "utf8mb3_general_ci",
     .max_length = 3},
    {.name = "utf8mb4",
     .description = "UTF-8 Unicode",
     .default_collation = "utf8mb4_0900_ai_ci",
     .max_length = 4},
};

static const struct mylite_collation collations[] = {
    {.name = "binary",
     .character_set = "binary",
     .id = 63,
     .sort_length = 1,
     .pad_attribute = "NO PAD",
     .is_default = true},
    {.name = "latin1_swedish_ci",
     .character_set = "latin1",
     .id = 8,
     .sort_length = 1,
     .pad_attribute = "PAD SPACE",
     .is_default = true},
    {.name = "latin1_bin",
     .character_set = "latin1",
     .id = 47,
     .sort_length = 1,
     .pad_attribute = "PAD SPACE",
     .is_default = false},
    {.name = "utf8mb3_general_ci",
     .character_set = "utf8mb3",
     .id = 33,
     .sort_length = 1,
     .pad_attribute = "PAD SPACE",
     .is_default = true},
    {.name = "utf8mb3_bin",
     .character_set = "utf8mb3",
     .id = 83,
     .sort_length = 1,
     .pad_attribute = "PAD SPACE",
     .is_default = false},
    {.name = "utf8mb4_0900_ai_ci",
     .character_set = "utf8mb4",
     .id = 255,
     .sort_length = 0,
     .pad_attribute = "NO PAD",
     .is_default = true},
    {.name = "utf8mb4_unicode_520_ci",
     .character_set = "utf8mb4",
     .id = 246,
     .sort_length = 8,
     .pad_attribute = "PAD SPACE",
     .is_default = false},
    {.name = "utf8mb4_unicode_ci",
     .character_set = "utf8mb4",
     .id = 224,
     .sort_length = 8,
     .pad_attribute = "PAD SPACE",
     .is_default = false},
    {.name = "utf8mb4_bin",
     .character_set = "utf8mb4",
     .id = 46,
     .sort_length = 1,
     .pad_attribute = "PAD SPACE",
     .is_default = false},
};

const char *mylite_charset_default_name(void) {
    return "utf8mb4";
}

const char *mylite_charset_default_collation_name(void) {
    return "utf8mb4_0900_ai_ci";
}

size_t mylite_charset_count(void) {
    return sizeof(character_sets) / sizeof(character_sets[0]);
}

const struct mylite_charset *mylite_charset_at(size_t index) {
    if (index >= mylite_charset_count()) {
        return NULL;
    }
    return &character_sets[index];
}

const struct mylite_charset *mylite_charset_lookup(const char *name) {
    const struct mylite_charset *character_set = lookup_registered_charset(name);
    if (character_set != NULL) {
        return character_set;
    }
    if (mylite_charset_name_is_utf8_alias(name)) {
        return lookup_registered_charset("utf8mb3");
    }
    return NULL;
}

bool mylite_charset_name_is_utf8_alias(const char *name) {
    return ascii_case_equal(name, "utf8");
}

size_t mylite_collation_count(void) {
    return sizeof(collations) / sizeof(collations[0]);
}

const struct mylite_collation *mylite_collation_at(size_t index) {
    if (index >= mylite_collation_count()) {
        return NULL;
    }
    return &collations[index];
}

const struct mylite_collation *mylite_collation_lookup(const char *name) {
    for (size_t index = 0U; index < mylite_collation_count(); ++index) {
        if (ascii_case_equal(name, collations[index].name)) {
            return &collations[index];
        }
    }
    return NULL;
}

bool mylite_charset_collation_match(
    const struct mylite_charset *character_set,
    const struct mylite_collation *collation
) {
    if (character_set == NULL || collation == NULL) {
        return false;
    }
    return ascii_case_equal(character_set->name, collation->character_set);
}

static const struct mylite_charset *lookup_registered_charset(const char *name) {
    for (size_t index = 0U; index < mylite_charset_count(); ++index) {
        if (ascii_case_equal(name, character_sets[index].name)) {
            return &character_sets[index];
        }
    }
    return NULL;
}

static bool ascii_case_equal(const char *left, const char *right) {
    size_t index = 0U;

    if (left == NULL || right == NULL) {
        return false;
    }

    while (left[index] != '\0' && right[index] != '\0') {
        unsigned char left_byte = (unsigned char)left[index];
        unsigned char right_byte = (unsigned char)right[index];

        if (left_byte >= 'A' && left_byte <= 'Z') {
            left_byte = (unsigned char)(left_byte - 'A' + 'a');
        }
        if (right_byte >= 'A' && right_byte <= 'Z') {
            right_byte = (unsigned char)(right_byte - 'A' + 'a');
        }
        if (left_byte != right_byte) {
            return false;
        }
        ++index;
    }
    if (left[index] == '\0' && right[index] == '\0') {
        return true;
    }
    return false;
}
