#ifndef MYLITE_RUNTIME_MYLITE_SHOW_H
#define MYLITE_RUNTIME_MYLITE_SHOW_H

#include <mylite/mylite.h>

struct mylite_show_character_set_query;
struct mylite_show_collation_query;
struct mylite_show_status_query;
struct mylite_show_variables_query;

int mylite_show_engines_sql(mylite_db *database, char **out_sql);
int mylite_show_information_schema_engines_sql(mylite_db *database, char **out_sql);
int mylite_show_character_set_sql(mylite_db *database,
                                  const struct mylite_show_character_set_query *query,
                                  char **out_sql);
int mylite_show_information_schema_character_sets_sql(mylite_db *database, char **out_sql);
int mylite_show_collation_sql(mylite_db *database, const struct mylite_show_collation_query *query,
                              char **out_sql);
int mylite_show_information_schema_collations_sql(mylite_db *database, char **out_sql);
int mylite_show_information_schema_collation_character_set_applicability_sql(mylite_db *database,
                                                                             char **out_sql);
int mylite_show_status_sql(mylite_db *database, const struct mylite_show_status_query *query,
                           char **out_sql);
int mylite_show_variables_sql(mylite_db *database, const struct mylite_show_variables_query *query,
                              char **out_sql);
const char *mylite_show_schemas_sql(void);
const char *mylite_show_information_schema_schemata_sql(void);
int mylite_show_attach_engines_result_metadata(mylite_db *database, mylite_stmt *stmt);

#endif
