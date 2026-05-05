#include "mylite_show.h"

#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_show_create_common.h"
#include "mylite_show_types.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct mylite_show_create_table_target {
    char *schema_name;
    char *table_name;
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

static int copy_show_create_table_target(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         struct mylite_show_create_table_target *out_target);
static int validate_show_create_table_target(mylite_db *database,
                                             const struct mylite_show_create_table_target *target);
static int show_create_table_sql(mylite_db *database,
                                 const struct mylite_show_create_table_target *target,
                                 char **out_sql);
static int read_show_create_table_info(mylite_db *database,
                                       const struct mylite_show_create_table_target *target,
                                       struct mylite_show_create_table_info *out_info);
static int append_show_create_table_columns(mylite_db *database, sqlite3_str *create_sql,
                                            const struct mylite_show_create_table_target *target,
                                            const struct mylite_show_create_table_info *info,
                                            bool *first_line);
static int append_show_create_table_column(sqlite3_str *create_sql, sqlite3_stmt *select,
                                           const struct mylite_show_create_table_info *info);
static int append_show_create_table_indexes(mylite_db *database, sqlite3_str *create_sql,
                                            const struct mylite_show_create_table_target *target,
                                            bool *first_line);
static int append_show_create_table_index(sqlite3_str *create_sql, sqlite3_stmt *select);
static int append_show_create_table_key_parts(mylite_db *database, sqlite3_str *create_sql,
                                              sqlite3_stmt *select, const char *index_name);
static void append_show_create_table_options(sqlite3_str *create_sql,
                                             const struct mylite_show_create_table_info *info);
static void append_show_create_table_line_prefix(sqlite3_str *create_sql, bool *first_line);
static bool show_create_column_needs_implicit_default_null(const char *data_type);
static void
append_show_create_column_collation(sqlite3_str *create_sql,
                                    struct mylite_show_create_column_collation collation);
static void show_create_table_info_deinit(struct mylite_show_create_table_info *info);
static void show_create_table_target_deinit(struct mylite_show_create_table_target *target);

int mylite_show_prepare_create_table_statement(mylite_db *database,
                                               const struct mylite_sql_ast_node *statement,
                                               mylite_stmt **out_stmt)
{
    struct mylite_show_create_table_target target = {0};
    char *sqlite_sql = NULL;
    int status = copy_show_create_table_target(database, statement, &target);

    if (status == MYLITE_OK) {
        status = validate_show_create_table_target(database, &target);
    }
    if (status == MYLITE_OK) {
        status = show_create_table_sql(database, &target, &sqlite_sql);
    }
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    show_create_table_target_deinit(&target);
    sqlite3_free(sqlite_sql);
    return status;
}

static int copy_show_create_table_target(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         struct mylite_show_create_table_target *out_target)
{
    struct mylite_show_columns_target target = {0};
    int status = MYLITE_OK;

    *out_target = (struct mylite_show_create_table_target){0};
    status = mylite_show_copy_columns_table_target(
        &(const struct mylite_show_columns_source_nodes){
            .table_name = mylite_ast_child_at(statement, 0U),
            .explicit_schema = NULL,
        },
        &target);
    if (status != MYLITE_OK) {
        if (status == MYLITE_UNSUPPORTED) {
            (void)mylite_diagnostics_set_error_message(
                database, "SHOW CREATE TABLE names with more than two parts are not supported");
        }
        return status;
    }
    if (target.schema_name == NULL) {
        status = mylite_show_copy_columns_selected_schema(database, &target);
    }
    if (status == MYLITE_OK) {
        out_target->schema_name = target.schema_name;
        out_target->table_name = target.table_name;
        target = (struct mylite_show_columns_target){0};
    }

    mylite_show_columns_target_deinit(&target);
    return status;
}

static int validate_show_create_table_target(mylite_db *database,
                                             const struct mylite_show_create_table_target *target)
{
    struct mylite_show_columns_target columns_target = {
        .schema_name = target->schema_name,
        .table_name = target->table_name,
    };

    return mylite_show_validate_columns_target(
        database, &columns_target,
        "SHOW CREATE TABLE for information_schema tables is not supported");
}

