#include "mylite_show.h"

#include "mylite_diagnostics.h"
#include "mylite_field_descriptor.h"
#include "mylite_metadata.h"
#include "mylite_metadata_constants.h"
#include "mylite_show_types.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "sqlite3.h"

#include <stdint.h>
#include <stdlib.h>

static bool show_diagnostics_query_from_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_show_diagnostics_query *out_query
);

static const struct mylite_sql_ast_node *show_filter_where_expression(
    const struct mylite_sql_ast_node *filter
);

static char *copy_show_variables_like_pattern(const struct mylite_sql_ast_node *filter);

static char *copy_show_status_like_pattern(const struct mylite_sql_ast_node *filter);

static char *copy_show_character_set_like_pattern(const struct mylite_sql_ast_node *filter);

static char *copy_show_collation_like_pattern(const struct mylite_sql_ast_node *filter);

static bool show_like_uses_backslash_escape(const struct mylite_sql_ast_node *filter);

static bool decode_show_string_escape(char escaped, char *out_character);

static int attach_show_diagnostics_result_metadata(mylite_db *database, mylite_stmt *stmt);

static struct mylite_field_descriptor show_diagnostics_descriptor(
    int type,
    uint64_t length,
    unsigned int flags,
    unsigned int decimals,
    unsigned int charset_id,
    bool nullable
);

static int attach_show_diagnostics_count_result_metadata(
    mylite_db *database,
    mylite_stmt *stmt,
    enum mylite_sql_ast_show_diagnostics_kind kind
);

static int attach_show_variables_result_metadata(
    mylite_db *database,
    mylite_stmt *stmt,
    enum mylite_sql_ast_show_variables_scope scope
);

static const char *show_variables_metadata_table_name(
    enum mylite_sql_ast_show_variables_scope scope
);

static int attach_show_status_result_metadata(
    mylite_db *database,
    mylite_stmt *stmt,
    enum mylite_sql_ast_show_status_scope scope
);

static const char *show_status_metadata_table_name(enum mylite_sql_ast_show_status_scope scope);

static struct mylite_field_descriptor show_performance_schema_string_descriptor(
    uint64_t length,
    unsigned int flags,
    bool nullable
);

static int attach_show_character_set_result_metadata(mylite_db *database, mylite_stmt *stmt);

static int attach_show_collation_result_metadata(mylite_db *database, mylite_stmt *stmt);

static struct mylite_field_descriptor show_schema_descriptor(
    int type,
    uint64_t length,
    unsigned int flags,
    bool nullable
);

int mylite_show_prepare_diagnostics_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    struct mylite_show_diagnostics_query query = {0};
    char *sqlite_sql = NULL;
    int status = MYLITE_OK;

    *out_stmt = NULL;
    if (!show_diagnostics_query_from_statement(statement, &query)) {
        return MYLITE_UNSUPPORTED;
    }

    sqlite_sql = mylite_show_diagnostics_sql(database, &query);
    if (sqlite_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    if (status == MYLITE_OK) {
        status = attach_show_diagnostics_result_metadata(database, *out_stmt);
    }
    if (status == MYLITE_OK) {
        (*out_stmt)->preserve_prepare_warnings = true;
    }
    if (status != MYLITE_OK) {
        mylite_finalize(*out_stmt);
        *out_stmt = NULL;
    }
    sqlite3_free(sqlite_sql);
    return status;
}

int mylite_show_prepare_diagnostics_count_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    char *sqlite_sql =
        mylite_show_diagnostics_count_sql(database, statement->show_diagnostics_kind);
    int status = MYLITE_OK;

    *out_stmt = NULL;
    if (sqlite_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    if (status == MYLITE_OK) {
        status = attach_show_diagnostics_count_result_metadata(
            database,
            *out_stmt,
            statement->show_diagnostics_kind
        );
    }
    if (status == MYLITE_OK) {
        (*out_stmt)->preserve_prepare_warnings = true;
    }
    if (status != MYLITE_OK) {
        mylite_finalize(*out_stmt);
        *out_stmt = NULL;
    }
    sqlite3_free(sqlite_sql);
    return status;
}

