#ifndef MYLITE_RUNTIME_MYLITE_SELECT_SQL_H
#define MYLITE_RUNTIME_MYLITE_SELECT_SQL_H

#include <mylite/mylite.h>

struct mylite_select_plan;
struct mylite_select_table;

char *mylite_select_build_physical_sql(mylite_db *database, const struct mylite_select_plan *plan);
char *mylite_select_build_scan_sql(mylite_db *database, const struct mylite_select_plan *plan);
char *mylite_select_build_table_scan_sql(
    mylite_db *database,
    const struct mylite_select_table *table
);
char *mylite_select_build_table_rowid_scan_sql(
    mylite_db *database,
    const struct mylite_select_table *table
);

#endif
