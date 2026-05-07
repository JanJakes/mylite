#include "mylite_show.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_field_descriptor.h"
#include "mylite_metadata.h"
#include "mylite_metadata_constants.h"
#include "mylite_runtime.h"
#include "mylite_show_types.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "sqlite3.h"

#include <stdlib.h>
#include <string.h>

static int copy_show_tables_schema_name(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    char **out_schema_name
);

static int validate_show_tables_schema(mylite_db *database, const char *schema_name);

static int attach_show_tables_result_metadata(
    mylite_db *database,
    mylite_stmt *stmt,
    const char *column_name,
    bool full
);

static struct mylite_field_descriptor show_tables_latin1_descriptor(
    int type,
    uint64_t length,
    unsigned int flags,
    bool nullable
);

static int attach_show_table_status_result_metadata(mylite_db *database, mylite_stmt *stmt);

static struct mylite_field_descriptor show_table_status_comment_descriptor(void);

static struct mylite_field_descriptor show_table_status_descriptor(
    int type,
    uint64_t length,
    unsigned int flags,
    bool nullable
);

static char *copy_show_tables_like_pattern(const struct mylite_sql_ast_node *statement);

static const struct mylite_sql_ast_node *show_tables_filter(
    const struct mylite_sql_ast_node *statement
);

static const struct mylite_sql_ast_node *show_tables_where_expression(
    const struct mylite_sql_ast_node *statement
);

static char *copy_show_tables_display_pattern(const char *like_pattern, bool uppercase_pattern);

static char *show_tables_column_name(const char *schema_name, const char *like_pattern);

static char *show_tables_glob_pattern(const char *like_pattern, bool escape_backslash);

static int normalize_show_tables_schema_name(char **schema_name);

static void append_show_tables_glob_literal(sqlite3_str *glob, char character);

