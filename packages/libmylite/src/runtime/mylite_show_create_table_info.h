#ifndef MYLITE_RUNTIME_MYLITE_SHOW_CREATE_TABLE_INFO_H
#define MYLITE_RUNTIME_MYLITE_SHOW_CREATE_TABLE_INFO_H

#include "sqlite3.h"

#include <mylite/mylite.h>

#include <stdbool.h>

struct mylite_show_create_table_target;

struct mylite_show_create_table_info {
    char *engine;
    bool has_auto_increment;
    sqlite3_int64 auto_increment;
    char *table_collation;
    char *table_comment;
};

int mylite_show_create_table_read_info(mylite_db *database,
                                       const struct mylite_show_create_table_target *target,
                                       struct mylite_show_create_table_info *out_info);
void mylite_show_create_table_info_deinit(struct mylite_show_create_table_info *info);

#endif
