#include "mylite_show_create_table_info.h"

#include "mylite_catalog.h"
#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_show_create_common.h"
#include "mylite_show_create_table_target.h"
#include "mylite_span.h"
#include "mylite_uint64_text.h"
#include "sqlite3.h"

#include <stdlib.h>
#include <string.h>

int mylite_show_create_table_read_info(
    mylite_db *database,
    const struct mylite_show_create_table_target *target,
    struct mylite_show_create_table_info *out_info
) {
    sqlite3_stmt *select = NULL;
    char *sql = sqlite3_mprintf(
        "SELECT engine, auto_increment, table_collation, table_comment "
        "FROM %s WHERE table_schema = ? AND table_name = ?",
        mylite_catalog_table_catalog_name(target->temporary)
    );
    int rc = SQLITE_OK;

    *out_info = (struct mylite_show_create_table_info){0};
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

    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW) {
        const unsigned char *engine = sqlite3_column_text(select, 0);
        const unsigned char *collation = sqlite3_column_text(select, 2);
        const unsigned char *comment = sqlite3_column_text(select, 3);

        out_info->engine =
            mylite_copy_nonempty_cstring(engine == NULL ? "InnoDB" : (const char *)engine);
        out_info->table_collation = mylite_copy_nonempty_cstring(
            collation == NULL ? mylite_charset_default_collation_name() : (const char *)collation
        );
        out_info->table_comment = mylite_copy_span_text(
            comment == NULL ? "" : (const char *)comment,
            comment == NULL ? 0U : strlen((const char *)comment)
        );
        out_info->has_auto_increment = sqlite3_column_type(select, 1) != SQLITE_NULL;
        (void)mylite_sqlite_column_uint64(select, 1, &out_info->auto_increment);
        sqlite3_finalize(select);
        if (out_info->engine == NULL || out_info->table_collation == NULL ||
            out_info->table_comment == NULL) {
            mylite_show_create_table_info_deinit(out_info);
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }

    sqlite3_finalize(select);
    if (rc == SQLITE_DONE) {
        return mylite_diagnostics_set_table_doesnt_exist_error(
            database,
            target->schema_name,
            target->table_name
        );
    }
    return mylite_diagnostics_set_sqlite_error(database);
}

void mylite_show_create_table_info_deinit(struct mylite_show_create_table_info *info) {
    if (info == NULL) {
        return;
    }

    free(info->engine);
    free(info->table_collation);
    free(info->table_comment);
    *info = (struct mylite_show_create_table_info){0};
}
