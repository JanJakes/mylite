#ifndef MYLITE_RUNTIME_MYLITE_SELECT_CATALOG_DESCRIPTOR_SOURCE_H
#define MYLITE_RUNTIME_MYLITE_SELECT_CATALOG_DESCRIPTOR_SOURCE_H

#include "sqlite3.h"

#include <stdbool.h>

struct mylite_catalog_text_match {
    const char *text;
    const char *word;
};

struct mylite_catalog_column_descriptor_source {
    sqlite3_stmt *select;
    const char *extra;
    const char *is_nullable;
    const char *data_type;
    const char *collation_name;
    const char *column_type;
    const char *column_key;
    int column_default_index;
    int character_octet_length_index;
    int numeric_precision_index;
    int numeric_scale_index;
    int datetime_precision_index;
    bool nullable;
    bool is_unsigned;
    bool is_zerofill;
    bool auto_increment;
};

struct mylite_catalog_column_descriptor_source
mylite_select_catalog_column_descriptor_source(sqlite3_stmt *select);
bool mylite_select_catalog_text_contains_word(struct mylite_catalog_text_match match);

#endif
