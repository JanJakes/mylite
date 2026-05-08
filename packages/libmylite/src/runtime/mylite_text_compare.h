#ifndef MYLITE_RUNTIME_MYLITE_TEXT_COMPARE_H
#define MYLITE_RUNTIME_MYLITE_TEXT_COMPARE_H

#include <stdbool.h>

bool mylite_column_definition_uses_case_insensitive_text_compare(
    const char *data_type,
    const char *character_set_name,
    const char *collation_name
);

#endif