int mylite_show_prepare_variables_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    const struct mylite_sql_ast_node *filter = mylite_ast_child_at(statement, 0U);
    char *like_pattern = NULL;
    char *sqlite_sql = NULL;
    int status = MYLITE_OK;

    *out_stmt = NULL;
    like_pattern = copy_show_variables_like_pattern(filter);
    if (filter != NULL && filter->kind == MYLITE_SQL_AST_LITERAL && like_pattern == NULL) {
        status = MYLITE_NOMEM;
    }
    if (status == MYLITE_OK) {
        status = mylite_show_variables_sql(
            database,
            &(const struct mylite_show_variables_query){
                .scope = statement->show_variables_scope,
                .like_pattern = like_pattern,
                .where_expression = show_filter_where_expression(filter),
                .like_escape_backslash = show_like_uses_backslash_escape(filter),
            },
            &sqlite_sql
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }
    if (status == MYLITE_OK) {
        status = attach_show_variables_result_metadata(
            database,
            *out_stmt,
            statement->show_variables_scope
        );
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    if (status != MYLITE_OK) {
        mylite_finalize(*out_stmt);
        *out_stmt = NULL;
    }
    free(like_pattern);
    sqlite3_free(sqlite_sql);
    return status;
}

int mylite_show_prepare_status_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    const struct mylite_sql_ast_node *filter = mylite_ast_child_at(statement, 0U);
    char *like_pattern = NULL;
    char *sqlite_sql = NULL;
    int status = MYLITE_OK;

    *out_stmt = NULL;
    like_pattern = copy_show_status_like_pattern(filter);
    if (filter != NULL && filter->kind == MYLITE_SQL_AST_LITERAL && like_pattern == NULL) {
        status = MYLITE_NOMEM;
    }
    if (status == MYLITE_OK) {
        status = mylite_show_status_sql(
            database,
            &(const struct mylite_show_status_query){
                .scope = statement->show_status_scope,
                .like_pattern = like_pattern,
                .where_expression = show_filter_where_expression(filter),
                .like_escape_backslash = show_like_uses_backslash_escape(filter),
            },
            &sqlite_sql
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }
    if (status == MYLITE_OK) {
        status =
            attach_show_status_result_metadata(database, *out_stmt, statement->show_status_scope);
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    if (status != MYLITE_OK) {
        mylite_finalize(*out_stmt);
        *out_stmt = NULL;
    }
    free(like_pattern);
    sqlite3_free(sqlite_sql);
    return status;
}

int mylite_show_prepare_character_set_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    const struct mylite_sql_ast_node *filter = mylite_ast_child_at(statement, 0U);
    char *like_pattern = NULL;
    char *sqlite_sql = NULL;
    int status = MYLITE_OK;

    *out_stmt = NULL;
    like_pattern = copy_show_character_set_like_pattern(filter);
    if (filter != NULL && filter->kind == MYLITE_SQL_AST_LITERAL && like_pattern == NULL) {
        status = MYLITE_NOMEM;
    }
    if (status == MYLITE_OK) {
        status = mylite_show_character_set_sql(
            database,
            &(const struct mylite_show_character_set_query){
                .like_pattern = like_pattern,
                .where_expression = show_filter_where_expression(filter),
                .like_escape_backslash = show_like_uses_backslash_escape(filter),
            },
            &sqlite_sql
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }
    if (status == MYLITE_OK) {
        status = attach_show_character_set_result_metadata(database, *out_stmt);
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    if (status != MYLITE_OK) {
        mylite_finalize(*out_stmt);
        *out_stmt = NULL;
    }
    free(like_pattern);
    sqlite3_free(sqlite_sql);
    return status;
}

int mylite_show_prepare_collation_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    const struct mylite_sql_ast_node *filter = mylite_ast_child_at(statement, 0U);
    char *like_pattern = NULL;
    char *sqlite_sql = NULL;
    int status = MYLITE_OK;

    *out_stmt = NULL;
    like_pattern = copy_show_collation_like_pattern(filter);
    if (filter != NULL && filter->kind == MYLITE_SQL_AST_LITERAL && like_pattern == NULL) {
        status = MYLITE_NOMEM;
    }
    if (status == MYLITE_OK) {
        status = mylite_show_collation_sql(
            database,
            &(const struct mylite_show_collation_query){
                .like_pattern = like_pattern,
                .where_expression = show_filter_where_expression(filter),
                .like_escape_backslash = show_like_uses_backslash_escape(filter),
            },
            &sqlite_sql
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }
    if (status == MYLITE_OK) {
        status = attach_show_collation_result_metadata(database, *out_stmt);
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    if (status != MYLITE_OK) {
        mylite_finalize(*out_stmt);
        *out_stmt = NULL;
    }
    free(like_pattern);
    sqlite3_free(sqlite_sql);
    return status;
}

static int attach_show_diagnostics_result_metadata(mylite_db *database, mylite_stmt *stmt) {
    const struct mylite_result_column_metadata_spec columns[] = {
        {.name = "Level",
         .descriptor = show_diagnostics_descriptor(
             MYLITE_FIELD_TYPE_VAR_STRING,
             7U,
             MYLITE_FIELD_FLAG_NOT_NULL,
             mylite_mysql_not_fixed_decimals,
             mylite_mysql_latin1_swedish_ci_charset_id,
             false
         )},
        {.name = "Code",
         .descriptor = show_diagnostics_descriptor(
             MYLITE_FIELD_TYPE_LONG,
             5U,
             MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_BINARY |
                 MYLITE_FIELD_FLAG_NUM,
             0U,
             mylite_mysql_binary_charset_id,
             false
         )},
        {.name = "Message",
         .descriptor = show_diagnostics_descriptor(
             MYLITE_FIELD_TYPE_VAR_STRING,
             512U,
             MYLITE_FIELD_FLAG_NOT_NULL,
             mylite_mysql_not_fixed_decimals,
             mylite_mysql_latin1_swedish_ci_charset_id,
             false
         )},
    };

    return mylite_result_metadata_attach_columns(
        database,
        stmt,
        columns,
        sizeof(columns) / sizeof(columns[0])
    );
}

static struct mylite_field_descriptor show_diagnostics_descriptor(
    int type,
    uint64_t length,
    unsigned int flags,
    unsigned int decimals,
    unsigned int charset_id,
    bool nullable
) {
    struct mylite_field_descriptor descriptor = {
        .type = type,
        .flags = flags,
        .length = length,
        .decimals = decimals,
        .charset_id = charset_id,
        .nullable = nullable,
    };

    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}

static int attach_show_diagnostics_count_result_metadata(
    mylite_db *database,
    mylite_stmt *stmt,
    enum mylite_sql_ast_show_diagnostics_kind kind
) {
    const char *name = kind == MYLITE_SQL_AST_SHOW_DIAGNOSTICS_ERRORS ? "@@session.error_count"
                                                                      : "@@session.warning_count";
    const struct mylite_result_column_metadata_spec columns[] = {
        {.name = name,
         .descriptor = show_diagnostics_descriptor(
             MYLITE_FIELD_TYPE_LONGLONG,
             21U,
             MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
             0U,
             mylite_mysql_binary_charset_id,
             true
         )},
    };

    return mylite_result_metadata_attach_columns(
        database,
        stmt,
        columns,
        sizeof(columns) / sizeof(columns[0])
    );
}

static int attach_show_variables_result_metadata(
    mylite_db *database,
    mylite_stmt *stmt,
    enum mylite_sql_ast_show_variables_scope scope
) {
    const char *table_name = show_variables_metadata_table_name(scope);
    const struct mylite_result_column_metadata_spec columns[] = {
        {.name = "Variable_name",
         .schema_name = "performance_schema",
         .table_name = table_name,
         .origin_schema_name = "performance_schema",
         .origin_table_name = table_name,
         .origin_column_name = "Variable_name",
         .descriptor = show_performance_schema_string_descriptor(
             64U,
             MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
             false
         )},
        {.name = "Value",
         .schema_name = "performance_schema",
         .table_name = table_name,
         .origin_schema_name = "performance_schema",
         .origin_table_name = table_name,
         .origin_column_name = "Value",
         .descriptor = show_performance_schema_string_descriptor(1024U, 0U, true)},
    };

    return mylite_result_metadata_attach_columns(
        database,
        stmt,
        columns,
        sizeof(columns) / sizeof(columns[0])
    );
}

static const char *show_variables_metadata_table_name(
    enum mylite_sql_ast_show_variables_scope scope
) {
    return scope == MYLITE_SQL_AST_SHOW_VARIABLES_GLOBAL ? "global_variables" : "session_variables";
}

static int attach_show_status_result_metadata(
    mylite_db *database,
    mylite_stmt *stmt,
    enum mylite_sql_ast_show_status_scope scope
) {
    const char *table_name = show_status_metadata_table_name(scope);
    const struct mylite_result_column_metadata_spec columns[] = {
        {.name = "Variable_name",
         .schema_name = "performance_schema",
         .table_name = table_name,
         .origin_schema_name = "performance_schema",
         .origin_table_name = table_name,
         .origin_column_name = "Variable_name",
         .descriptor = show_performance_schema_string_descriptor(
             64U,
             MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
             false
         )},
        {.name = "Value",
         .schema_name = "performance_schema",
         .table_name = table_name,
         .origin_schema_name = "performance_schema",
         .origin_table_name = table_name,
         .origin_column_name = "Value",
         .descriptor = show_performance_schema_string_descriptor(1024U, 0U, true)},
    };

    return mylite_result_metadata_attach_columns(
        database,
        stmt,
        columns,
        sizeof(columns) / sizeof(columns[0])
    );
}

static const char *show_status_metadata_table_name(enum mylite_sql_ast_show_status_scope scope) {
    return scope == MYLITE_SQL_AST_SHOW_STATUS_GLOBAL ? "global_status" : "session_status";
}

static struct mylite_field_descriptor show_performance_schema_string_descriptor(
    uint64_t length,
    unsigned int flags,
    bool nullable
) {
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = flags,
        .length = length,
        .decimals = 0U,
        .charset_id = mylite_mysql_latin1_swedish_ci_charset_id,
        .nullable = nullable,
    };

    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}

static int attach_show_character_set_result_metadata(mylite_db *database, mylite_stmt *stmt) {
    const struct mylite_result_column_metadata_spec columns[] = {
        {.name = "Charset",
         .schema_name = "information_schema",
         .table_name = "CHARACTER_SETS",
         .origin_schema_name = "information_schema",
         .origin_table_name = "cs",
         .origin_column_name = "Charset",
         .descriptor = show_schema_descriptor(
             MYLITE_FIELD_TYPE_VAR_STRING,
             64U,
             MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNIQUE_KEY |
                 MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE | MYLITE_FIELD_FLAG_PART_KEY,
             false
         )},
        {.name = "Description",
         .schema_name = "information_schema",
         .table_name = "CHARACTER_SETS",
         .origin_schema_name = "information_schema",
         .origin_table_name = "cs",
         .origin_column_name = "Description",
         .descriptor = show_schema_descriptor(
             MYLITE_FIELD_TYPE_VAR_STRING,
             2048U,
             MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
             false
         )},
        {.name = "Default collation",
         .schema_name = "information_schema",
         .table_name = "CHARACTER_SETS",
         .origin_schema_name = "information_schema",
         .origin_table_name = "col",
         .origin_column_name = "Default collation",
         .descriptor = show_schema_descriptor(
             MYLITE_FIELD_TYPE_VAR_STRING,
             64U,
             MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNIQUE_KEY |
                 MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE | MYLITE_FIELD_FLAG_PART_KEY,
             false
         )},
        {.name = "Maxlen",
         .schema_name = "information_schema",
         .table_name = "CHARACTER_SETS",
         .origin_schema_name = "information_schema",
         .origin_table_name = "cs",
         .origin_column_name = "Maxlen",
         .descriptor = show_diagnostics_descriptor(
             MYLITE_FIELD_TYPE_LONG,
             10U,
             MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED |
                 MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE | MYLITE_FIELD_FLAG_NUM,
             0U,
             mylite_mysql_binary_charset_id,
             false
         )},
    };

    return mylite_result_metadata_attach_columns(
        database,
        stmt,
        columns,
        sizeof(columns) / sizeof(columns[0])
    );
}

static int attach_show_collation_result_metadata(mylite_db *database, mylite_stmt *stmt) {
    const struct mylite_result_column_metadata_spec columns[] = {
        {.name = "Collation",
         .schema_name = "information_schema",
         .table_name = "COLLATIONS",
         .origin_schema_name = "information_schema",
         .origin_table_name = "COLLATIONS",
         .origin_column_name = "Collation",
         .descriptor = show_schema_descriptor(
             MYLITE_FIELD_TYPE_VAR_STRING,
             64U,
             MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
             false
         )},
        {.name = "Charset",
         .schema_name = "information_schema",
         .table_name = "COLLATIONS",
         .origin_schema_name = "information_schema",
         .origin_table_name = "COLLATIONS",
         .origin_column_name = "Charset",
         .descriptor = show_schema_descriptor(
             MYLITE_FIELD_TYPE_VAR_STRING,
             64U,
             MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
             false
         )},
        {.name = "Id",
         .schema_name = "information_schema",
         .table_name = "COLLATIONS",
         .origin_schema_name = "information_schema",
         .origin_table_name = "COLLATIONS",
         .origin_column_name = "Id",
         .descriptor = show_diagnostics_descriptor(
             MYLITE_FIELD_TYPE_LONGLONG,
             20U,
             MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_NUM,
             0U,
             mylite_mysql_binary_charset_id,
             false
         )},
        {.name = "Default",
         .schema_name = "information_schema",
         .table_name = "COLLATIONS",
         .origin_schema_name = "information_schema",
         .origin_table_name = "COLLATIONS",
         .origin_column_name = "Default",
         .descriptor = show_schema_descriptor(
             MYLITE_FIELD_TYPE_VAR_STRING,
             3U,
             MYLITE_FIELD_FLAG_NOT_NULL,
             false
         )},
        {.name = "Compiled",
         .schema_name = "information_schema",
         .table_name = "COLLATIONS",
         .origin_schema_name = "information_schema",
         .origin_table_name = "COLLATIONS",
         .origin_column_name = "Compiled",
         .descriptor = show_schema_descriptor(
             MYLITE_FIELD_TYPE_VAR_STRING,
             3U,
             MYLITE_FIELD_FLAG_NOT_NULL,
             false
         )},
        {.name = "Sortlen",
         .schema_name = "information_schema",
         .table_name = "COLLATIONS",
         .origin_schema_name = "information_schema",
         .origin_table_name = "COLLATIONS",
         .origin_column_name = "Sortlen",
         .descriptor = show_diagnostics_descriptor(
             MYLITE_FIELD_TYPE_LONG,
             10U,
             MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED |
                 MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE | MYLITE_FIELD_FLAG_NUM,
             0U,
             mylite_mysql_binary_charset_id,
             false
         )},
        {.name = "Pad_attribute",
         .schema_name = "information_schema",
         .table_name = "COLLATIONS",
         .origin_schema_name = "information_schema",
         .origin_table_name = "COLLATIONS",
         .origin_column_name = "Pad_attribute",
         .descriptor = show_schema_descriptor(
             MYLITE_FIELD_TYPE_STRING,
             9U,
             MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_ENUM |
                 MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
             false
         )},
    };

    return mylite_result_metadata_attach_columns(
        database,
        stmt,
        columns,
        sizeof(columns) / sizeof(columns[0])
    );
}

static struct mylite_field_descriptor show_schema_descriptor(
    int type,
    uint64_t length,
    unsigned int flags,
    bool nullable
) {
    struct mylite_field_descriptor descriptor = {
        .type = type,
        .flags = flags,
        .length = length,
        .decimals = 0U,
        .charset_id = mylite_mysql_latin1_swedish_ci_charset_id,
        .nullable = nullable,
    };

    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}

static bool show_diagnostics_query_from_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_show_diagnostics_query *out_query
) {
    const struct mylite_sql_ast_node *limit = mylite_ast_child_at(statement, 0U);

    *out_query = (struct mylite_show_diagnostics_query){
        .kind = statement->show_diagnostics_kind,
        .offset = 0U,
        .row_count = UINT64_MAX,
        .has_limit = false,
    };

    if (limit == NULL) {
        return true;
    }
    if (limit->kind != MYLITE_SQL_AST_LIMIT_CLAUSE ||
        mylite_sql_ast_node_child_count(limit) != 2U) {
        return false;
    }
    out_query->offset = mylite_ast_child_at(limit, 0U)->limit_bound_value;
    out_query->row_count = mylite_ast_child_at(limit, 1U)->limit_bound_value;
    out_query->has_limit = true;
    return true;
}

static const struct mylite_sql_ast_node *show_filter_where_expression(
    const struct mylite_sql_ast_node *filter
) {
    if (filter == NULL || filter->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        return NULL;
    }
    return mylite_ast_child_at(filter, 0U);
}

static char *copy_show_variables_like_pattern(const struct mylite_sql_ast_node *filter) {
    if (filter == NULL || filter->kind != MYLITE_SQL_AST_LITERAL) {
        return NULL;
    }
    return mylite_show_copy_like_pattern_span(filter);
}

static char *copy_show_status_like_pattern(const struct mylite_sql_ast_node *filter) {
    if (filter == NULL || filter->kind != MYLITE_SQL_AST_LITERAL) {
        return NULL;
    }
    return mylite_show_copy_like_pattern_span(filter);
}

static char *copy_show_character_set_like_pattern(const struct mylite_sql_ast_node *filter) {
    if (filter == NULL || filter->kind != MYLITE_SQL_AST_LITERAL) {
        return NULL;
    }
    return mylite_show_copy_like_pattern_span(filter);
}

static char *copy_show_collation_like_pattern(const struct mylite_sql_ast_node *filter) {
    if (filter == NULL || filter->kind != MYLITE_SQL_AST_LITERAL) {
        return NULL;
    }
    return mylite_show_copy_like_pattern_span(filter);
}

static bool show_like_uses_backslash_escape(const struct mylite_sql_ast_node *filter) {
    return filter != NULL && filter->kind == MYLITE_SQL_AST_LITERAL &&
           !filter->no_backslash_escapes;
}

char *mylite_show_copy_like_pattern_span(const struct mylite_sql_ast_node *node) {
    const char *text = node == NULL ? NULL : node->span.text;
    size_t length = node == NULL ? 0U : node->span.length;
    size_t start = 0U;
    size_t end = length;
    char *copy = NULL;
    size_t output = 0U;

    if (text == NULL) {
        return NULL;
    }
    if (length >= 2U && (text[0] == '\'' || text[0] == '"')) {
        start = 1U;
        end = length - 1U;
    } else if (
        length >= 3U && (text[0] == 'N' || text[0] == 'n') && (text[1] == '\'' || text[1] == '"')
    ) {
        start = 2U;
        end = length - 1U;
    }

    copy = malloc(end >= start ? end - start + 1U : 1U);
    if (copy == NULL) {
        return NULL;
    }

    for (size_t index = start; index < end; ++index) {
        if (!node->no_backslash_escapes && text[index] == '\\' && index + 1U < end) {
            char escaped = '\0';

            if (decode_show_string_escape(text[index + 1U], &escaped)) {
                copy[output++] = escaped;
                ++index;
            } else {
                copy[output++] = text[index];
            }
        } else if (
            (text[index] == '\'' || text[index] == '"') && index + 1U < end &&
            text[index + 1U] == text[index]
        ) {
            copy[output++] = text[index++];
        } else {
            copy[output++] = text[index];
        }
    }
    copy[output] = '\0';
    return copy;
}

static bool decode_show_string_escape(char escaped, char *out_character) {
    switch (escaped) {
    case '\'':
    case '"':
    case '\\':
        *out_character = escaped;
        return true;
    case 'b':
        *out_character = '\b';
        return true;
    case 'n':
        *out_character = '\n';
        return true;
    case 'r':
        *out_character = '\r';
        return true;
    case 't':
        *out_character = '\t';
        return true;
    case '0':
        *out_character = '\0';
        return true;
    case 'Z':
        *out_character = '\x1a';
        return true;
    default:
        return false;
    }
}
