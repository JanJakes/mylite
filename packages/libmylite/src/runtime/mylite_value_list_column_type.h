#ifndef MYLITE_RUNTIME_MYLITE_VALUE_LIST_COLUMN_TYPE_H
#define MYLITE_RUNTIME_MYLITE_VALUE_LIST_COLUMN_TYPE_H

#include <mylite/mylite.h>

#include <stdbool.h>

bool mylite_value_list_column_type_is_supported(const char *data_type);
int mylite_configure_value_list_column_type(
    mylite_db *database,
    const char *physical_table_name,
    const char *column_name,
    const char *data_type,
    const char *column_type
);

#endif