int mylite_show_prepare_tables_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    char *schema_name = NULL;
    char *like_pattern = NULL;
    char *display_pattern = NULL;
    char *glob_pattern = NULL;
    char *column_name = NULL;
    char *sqlite_sql = NULL;
    int status = copy_show_tables_schema_name(database, statement, &schema_name);

    *out_stmt = NULL;
    if (status == MYLITE_OK) {
        status = validate_show_tables_schema(database, schema_name);
    }
    if (status == MYLITE_OK) {
        like_pattern = copy_show_tables_like_pattern(statement);
        const struct mylite_sql_ast_node *filter = show_tables_filter(statement);

        if (filter != NULL && filter->kind == MYLITE_SQL_AST_LITERAL && like_pattern == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK && like_pattern != NULL) {
        display_pattern = copy_show_tables_display_pattern(
            like_pattern,
            mylite_ascii_case_equal(schema_name, "information_schema")
        );
        if (display_pattern == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK && display_pattern != NULL) {
        glob_pattern = show_tables_glob_pattern(
            display_pattern,
            show_tables_filter(statement) != NULL &&
                !show_tables_filter(statement)->no_backslash_escapes
        );
        if (glob_pattern == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK) {
        column_name = show_tables_column_name(schema_name, display_pattern);
        if (column_name == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK) {
        status = mylite_show_tables_sql(
            database,
            &(const struct mylite_show_tables_query){
                .schema_name = schema_name,
                .column_name = column_name,
                .glob_pattern = glob_pattern,
                .where_expression = show_tables_where_expression(statement),
                .full = statement->show_tables_full,
            },
            &sqlite_sql
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }
    if (status == MYLITE_OK) {
        status = attach_show_tables_result_metadata(
            database,
            *out_stmt,
            column_name,
            statement->show_tables_full
        );
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    if (status != MYLITE_OK) {
        mylite_finalize(*out_stmt);
        *out_stmt = NULL;
    }
    free(schema_name);
    free(like_pattern);
    free(display_pattern);
    sqlite3_free(column_name);
    sqlite3_free(glob_pattern);
    sqlite3_free(sqlite_sql);
    return status;
}

int mylite_show_prepare_table_status_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    char *schema_name = NULL;
    char *like_pattern = NULL;
    char *display_pattern = NULL;
    char *glob_pattern = NULL;
    char *sqlite_sql = NULL;
    int status = copy_show_tables_schema_name(database, statement, &schema_name);

    *out_stmt = NULL;
    if (status == MYLITE_OK) {
        status = validate_show_tables_schema(database, schema_name);
    }
    if (status == MYLITE_OK) {
        like_pattern = copy_show_tables_like_pattern(statement);
        const struct mylite_sql_ast_node *filter = show_tables_filter(statement);

        if (filter != NULL && filter->kind == MYLITE_SQL_AST_LITERAL && like_pattern == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK && like_pattern != NULL) {
        display_pattern = copy_show_tables_display_pattern(
            like_pattern,
            mylite_ascii_case_equal(schema_name, "information_schema")
        );
        if (display_pattern == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK && display_pattern != NULL) {
        glob_pattern = show_tables_glob_pattern(
            display_pattern,
            show_tables_filter(statement) != NULL &&
                !show_tables_filter(statement)->no_backslash_escapes
        );
        if (glob_pattern == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK) {
        status = mylite_show_table_status_sql(
            database,
            &(const struct mylite_show_table_status_query){
                .schema_name = schema_name,
                .glob_pattern = glob_pattern,
                .where_expression = show_tables_where_expression(statement),
            },
            &sqlite_sql
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }
    if (status == MYLITE_OK) {
        status = attach_show_table_status_result_metadata(database, *out_stmt);
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    if (status != MYLITE_OK) {
        mylite_finalize(*out_stmt);
        *out_stmt = NULL;
    }
    free(schema_name);
    free(like_pattern);
    free(display_pattern);
    sqlite3_free(glob_pattern);
    sqlite3_free(sqlite_sql);
    return status;
}

static int attach_show_tables_result_metadata(
    mylite_db *database,
    mylite_stmt *stmt,
    const char *column_name,
    bool full
) {
    size_t column_count = 1U;
    const struct mylite_result_column_metadata_spec columns[] = {
        {.name = column_name,
         .table_name = "TABLES",
         .origin_table_name = "tables",
         .descriptor = show_tables_latin1_descriptor(
             MYLITE_FIELD_TYPE_VAR_STRING,
             64U,
             MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY |
                 MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
             false
         )},
        {.name = "Table_type",
         .table_name = "TABLES",
         .origin_table_name = "tables",
         .descriptor = show_tables_latin1_descriptor(
             MYLITE_FIELD_TYPE_STRING,
             11U,
             MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_ENUM |
                 MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
             false
         )},
    };

    if (full) {
        column_count = 2U;
    }
    return mylite_result_metadata_attach_columns(database, stmt, columns, column_count);
}

static struct mylite_field_descriptor show_tables_latin1_descriptor(
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

static int attach_show_table_status_result_metadata(mylite_db *database, mylite_stmt *stmt) {
    const struct mylite_result_column_metadata_spec columns[] = {
        {.name = "Name",
         .table_name = "TABLES",
         .origin_table_name = "tables",
         .descriptor = show_table_status_descriptor(
             MYLITE_FIELD_TYPE_VAR_STRING,
             64U,
             MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY |
                 MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
             false
         )},
        {.name = "Engine",
         .table_name = "TABLES",
         .descriptor = show_table_status_descriptor(MYLITE_FIELD_TYPE_VAR_STRING, 64U, 0U, true)},
        {.name = "Version",
         .table_name = "TABLES",
         .descriptor =
             show_table_status_descriptor(MYLITE_FIELD_TYPE_LONG, 3U, MYLITE_FIELD_FLAG_NUM, true)},
        {.name = "Row_format",
         .table_name = "TABLES",
         .origin_table_name = "tables",
         .descriptor = show_table_status_descriptor(
             MYLITE_FIELD_TYPE_STRING,
             10U,
             MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_ENUM,
             true
         )},
        {.name = "Rows",
         .table_name = "TABLES",
         .descriptor = show_table_status_descriptor(
             MYLITE_FIELD_TYPE_LONGLONG,
             21U,
             MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_NUM,
             true
         )},
        {.name = "Avg_row_length",
         .table_name = "TABLES",
         .descriptor = show_table_status_descriptor(
             MYLITE_FIELD_TYPE_LONGLONG,
             21U,
             MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_NUM,
             true
         )},
        {.name = "Data_length",
         .table_name = "TABLES",
         .descriptor = show_table_status_descriptor(
             MYLITE_FIELD_TYPE_LONGLONG,
             21U,
             MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_NUM,
             true
         )},
        {.name = "Max_data_length",
         .table_name = "TABLES",
         .descriptor = show_table_status_descriptor(
             MYLITE_FIELD_TYPE_LONGLONG,
             21U,
             MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_NUM,
             true
         )},
        {.name = "Index_length",
         .table_name = "TABLES",
         .descriptor = show_table_status_descriptor(
             MYLITE_FIELD_TYPE_LONGLONG,
             21U,
             MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_NUM,
             true
         )},
        {.name = "Data_free",
         .table_name = "TABLES",
         .descriptor = show_table_status_descriptor(
             MYLITE_FIELD_TYPE_LONGLONG,
             21U,
             MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_NUM,
             true
         )},
        {.name = "Auto_increment",
         .table_name = "TABLES",
         .descriptor = show_table_status_descriptor(
             MYLITE_FIELD_TYPE_LONGLONG,
             21U,
             MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_NUM,
             true
         )},
        {.name = "Create_time",
         .table_name = "TABLES",
         .origin_table_name = "tables",
         .descriptor = show_table_status_descriptor(
             MYLITE_FIELD_TYPE_TIMESTAMP,
             19U,
             MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY |
                 MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
             false
         )},
        {.name = "Update_time",
         .table_name = "TABLES",
         .descriptor = show_table_status_descriptor(
             MYLITE_FIELD_TYPE_DATETIME,
             19U,
             MYLITE_FIELD_FLAG_BINARY,
             true
         )},
        {.name = "Check_time",
         .table_name = "TABLES",
         .descriptor = show_table_status_descriptor(
             MYLITE_FIELD_TYPE_DATETIME,
             19U,
             MYLITE_FIELD_FLAG_BINARY,
             true
         )},
        {.name = "Collation",
         .table_name = "TABLES",
         .origin_table_name = "collations",
         .descriptor = show_table_status_descriptor(
             MYLITE_FIELD_TYPE_VAR_STRING,
             64U,
             MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
             true
         )},
        {.name = "Checksum",
         .table_name = "TABLES",
         .descriptor = show_table_status_descriptor(
             MYLITE_FIELD_TYPE_LONGLONG,
             21U,
             MYLITE_FIELD_FLAG_NUM,
             true
         )},
        {.name = "Create_options",
         .table_name = "TABLES",
         .descriptor = show_table_status_descriptor(MYLITE_FIELD_TYPE_VAR_STRING, 256U, 0U, true)},
        {.name = "Comment",
         .table_name = "TABLES",
         .descriptor = show_table_status_comment_descriptor()},
    };

    return mylite_result_metadata_attach_columns(
        database,
        stmt,
        columns,
        sizeof(columns) / sizeof(columns[0])
    );
}

static struct mylite_field_descriptor show_table_status_comment_descriptor(void) {
    struct mylite_field_descriptor descriptor =
        show_table_status_descriptor(MYLITE_FIELD_TYPE_VAR_STRING, 2048U, 0U, true);
    descriptor.decimals = mylite_mysql_not_fixed_decimals;
    return descriptor;
}

static struct mylite_field_descriptor show_table_status_descriptor(
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
        .charset_id = (type == MYLITE_FIELD_TYPE_VAR_STRING || type == MYLITE_FIELD_TYPE_STRING ||
                       type == MYLITE_FIELD_TYPE_BLOB)
                          ? mylite_mysql_latin1_swedish_ci_charset_id
                          : mylite_mysql_binary_charset_id,
        .nullable = nullable,
    };

    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}

static int copy_show_tables_schema_name(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    char **out_schema_name
) {
    const struct mylite_sql_ast_node *schema_name = mylite_ast_child_at(statement, 0U);

    *out_schema_name = NULL;
    if (schema_name != NULL && schema_name->kind == MYLITE_SQL_AST_IDENTIFIER) {
        *out_schema_name = mylite_copy_identifier_span(schema_name);
        if (*out_schema_name == NULL) {
            return MYLITE_NOMEM;
        }
        return normalize_show_tables_schema_name(out_schema_name);
    }
    if (database->selected_schema == NULL || database->selected_schema[0] == '\0') {
        (void)mylite_diagnostics_set_error_message(database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }

    *out_schema_name = mylite_copy_nonempty_cstring(database->selected_schema);
    if (*out_schema_name == NULL) {
        return MYLITE_NOMEM;
    }
    return normalize_show_tables_schema_name(out_schema_name);
}

static int validate_show_tables_schema(mylite_db *database, const char *schema_name) {
    struct mylite_schema_presence presence;
    int status = mylite_catalog_schema_exists(database, schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(
            database,
            "Unknown database '",
            schema_name,
            "'"
        );
        return MYLITE_EXEC_ERROR;
    }
    return MYLITE_OK;
}

static char *copy_show_tables_like_pattern(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *literal = show_tables_filter(statement);

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return NULL;
    }
    return mylite_show_copy_like_pattern_span(literal);
}

static const struct mylite_sql_ast_node *show_tables_filter(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *first = mylite_ast_child_at(statement, 0U);

    if (first == NULL || first->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return first;
    }
    return mylite_ast_child_at(statement, 1U);
}

static const struct mylite_sql_ast_node *show_tables_where_expression(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *filter = show_tables_filter(statement);

    if (filter == NULL || filter->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        return NULL;
    }
    return mylite_ast_child_at(filter, 0U);
}

static char *copy_show_tables_display_pattern(const char *like_pattern, bool uppercase_pattern) {
    char *display_pattern = mylite_copy_span_text(like_pattern, strlen(like_pattern));

    if (display_pattern == NULL) {
        return NULL;
    }
    if (uppercase_pattern) {
        mylite_uppercase_ascii_text(display_pattern);
    }
    return display_pattern;
}

static char *show_tables_column_name(const char *schema_name, const char *like_pattern) {
    if (like_pattern == NULL) {
        return sqlite3_mprintf("Tables_in_%s", schema_name);
    }
    return sqlite3_mprintf("Tables_in_%s (%s)", schema_name, like_pattern);
}

static char *show_tables_glob_pattern(const char *like_pattern, bool escape_backslash) {
    sqlite3_str *glob = sqlite3_str_new(NULL);

    if (glob == NULL) {
        return NULL;
    }

    for (size_t index = 0U; like_pattern[index] != '\0'; ++index) {
        char character = like_pattern[index];

        if (escape_backslash && character == '\\' && like_pattern[index + 1U] != '\0') {
            append_show_tables_glob_literal(glob, like_pattern[++index]);
        } else if (character == '%') {
            sqlite3_str_append(glob, "*", 1);
        } else if (character == '_') {
            sqlite3_str_append(glob, "?", 1);
        } else {
            append_show_tables_glob_literal(glob, character);
        }
    }
    return sqlite3_str_finish(glob);
}

static int normalize_show_tables_schema_name(char **schema_name) {
    char *normalized = NULL;

    if (schema_name == NULL || *schema_name == NULL ||
        !mylite_ascii_case_equal(*schema_name, "information_schema") ||
        strcmp(*schema_name, "information_schema") == 0) {
        return MYLITE_OK;
    }

    normalized = mylite_copy_nonempty_cstring("information_schema");
    if (normalized == NULL) {
        return MYLITE_NOMEM;
    }

    free(*schema_name);
    *schema_name = normalized;
    return MYLITE_OK;
}

static void append_show_tables_glob_literal(sqlite3_str *glob, char character) {
    char literal[1] = {character};

    switch (character) {
    case '*':
        sqlite3_str_append(glob, "[*]", 3);
        break;
    case '?':
        sqlite3_str_append(glob, "[?]", 3);
        break;
    case '[':
        sqlite3_str_append(glob, "[[]", 3);
        break;
    case ']':
        sqlite3_str_append(glob, "[]]", 3);
        break;
    default:
        sqlite3_str_append(glob, literal, 1);
        break;
    }
}
