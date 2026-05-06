#include "mylite_table_ddl_create_catalog_index.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_table_ddl.h"
#include "mylite_table_ddl_plan_lookup.h"
#include "sqlite3.h"

#include <stddef.h>

static int insert_index_catalog_part(mylite_db *database, sqlite3_stmt *insert,
                                     const char *schema_name,
                                     const struct mylite_create_table_plan *plan,
                                     const struct mylite_create_table_index *index,
                                     const struct mylite_create_table_key_part *part,
                                     size_t part_index);
static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_table_ddl_insert_create_table_index_catalog_rows(
    mylite_db *database, const char *schema_name, const struct mylite_create_table_plan *plan)
{
    sqlite3_stmt *insert = NULL;
    char *sql = sqlite3_mprintf(
        "INSERT INTO %s("
        "table_catalog, table_schema, table_name, non_unique, index_schema, index_name, "
        "seq_in_index, column_name, collation, cardinality, sub_part, packed, nullable, "
        "index_type, comment, index_comment, is_visible, expression)"
        " VALUES('def', ?, ?, ?, ?, ?, ?, ?, ?, NULL, ?, NULL, ?, ?, '', ?, ?, NULL)",
        mylite_catalog_index_catalog_name(plan->temporary));
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    for (size_t index_index = 0U; index_index < plan->index_count; ++index_index) {
        const struct mylite_create_table_index *index = &plan->indexes[index_index];

        for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
            int status = insert_index_catalog_part(database, insert, schema_name, plan, index,
                                                   &index->parts[part_index], part_index);
            if (status != MYLITE_OK) {
                sqlite3_finalize(insert);
                return status;
            }
        }
    }

    sqlite3_finalize(insert);
    return MYLITE_OK;
}

static int insert_index_catalog_part(mylite_db *database, sqlite3_stmt *insert,
                                     const char *schema_name,
                                     const struct mylite_create_table_plan *plan,
                                     const struct mylite_create_table_index *index,
                                     const struct mylite_create_table_key_part *part,
                                     size_t part_index)
{
    enum {
        bind_table_schema = 1,
        bind_table_name = 2,
        bind_non_unique = 3,
        bind_index_schema = 4,
        bind_index_name = 5,
        bind_seq_in_index = 6,
        bind_column_name = 7,
        bind_collation = 8,
        bind_sub_part = 9,
        bind_nullable = 10,
        bind_index_type = 11,
        bind_index_comment = 12,
        bind_is_visible = 13,
    };
    const struct mylite_create_table_column *column =
        mylite_table_ddl_find_create_table_column(plan, part->column_name);
    int non_unique = 1;
    const char *nullable = "";
    const char *index_type = "BTREE";
    const char *is_visible = "NO";
    int rc = SQLITE_OK;

    if (index->is_unique) {
        non_unique = 0;
    }
    if (column != NULL && column->nullable) {
        nullable = "YES";
    }
    if (index->algorithm == MYLITE_SQL_AST_INDEX_ALGORITHM_HASH) {
        index_type = "HASH";
    }
    if (index->is_visible) {
        is_visible = "YES";
    }

    sqlite3_reset(insert);
    sqlite3_clear_bindings(insert);
    sqlite3_bind_text(insert, bind_table_schema, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_table_name, plan->table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_int(insert, bind_non_unique, non_unique);
    sqlite3_bind_text(insert, bind_index_schema, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_index_name, index->name, -1, sqlite_transient_destructor());
    sqlite3_bind_int64(insert, bind_seq_in_index, (sqlite3_int64)part_index + 1);
    sqlite3_bind_text(insert, bind_column_name, part->column_name, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_collation,
                      mylite_table_ddl_index_collation_for_order(part->order), -1, SQLITE_STATIC);
    if (part->has_prefix_length) {
        sqlite3_bind_int64(insert, bind_sub_part, (sqlite3_int64)part->prefix_length);
    } else {
        sqlite3_bind_null(insert, bind_sub_part);
    }
    sqlite3_bind_text(insert, bind_nullable, nullable, -1, SQLITE_STATIC);
    sqlite3_bind_text(insert, bind_index_type, index_type, -1, SQLITE_STATIC);
    sqlite3_bind_text(insert, bind_index_comment, index->comment == NULL ? "" : index->comment, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_is_visible, is_visible, -1, SQLITE_STATIC);

    rc = sqlite3_step(insert);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static sqlite3_destructor_type sqlite_transient_destructor(void)
{
    // SQLite's public macro intentionally uses this sentinel pointer value.
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
