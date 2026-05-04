#ifndef MYLITE_RUNTIME_MYLITE_STORAGE_ENGINE_H
#define MYLITE_RUNTIME_MYLITE_STORAGE_ENGINE_H

#include <mylite/mylite.h>

int mylite_storage_engine_show_sql(mylite_db *database, char **out_sql);
int mylite_storage_engine_information_schema_sql(mylite_db *database, char **out_sql);

#endif
