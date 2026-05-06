#ifndef MYLITE_RUNTIME_MYLITE_INFORMATION_SCHEMA_DYNAMIC_H
#define MYLITE_RUNTIME_MYLITE_INFORMATION_SCHEMA_DYNAMIC_H

#include <mylite/mylite.h>

int mylite_information_schema_character_sets_sql(mylite_db *database, char **out_sql);
int mylite_information_schema_collations_sql(mylite_db *database, char **out_sql);
int mylite_information_schema_collation_character_set_applicability_sql(
    mylite_db *database,
    char **out_sql
);
int mylite_information_schema_keywords_sql(mylite_db *database, char **out_sql);

#endif
