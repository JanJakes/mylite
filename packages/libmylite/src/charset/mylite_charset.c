#include "mylite_charset.h"

#include <stddef.h>

static bool ascii_case_equal(const char *left, const char *right);

const char *mylite_charset_default_name(void)
{
    return "utf8mb4";
}

const char *mylite_charset_default_collation_name(void)
{
    return "utf8mb4_0900_ai_ci";
}

const struct mylite_charset *mylite_charset_lookup(const char *name)
{
    static const struct mylite_charset character_sets[] = {
        {.name = "binary", .default_collation = "binary", .max_length = 1},
        {.name = "latin1", .default_collation = "latin1_swedish_ci", .max_length = 1},
        {.name = "utf8mb3", .default_collation = "utf8mb3_general_ci", .max_length = 3},
        {.name = "utf8mb4", .default_collation = "utf8mb4_0900_ai_ci", .max_length = 4},
    };

    for (size_t index = 0U; index < sizeof(character_sets) / sizeof(character_sets[0]); ++index) {
        if (ascii_case_equal(name, character_sets[index].name)) {
            return &character_sets[index];
        }
    }
    return NULL;
}

const struct mylite_collation *mylite_collation_lookup(const char *name)
{
    static const struct mylite_collation collations[] = {
        {.name = "binary", .character_set = "binary", .id = 63, .is_default = true},
        {.name = "latin1_swedish_ci", .character_set = "latin1", .id = 8, .is_default = true},
        {.name = "latin1_bin", .character_set = "latin1", .id = 47, .is_default = false},
        {.name = "utf8mb3_general_ci", .character_set = "utf8mb3", .id = 33, .is_default = true},
        {.name = "utf8mb3_bin", .character_set = "utf8mb3", .id = 83, .is_default = false},
        {.name = "utf8mb4_0900_ai_ci", .character_set = "utf8mb4", .id = 255, .is_default = true},
        {.name = "utf8mb4_bin", .character_set = "utf8mb4", .id = 46, .is_default = false},
    };

    for (size_t index = 0U; index < sizeof(collations) / sizeof(collations[0]); ++index) {
        if (ascii_case_equal(name, collations[index].name)) {
            return &collations[index];
        }
    }
    return NULL;
}

bool mylite_charset_collation_match(const struct mylite_charset *character_set,
                                    const struct mylite_collation *collation)
{
    if (character_set == NULL || collation == NULL) {
        return false;
    }
    return ascii_case_equal(character_set->name, collation->character_set);
}

static bool ascii_case_equal(const char *left, const char *right)
{
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
