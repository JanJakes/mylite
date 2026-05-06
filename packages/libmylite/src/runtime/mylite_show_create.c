#include "mylite_show.h"

#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_show_create_common.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct mylite_show_create_schema_info {
    char *name;
    char *character_set;
    char *collation;
    char *encryption;
};

static int show_create_schema_sql(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    char **out_sql
);

static int read_show_create_schema_info(
    mylite_db *database,
    const char *schema_name,
    struct mylite_show_create_schema_info *out_info
);

static int append_show_create_schema_text(
    sqlite3_str *create_sql,
    const struct mylite_show_create_schema_info *info,
    bool if_not_exists
);

static bool show_create_schema_should_append_collation(
    const char *character_set,
    const char *collation
);

static void show_create_schema_info_deinit(struct mylite_show_create_schema_info *info);

int mylite_show_prepare_create_schema_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    char *sqlite_sql = NULL;
    int status = show_create_schema_sql(database, statement, &sqlite_sql);

    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    sqlite3_free(sqlite_sql);
    return status;
}

static int show_create_schema_sql(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    char **out_sql
) {
    struct mylite_show_create_schema_info info = {0};
    sqlite3_str *create_sql = NULL;
    char *schema_name = mylite_copy_identifier_span(mylite_ast_child_at(statement, 0U));
    char *create_text = NULL;
    int status = MYLITE_OK;

    *out_sql = NULL;
    if (schema_name == NULL) {
        return MYLITE_NOMEM;
    }

    status = read_show_create_schema_info(database, schema_name, &info);
    if (status != MYLITE_OK) {
        free(schema_name);
        return status;
    }

    create_sql = sqlite3_str_new(database->sqlite);
    if (create_sql == NULL) {
        show_create_schema_info_deinit(&info);
        free(schema_name);
        return MYLITE_NOMEM;
    }

    status = append_show_create_schema_text(
        create_sql,
        &info,
        statement->show_create_schema_if_not_exists
    );
    create_text = sqlite3_str_finish(create_sql);

    if (status == MYLITE_OK && create_text != NULL) {
        *out_sql = sqlite3_mprintf(
            "SELECT %Q AS \"Database\", %Q AS \"Create Database\"",
            info.name,
            create_text
        );
        if (*out_sql == NULL) {
            status = MYLITE_NOMEM;
        }
    } else if (status == MYLITE_OK) {
        status = MYLITE_NOMEM;
    }

    sqlite3_free(create_text);
    show_create_schema_info_deinit(&info);
    free(schema_name);
    return status;
}

static int read_show_create_schema_info(
    mylite_db *database,
    const char *schema_name,
    struct mylite_show_create_schema_info *out_info
) {
    sqlite3_stmt *select = NULL;
    static const char sql[] = "SELECT name, default_character_set, default_collation, "
                              "default_encryption FROM __mylite_schema_catalog WHERE name = ?";
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);

    *out_info = (struct mylite_show_create_schema_info){0};
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(select, 1, schema_name, -1, mylite_show_sqlite_transient_destructor());

    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(select, 0);
        const char *character_set = (const char *)sqlite3_column_text(select, 1);
        const char *collation = (const char *)sqlite3_column_text(select, 2);
        const char *encryption = (const char *)sqlite3_column_text(select, 3);

        out_info->name =
            mylite_copy_span_text(name == NULL ? "" : name, name == NULL ? 0U : strlen(name));
        out_info->character_set = mylite_copy_span_text(
            character_set == NULL ? "" : character_set,
            character_set == NULL ? 0U : strlen(character_set)
        );
        out_info->collation = mylite_copy_span_text(
            collation == NULL ? "" : collation,
            collation == NULL ? 0U : strlen(collation)
        );
        out_info->encryption = mylite_copy_span_text(
            encryption == NULL ? "N" : encryption,
            encryption == NULL ? 1U : strlen(encryption)
        );
        sqlite3_finalize(select);
        if (out_info->name == NULL || out_info->character_set == NULL ||
            out_info->collation == NULL || out_info->encryption == NULL) {
            show_create_schema_info_deinit(out_info);
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }

    sqlite3_finalize(select);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    (void)mylite_diagnostics_set_error_message_parts(
        database,
        "Unknown database '",
        schema_name,
        "'"
    );
    return MYLITE_EXEC_ERROR;
}

static int append_show_create_schema_text(
    sqlite3_str *create_sql,
    const struct mylite_show_create_schema_info *info,
    bool if_not_exists
) {
    sqlite3_str_appendall(create_sql, "CREATE DATABASE ");
    if (if_not_exists) {
        sqlite3_str_appendall(create_sql, "/*!32312 IF NOT EXISTS*/ ");
    }
    mylite_show_create_append_identifier(create_sql, info->name);
    sqlite3_str_appendf(create_sql, " /*!40100 DEFAULT CHARACTER SET %s", info->character_set);
    if (show_create_schema_should_append_collation(info->character_set, info->collation)) {
        sqlite3_str_appendf(create_sql, " COLLATE %s", info->collation);
    }
    sqlite3_str_appendf(create_sql, " */ /*!80016 DEFAULT ENCRYPTION='%s' */", info->encryption);
    return sqlite3_str_errcode(create_sql) == SQLITE_OK ? MYLITE_OK : MYLITE_NOMEM;
}

static bool show_create_schema_should_append_collation(
    const char *character_set,
    const char *collation
) {
    const struct mylite_charset *charset = mylite_charset_lookup(character_set);

    if (collation == NULL || collation[0] == '\0') {
        return false;
    }
    if (charset == NULL) {
        return true;
    }
    if (!mylite_ascii_case_equal(collation, charset->default_collation)) {
        return true;
    }
    return mylite_ascii_case_equal(character_set, "utf8mb4");
}

static void show_create_schema_info_deinit(struct mylite_show_create_schema_info *info) {
    if (info == NULL) {
        return;
    }

    free(info->name);
    free(info->character_set);
    free(info->collation);
    free(info->encryption);
    *info = (struct mylite_show_create_schema_info){0};
}