static int show_create_table_sql(mylite_db *database,
                                 const struct mylite_show_create_table_target *target,
                                 char **out_sql)
{
    struct mylite_show_create_table_info info = {0};
    sqlite3_str *create_sql = NULL;
    char *create_text = NULL;
    bool first_line = true;
    int status = read_show_create_table_info(database, target, &info);

    *out_sql = NULL;
    if (status != MYLITE_OK) {
        return status;
    }

    create_sql = sqlite3_str_new(database->sqlite);
    if (create_sql == NULL) {
        show_create_table_info_deinit(&info);
        return MYLITE_NOMEM;
    }

    sqlite3_str_appendall(create_sql, "CREATE TABLE ");
    mylite_show_create_append_identifier(create_sql, target->table_name);
    sqlite3_str_appendall(create_sql, " (\n");
    status = append_show_create_table_columns(database, create_sql, target, &info, &first_line);
    if (status == MYLITE_OK) {
        status = append_show_create_table_indexes(database, create_sql, target, &first_line);
    }
    if (status == MYLITE_OK) {
        sqlite3_str_appendall(create_sql, "\n)");
        append_show_create_table_options(create_sql, &info);
    }
    create_text = sqlite3_str_finish(create_sql);

    if (status == MYLITE_OK && create_text != NULL) {
        *out_sql = sqlite3_mprintf("SELECT %Q AS \"Table\", %Q AS \"Create Table\"",
                                   target->table_name, create_text);
        if (*out_sql == NULL) {
            status = MYLITE_NOMEM;
        }
    } else if (status == MYLITE_OK) {
        status = MYLITE_NOMEM;
    }

    sqlite3_free(create_text);
    show_create_table_info_deinit(&info);
    return status;
}

static int read_show_create_table_info(mylite_db *database,
                                       const struct mylite_show_create_table_target *target,
                                       struct mylite_show_create_table_info *out_info)
{
    sqlite3_stmt *select = NULL;
    static const char sql[] =
        "SELECT engine, auto_increment, table_collation, table_comment "
        "FROM __mylite_table_catalog WHERE table_schema = ? AND table_name = ?";
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);

    *out_info = (struct mylite_show_create_table_info){0};
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(select, 1, target->schema_name, -1,
                      mylite_show_sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, target->table_name, -1, mylite_show_sqlite_transient_destructor());

    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW) {
        const unsigned char *engine = sqlite3_column_text(select, 0);
        const unsigned char *collation = sqlite3_column_text(select, 2);
        const unsigned char *comment = sqlite3_column_text(select, 3);

        out_info->engine =
            mylite_copy_nonempty_cstring(engine == NULL ? "InnoDB" : (const char *)engine);
        out_info->table_collation = mylite_copy_nonempty_cstring(
            collation == NULL ? mylite_charset_default_collation_name() : (const char *)collation);
        out_info->table_comment =
            mylite_copy_span_text(comment == NULL ? "" : (const char *)comment,
                                  comment == NULL ? 0U : strlen((const char *)comment));
        out_info->has_auto_increment = sqlite3_column_type(select, 1) != SQLITE_NULL;
        out_info->auto_increment = sqlite3_column_int64(select, 1);
        sqlite3_finalize(select);
        if (out_info->engine == NULL || out_info->table_collation == NULL ||
            out_info->table_comment == NULL) {
            show_create_table_info_deinit(out_info);
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }

    sqlite3_finalize(select);
    if (rc == SQLITE_DONE) {
        return mylite_diagnostics_set_table_doesnt_exist_error(database, target->schema_name,
                                                               target->table_name);
    }
    return mylite_diagnostics_set_sqlite_error(database);
}

