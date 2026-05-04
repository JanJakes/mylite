#ifndef MYLITE_RUNTIME_MYLITE_SHOW_TYPES_H
#define MYLITE_RUNTIME_MYLITE_SHOW_TYPES_H

#include "sql/mylite_ast.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>

struct mylite_show_variables_query {
    enum mylite_sql_ast_show_variables_scope scope;
    const char *like_pattern;
};

struct mylite_show_status_query {
    enum mylite_sql_ast_show_status_scope scope;
    const char *like_pattern;
};

struct mylite_storage_engine_row {
    const char *engine;
    const char *support;
    const char *comment;
    const char *transactions;
    const char *xa;
    const char *savepoints;
};

struct mylite_storage_engine_columns {
    const char *engine;
    const char *support;
    const char *comment;
    const char *transactions;
    const char *xa;
    const char *savepoints;
};

struct mylite_show_engines_metadata_column {
    const char *name;
    uint64_t length;
    bool nullable;
};

struct mylite_show_character_set_query {
    const char *like_pattern;
};

struct mylite_show_collation_query {
    const char *like_pattern;
};

struct mylite_show_tables_query {
    const char *schema_name;
    const char *column_name;
    const char *glob_pattern;
    bool full;
};

struct mylite_show_table_status_query {
    const char *schema_name;
    const char *glob_pattern;
};

struct mylite_show_columns_target {
    char *schema_name;
    char *table_name;
};

struct mylite_show_columns_source_nodes {
    const struct mylite_sql_ast_node *table_name;
    const struct mylite_sql_ast_node *explicit_schema;
};

struct mylite_show_columns_query {
    const char *schema_name;
    const char *table_name;
    const char *like_pattern;
    bool full;
};

struct mylite_show_index_target {
    char *schema_name;
    char *table_name;
};

struct mylite_show_index_source_nodes {
    const struct mylite_sql_ast_node *table_name;
    const struct mylite_sql_ast_node *explicit_schema;
};

struct mylite_show_index_query {
    const char *schema_name;
    const char *table_name;
};

struct mylite_show_create_table_target {
    char *schema_name;
    char *table_name;
};

struct mylite_show_create_schema_info {
    char *name;
    char *character_set;
    char *collation;
    char *encryption;
};

struct mylite_show_diagnostics_query {
    enum mylite_sql_ast_show_diagnostics_kind kind;
    uint64_t offset;
    uint64_t row_count;
    bool has_limit;
};

struct mylite_show_create_table_info {
    char *engine;
    bool has_auto_increment;
    sqlite3_int64 auto_increment;
    char *table_collation;
    char *table_comment;
};

struct mylite_show_create_column_collation {
    const char *character_set_name;
    const char *column_collation;
    const char *table_collation;
};

#endif
