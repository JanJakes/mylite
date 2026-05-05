#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_PLAN_LOOKUP_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_PLAN_LOOKUP_H

struct mylite_create_table_column;
struct mylite_create_table_plan;

const struct mylite_create_table_column *
mylite_table_ddl_find_create_table_column(const struct mylite_create_table_plan *plan,
                                          const char *name);

#endif