static int append_show_create_table_columns(mylite_db *database, sqlite3_str *create_sql,
                                            const struct mylite_show_create_table_target *target,
                                            const struct mylite_show_create_table_info *info,
                                            bool *first_line)
{
    sqlite3_stmt *select = NULL;
    static const char sql[] =
        "SELECT column_name, column_type, is_nullable, column_default, extra, column_comment, "
        "character_set_name, collation_name, data_type "
        "FROM __mylite_column_catalog WHERE table_schema = ? AND table_name = ? "
        "ORDER BY ordinal_position";
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(select, 1, target->schema_name, -1,
                      mylite_show_sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, target->table_name, -1, mylite_show_sqlite_transient_destructor());

    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        append_show_create_table_line_prefix(create_sql, first_line);
        if (append_show_create_table_column(create_sql, select, info) != MYLITE_OK) {
            sqlite3_finalize(select);
            return MYLITE_NOMEM;
        }
    }

    sqlite3_finalize(select);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int append_show_create_table_column(sqlite3_str *create_sql, sqlite3_stmt *select,
                                           const struct mylite_show_create_table_info *info)
{
    enum {
        column_name_column = 0,
        column_type_column = 1,
        is_nullable_column = 2,
        column_default_column = 3,
        extra_column = 4,
        comment_column = 5,
        character_set_column = 6,
        collation_column = 7,
        data_type_column = 8,
    };
    const char *column_name = (const char *)sqlite3_column_text(select, column_name_column);
    const char *column_type = (const char *)sqlite3_column_text(select, column_type_column);
    const char *is_nullable = (const char *)sqlite3_column_text(select, is_nullable_column);
    const char *column_default = (const char *)sqlite3_column_text(select, column_default_column);
    const char *extra = (const char *)sqlite3_column_text(select, extra_column);
    const char *comment = (const char *)sqlite3_column_text(select, comment_column);
    const char *character_set = (const char *)sqlite3_column_text(select, character_set_column);
    const char *column_collation = (const char *)sqlite3_column_text(select, collation_column);
    const char *data_type = (const char *)sqlite3_column_text(select, data_type_column);
    bool nullable = mylite_ascii_case_equal(is_nullable, "YES");

    mylite_show_create_append_identifier(create_sql, column_name);
    sqlite3_str_appendf(create_sql, " %s", column_type == NULL ? "" : column_type);
    append_show_create_column_collation(create_sql, (struct mylite_show_create_column_collation){
                                                        .character_set_name = character_set,
                                                        .column_collation = column_collation,
                                                        .table_collation = info->table_collation,
                                                    });
    if (!nullable) {
        sqlite3_str_appendall(create_sql, " NOT NULL");
    } else if (column_default != NULL &&
               mylite_column_default_is_current_timestamp(column_default)) {
        sqlite3_str_appendall(create_sql, " NULL");
    }
    if (column_default != NULL) {
        sqlite3_str_appendall(create_sql, " DEFAULT ");
        if (mylite_column_default_is_current_timestamp(column_default)) {
            sqlite3_str_appendall(create_sql, "CURRENT_TIMESTAMP");
        } else {
            mylite_show_create_append_string_literal(create_sql, column_default);
        }
    } else if (nullable && show_create_column_needs_implicit_default_null(data_type)) {
        sqlite3_str_appendall(create_sql, " DEFAULT NULL");
    }
    if (mylite_text_contains_word(extra, "auto_increment")) {
        sqlite3_str_appendall(create_sql, " AUTO_INCREMENT");
    }
    if (mylite_text_contains_word(extra, "on") && mylite_text_contains_word(extra, "update") &&
        mylite_text_contains_word(extra, "CURRENT_TIMESTAMP")) {
        sqlite3_str_appendall(create_sql, " ON UPDATE CURRENT_TIMESTAMP");
    }
    if (mylite_text_contains_word(extra, "INVISIBLE")) {
        sqlite3_str_appendall(create_sql, " /*!80023 INVISIBLE */");
    }
    if (comment != NULL && comment[0] != '\0') {
        sqlite3_str_appendall(create_sql, " COMMENT ");
        mylite_show_create_append_string_literal(create_sql, comment);
    }
    return sqlite3_str_errcode(create_sql) == SQLITE_OK ? MYLITE_OK : MYLITE_NOMEM;
}

