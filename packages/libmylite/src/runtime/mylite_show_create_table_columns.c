#include "mylite_show_create_table_columns.h"

#include "mylite_catalog.h"
#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_show_create_common.h"
#include "mylite_show_create_table_body.h"
#include "mylite_show_create_table_info.h"
#include "mylite_show_create_table_target.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <stdbool.h>

struct mylite_show_create_column_collation {
    const char *character_set_name;
    const char *column_collation;
    const char *table_collation;
};

static int append_show_create_table_column(
    sqlite3_str *create_sql,
    sqlite3_stmt *select,
    const struct mylite_show_create_table_info *info
);

static bool show_create_column_needs_implicit_default_null(const char *data_type);

static void append_show_create_column_collation(
    sqlite3_str *create_sql,
    struct mylite_show_create_column_collation collation
);

int mylite_show_create_table_append_columns(
    mylite_db *database,
    sqlite3_str *create_sql,
    const struct mylite_show_create_table_target *target,
    const struct mylite_show_create_table_info *info,
    bool *first_line
) {
    sqlite3_stmt *select = NULL;
    char *sql = sqlite3_mprintf(
        "SELECT column_name, column_type, is_nullable, column_default, extra, column_comment, "
        "character_set_name, collation_name, data_type, has_default "
        "FROM %s WHERE table_schema = ? AND table_name = ? "
        "ORDER BY ordinal_position",
        mylite_catalog_column_catalog_name(target->temporary)
    );
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(
        select,
        1,
        target->schema_name,
        -1,
        mylite_show_sqlite_transient_destructor()
    );
    sqlite3_bind_text(select, 2, target->table_name, -1, mylite_show_sqlite_transient_destructor());

    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        mylite_show_create_table_append_line_prefix(create_sql, first_line);
        if (append_show_create_table_column(create_sql, select, info) != MYLITE_OK) {
            sqlite3_finalize(select);
            return MYLITE_NOMEM;
        }
    }

    sqlite3_finalize(select);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int append_show_create_table_column(
    sqlite3_str *create_sql,
    sqlite3_stmt *select,
    const struct mylite_show_create_table_info *info
) {
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
        has_default_column = 9,
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
    bool auto_increment = mylite_text_contains_word(extra, "auto_increment");
    bool nullable = mylite_ascii_case_equal(is_nullable, "YES");
    bool has_default = sqlite3_column_int(select, has_default_column) != 0;

    mylite_show_create_append_identifier(create_sql, column_name);
    sqlite3_str_appendf(create_sql, " %s", column_type == NULL ? "" : column_type);
    append_show_create_column_collation(
        create_sql,
        (struct mylite_show_create_column_collation){
            .character_set_name = character_set,
            .column_collation = column_collation,
            .table_collation = info->table_collation,
        }
    );
    if (!nullable) {
        sqlite3_str_appendall(create_sql, " NOT NULL");
    } else if (
        column_default != NULL && mylite_column_default_is_current_timestamp(column_default)
    ) {
        sqlite3_str_appendall(create_sql, " NULL");
    }
    if (column_default != NULL && !auto_increment) {
        sqlite3_str_appendall(create_sql, " DEFAULT ");
        if (mylite_column_default_is_current_timestamp(column_default)) {
            sqlite3_str_appendall(create_sql, "CURRENT_TIMESTAMP");
        } else {
            mylite_show_create_append_string_literal(create_sql, column_default);
        }
    } else if (
        nullable && has_default && show_create_column_needs_implicit_default_null(data_type)
    ) {
        sqlite3_str_appendall(create_sql, " DEFAULT NULL");
    }
    if (auto_increment) {
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

static bool show_create_column_needs_implicit_default_null(const char *data_type) {
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

static void append_show_create_column_collation(
    sqlite3_str *create_sql,
    struct mylite_show_create_column_collation collation
) {
    if (collation.column_collation == NULL || collation.column_collation[0] == '\0') {
        return;
    }
    if (!mylite_ascii_case_equal(collation.column_collation, collation.table_collation)) {
        sqlite3_str_appendf(
            create_sql,
            " CHARACTER SET %s COLLATE %s",
            collation.character_set_name == NULL ? mylite_charset_default_name()
                                                 : collation.character_set_name,
            collation.column_collation
        );
        return;
    }
    if (!mylite_ascii_case_equal(
            collation.table_collation,
            mylite_charset_default_collation_name()
        )) {
        sqlite3_str_appendf(create_sql, " COLLATE %s", collation.column_collation);
    }
}
