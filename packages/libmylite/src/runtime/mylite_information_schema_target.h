#ifndef MYLITE_RUNTIME_MYLITE_INFORMATION_SCHEMA_TARGET_H
#define MYLITE_RUNTIME_MYLITE_INFORMATION_SCHEMA_TARGET_H

#include <mylite/mylite.h>

#include "sql/mylite_ast.h"

enum mylite_information_schema_table {
    MYLITE_INFORMATION_SCHEMA_NONE = 0,
    MYLITE_INFORMATION_SCHEMA_SCHEMATA = 1,
    MYLITE_INFORMATION_SCHEMA_TABLES = 2,
    MYLITE_INFORMATION_SCHEMA_COLUMNS = 3,
    MYLITE_INFORMATION_SCHEMA_STATISTICS = 4,
    MYLITE_INFORMATION_SCHEMA_ENGINES = 5,
    MYLITE_INFORMATION_SCHEMA_CHARACTER_SETS = 6,
    MYLITE_INFORMATION_SCHEMA_COLLATIONS = 7,
    MYLITE_INFORMATION_SCHEMA_COLLATION_CHARACTER_SET_APPLICABILITY = 8,
    MYLITE_INFORMATION_SCHEMA_KEYWORDS = 9,
    MYLITE_INFORMATION_SCHEMA_TABLE_CONSTRAINTS = 10,
    MYLITE_INFORMATION_SCHEMA_KEY_COLUMN_USAGE = 11,
    MYLITE_INFORMATION_SCHEMA_CHECK_CONSTRAINTS = 12,
    MYLITE_INFORMATION_SCHEMA_REFERENTIAL_CONSTRAINTS = 13,
};

int mylite_information_schema_table_from_select(const struct mylite_sql_ast_node *statement,
                                                enum mylite_information_schema_table *out_table);
enum mylite_information_schema_table mylite_information_schema_table_from_name(const char *name);

#endif