static int append_show_create_table_indexes(mylite_db *database, sqlite3_str *create_sql,
                                            const struct mylite_show_create_table_target *target,
                                            bool *first_line)
{
    sqlite3_stmt *select = NULL;
    static const char sql[] =
        "WITH first_parts AS ("
        " SELECT index_name, MIN(rowid) AS first_rowid FROM __mylite_index_catalog "
        " WHERE table_schema = ? AND table_name = ? GROUP BY index_name)"
        "SELECT i.index_name, i.non_unique, i.index_comment, i.is_visible, "
        "i.table_schema, i.table_name "
        "FROM __mylite_index_catalog i "
        "JOIN first_parts f ON f.index_name = i.index_name "
        "WHERE i.table_schema = ? AND i.table_name = ? AND i.seq_in_index = 1 "
        "ORDER BY CASE WHEN i.index_name = 'PRIMARY' THEN 0 WHEN i.non_unique = 0 THEN 1 "
        "ELSE 2 END, f.first_rowid";
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(select, 1, target->schema_name, -1,
                      mylite_show_sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, target->table_name, -1, mylite_show_sqlite_transient_destructor());
    sqlite3_bind_text(select, 3, target->schema_name, -1,
                      mylite_show_sqlite_transient_destructor());
    sqlite3_bind_text(select, 4, target->table_name, -1, mylite_show_sqlite_transient_destructor());

    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        int status = MYLITE_OK;

        append_show_create_table_line_prefix(create_sql, first_line);
        status = append_show_create_table_index(create_sql, select);
        if (status == MYLITE_OK) {
            status = append_show_create_table_key_parts(
                database, create_sql, select, (const char *)sqlite3_column_text(select, 0));
        }
        if (status != MYLITE_OK) {
            sqlite3_finalize(select);
            return status;
        }
    }

    sqlite3_finalize(select);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int append_show_create_table_index(sqlite3_str *create_sql, sqlite3_stmt *select)
{
    enum {
        index_name_column = 0,
        non_unique_column = 1,
    };
    const char *index_name = (const char *)sqlite3_column_text(select, index_name_column);
    int non_unique = sqlite3_column_int(select, non_unique_column);

    if (mylite_ascii_case_equal(index_name, "PRIMARY")) {
        sqlite3_str_appendall(create_sql, "PRIMARY KEY (");
    } else {
        if (non_unique == 0) {
            sqlite3_str_appendall(create_sql, "UNIQUE ");
        }
        sqlite3_str_appendall(create_sql, "KEY ");
        mylite_show_create_append_identifier(create_sql, index_name);
        sqlite3_str_appendall(create_sql, " (");
    }
    return sqlite3_str_errcode(create_sql) == SQLITE_OK ? MYLITE_OK : MYLITE_NOMEM;
}

