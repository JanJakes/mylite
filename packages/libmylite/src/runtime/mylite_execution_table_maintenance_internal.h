#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_TABLE_MAINTENANCE_INTERNAL_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_TABLE_MAINTENANCE_INTERNAL_H

enum {
    table_maintenance_result_column_count = 4,
    checksum_table_result_column_count = 2,
    checksum_table_name_display_length = 384,
    checksum_table_checksum_display_length = 22,
    table_maintenance_mysql_binary_collation_id = 63,
    table_maintenance_mysql_approximate_decimals = 31,
};

enum table_maintenance_operation {
    TABLE_MAINTENANCE_ANALYZE = 0,
    TABLE_MAINTENANCE_CHECK = 1,
    TABLE_MAINTENANCE_OPTIMIZE = 2,
    TABLE_MAINTENANCE_REPAIR = 3,
};

enum checksum_table_option {
    CHECKSUM_TABLE_OPTION_DEFAULT = 0,
    CHECKSUM_TABLE_OPTION_QUICK = 1,
    CHECKSUM_TABLE_OPTION_EXTENDED = 2,
};

struct table_maintenance_target {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char display_name[(MYLITE_CATALOG_IDENTIFIER_CAPACITY * 2) + 2];
    struct mylite_catalog_table_descriptor table;
    bool missing_schema;
    bool missing_table;
    bool unsupported_kind;
    bool has_table;
};

static int execute_checksum_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int append_checksum_table_result_columns(struct mylite_db *database, mylite_result *result);
static int append_checksum_table_target_row(
    struct mylite_db *database,
    mylite_result *result,
    const struct table_maintenance_target *target,
    enum checksum_table_option option
);
static enum checksum_table_option checksum_table_option_for_statement(
    const struct mylite_sql_ast_node *statement
);
static int checksum_table_target_value(
    struct mylite_db *database,
    const struct table_maintenance_target *target,
    enum checksum_table_option option,
    char *value_buffer,
    size_t value_buffer_size,
    const char **out_value
);
static int compute_checksum_table_target_value(
    struct mylite_db *database,
    const struct table_maintenance_target *target,
    char *value_buffer,
    size_t value_buffer_size,
    const char **out_value
);
static int build_checksum_table_scan_sql(
    struct mylite_db *database,
    const struct table_maintenance_target *target,
    char **out_sql
);
static int scan_checksum_table_rows(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    uint64_t *out_checksum,
    bool *out_has_rows
);
static void checksum_table_update_byte(uint64_t *checksum, unsigned char byte);
static void checksum_table_update_bytes(
    uint64_t *checksum,
    const unsigned char *bytes,
    size_t byte_count
);
static void checksum_table_update_u64(uint64_t *checksum, uint64_t value);
static int append_checksum_table_target_warning(
    struct mylite_db *database,
    const struct table_maintenance_target *target
);
static int append_checksum_table_warning(
    struct mylite_db *database,
    int code,
    const char *sqlstate,
    const char *message
);
static int execute_table_maintenance_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static enum table_maintenance_operation table_maintenance_operation_for_statement(
    const struct mylite_sql_ast_node *statement
);
static int append_table_maintenance_result_columns(
    struct mylite_db *database,
    mylite_result *result
);
static int append_table_maintenance_target_rows(
    struct mylite_db *database,
    mylite_result *result,
    enum table_maintenance_operation operation,
    const struct table_maintenance_target *target
);
static int append_table_maintenance_success_rows(
    struct mylite_db *database,
    mylite_result *result,
    enum table_maintenance_operation operation,
    const struct table_maintenance_target *target
);
static int append_table_maintenance_unknown_schema_rows(
    struct mylite_db *database,
    mylite_result *result,
    enum table_maintenance_operation operation,
    const struct table_maintenance_target *target
);
static int append_table_maintenance_unknown_table_rows(
    struct mylite_db *database,
    mylite_result *result,
    enum table_maintenance_operation operation,
    const struct table_maintenance_target *target
);
static int append_table_maintenance_row(
    struct mylite_db *database,
    mylite_result *result,
    const char *table_name,
    const char *operation,
    const char *message_type,
    const char *message_text
);
static int resolve_table_maintenance_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_maintenance_target *out_target
);
static int check_table_maintenance_duplicate_target(
    struct mylite_db *database,
    const struct table_maintenance_target *targets,
    size_t target_count,
    const struct table_maintenance_target *target
);
static int format_table_maintenance_display_name(
    struct mylite_db *database,
    struct table_maintenance_target *target
);
static const char *table_maintenance_operation_text(enum table_maintenance_operation operation);

#endif
