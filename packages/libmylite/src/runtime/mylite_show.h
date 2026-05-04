#ifndef MYLITE_RUNTIME_MYLITE_SHOW_H
#define MYLITE_RUNTIME_MYLITE_SHOW_H

#include <mylite/mylite.h>

int mylite_show_engines_sql(mylite_db *database, char **out_sql);
int mylite_show_information_schema_engines_sql(mylite_db *database, char **out_sql);
const char *mylite_show_schemas_sql(void);
const char *mylite_show_information_schema_schemata_sql(void);
int mylite_show_attach_engines_result_metadata(mylite_db *database, mylite_stmt *stmt);

#endif