static int append_show_create_table_key_parts(mylite_db *database, sqlite3_str *create_sql,
                                              sqlite3_stmt *select, const char *index_name)
{
    enum {
        index_comment_column = 2,
        is_visible_column = 3,
        table_schema_column = 4,
        table_name_column = 5,
    };
    sqlite3_stmt *parts = NULL;
    sqlite3 *sqlite = sqlite3_db_handle(select);
    static const char sql[] =
        "SELECT column_name, collation, sub_part FROM __mylite_index_catalog "
        "WHERE table_schema = ? AND table_name = ? AND index_name = ? ORDER BY seq_in_index";
    const char *schema_name = (const char *)sqlite3_column_text(select, table_schema_column);
    const char *table_name = (const char *)sqlite3_column_text(select, table_name_column);
    const char *index_comment = (const char *)sqlite3_column_text(select, index_comment_column);
    const char *is_visible = (const char *)sqlite3_column_text(select, is_visible_column);
    bool first_part = true;
    int rc = sqlite3_prepare_v3(sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &parts, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(parts, 1, schema_name, -1, mylite_show_sqlite_transient_destructor());
    sqlite3_bind_text(parts, 2, table_name, -1, mylite_show_sqlite_transient_destructor());
    sqlite3_bind_text(parts, 3, index_name, -1, mylite_show_sqlite_transient_destructor());

    while ((rc = sqlite3_step(parts)) == SQLITE_ROW) {
        enum {
            column_name_column = 0,
            collation_column = 1,
            sub_part_column = 2,
        };
        const char *column_name = (const char *)sqlite3_column_text(parts, column_name_column);
        const char *collation = (const char *)sqlite3_column_text(parts, collation_column);

        if (!first_part) {
            sqlite3_str_appendall(create_sql, ",");
        }
        first_part = false;
        mylite_show_create_append_identifier(create_sql, column_name);
        if (sqlite3_column_type(parts, sub_part_column) != SQLITE_NULL) {
            sqlite3_str_appendf(create_sql, "(%lld)", sqlite3_column_int64(parts, sub_part_column));
        }
        if (mylite_ascii_case_equal(collation, "D")) {
            sqlite3_str_appendall(create_sql, " DESC");
        }
    }
    sqlite3_finalize(parts);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_str_appendall(create_sql, ")");
    if (index_comment != NULL && index_comment[0] != '\0') {
        sqlite3_str_appendall(create_sql, " COMMENT ");
        mylite_show_create_append_string_literal(create_sql, index_comment);
    }
    if (!mylite_ascii_case_equal(index_name, "PRIMARY") &&
        mylite_ascii_case_equal(is_visible, "NO")) {
        sqlite3_str_appendall(create_sql, " /*!80000 INVISIBLE */");
    }
    return sqlite3_str_errcode(create_sql) == SQLITE_OK ? MYLITE_OK : MYLITE_NOMEM;
}

static void append_show_create_table_options(sqlite3_str *create_sql,
                                             const struct mylite_show_create_table_info *info)
{
    const struct mylite_collation *collation = mylite_collation_lookup(info->table_collation);
    const char *character_set =
        collation == NULL ? mylite_charset_default_name() : collation->character_set;

    sqlite3_str_appendf(create_sql, " ENGINE=%s", info->engine == NULL ? "InnoDB" : info->engine);
    if (info->has_auto_increment) {
        sqlite3_str_appendf(create_sql, " AUTO_INCREMENT=%lld", info->auto_increment);
    }
    sqlite3_str_appendf(create_sql, " DEFAULT CHARSET=%s COLLATE=%s", character_set,
                        info->table_collation == NULL ? mylite_charset_default_collation_name()
                                                      : info->table_collation);
    if (info->table_comment != NULL && info->table_comment[0] != '\0') {
        sqlite3_str_appendall(create_sql, " COMMENT=");
        mylite_show_create_append_string_literal(create_sql, info->table_comment);
    }
}

static void append_show_create_table_line_prefix(sqlite3_str *create_sql, bool *first_line)
{
    if (!*first_line) {
        sqlite3_str_appendall(create_sql, ",\n");
    }
    *first_line = false;
    sqlite3_str_appendall(create_sql, "  ");
}

static bool show_create_column_needs_implicit_default_null(const char *data_type)
{
    if (mylite_ascii_case_equal(data_type, "tinytext")) {
        return false;
    }
    if (mylite_ascii_case_equal(data_type, "text")) {
        return false;
    }
    if (mylite_ascii_case_equal(data_type, "mediumtext")) {
        return false;
    }
    if (mylite_ascii_case_equal(data_type, "longtext")) {
        return false;
    }
    if (mylite_ascii_case_equal(data_type, "tinyblob")) {
        return false;
    }
    if (mylite_ascii_case_equal(data_type, "blob")) {
        return false;
    }
    if (mylite_ascii_case_equal(data_type, "mediumblob")) {
        return false;
    }
    if (mylite_ascii_case_equal(data_type, "longblob")) {
        return false;
    }
    return true;
}

static void
append_show_create_column_collation(sqlite3_str *create_sql,
                                    struct mylite_show_create_column_collation collation)
{
    if (collation.column_collation == NULL || collation.column_collation[0] == '\0') {
        return;
    }
    if (!mylite_ascii_case_equal(collation.column_collation, collation.table_collation)) {
        sqlite3_str_appendf(create_sql, " CHARACTER SET %s COLLATE %s",
                            collation.character_set_name == NULL ? mylite_charset_default_name()
                                                                 : collation.character_set_name,
                            collation.column_collation);
        return;
    }
    if (!mylite_ascii_case_equal(collation.table_collation,
                                 mylite_charset_default_collation_name())) {
        sqlite3_str_appendf(create_sql, " COLLATE %s", collation.column_collation);
    }
}

static void show_create_table_info_deinit(struct mylite_show_create_table_info *info)
{
    if (info == NULL) {
        return;
    }

    free(info->engine);
    free(info->table_collation);
    free(info->table_comment);
    *info = (struct mylite_show_create_table_info){0};
}

static void show_create_table_target_deinit(struct mylite_show_create_table_target *target)
{
    if (target == NULL) {
        return;
    }

    free(target->schema_name);
    free(target->table_name);
    *target = (struct mylite_show_create_table_target){0};
}
