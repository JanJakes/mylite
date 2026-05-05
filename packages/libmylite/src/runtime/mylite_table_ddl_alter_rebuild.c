#include "mylite_table_ddl_alter_rebuild.h"

#include "mylite_diagnostics.h"
#include "mylite_dml.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_table_ddl_alter.h"
#include "mylite_table_ddl_alter_catalog.h"
#include "mylite_table_ddl_alter_warnings.h"
#include "mylite_transactions.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int create_alter_table_shadow_table(mylite_stmt *stmt,
                                           const struct mylite_alter_table_model *model,
                                           const char *shadow_name);
static char *build_alter_table_create_shadow_sql(mylite_db *database,
                                                 const struct mylite_alter_table_model *model,
                                                 const char *shadow_name);
static int copy_alter_table_rows(mylite_stmt *stmt, const struct mylite_alter_table_model *model,
                                 const char *shadow_name, int64_t *out_copied_rows);
static char *build_alter_table_copy_sql(mylite_db *database,
                                        const struct mylite_alter_table_model *model,
                                        const char *shadow_name);
static int bind_alter_table_added_column_values(mylite_stmt *stmt, sqlite3_stmt *insert,
                                                const struct mylite_alter_table_model *model);
static int swap_alter_table_physical_table(mylite_stmt *stmt, const char *shadow_name,
                                           const char *physical_name);
static char *alter_table_shadow_physical_name(mylite_db *database, const char *physical_name);
static bool sqlite_table_name_exists(mylite_db *database, const char *name);
static const char *sqlite_affinity_for_catalog_data_type(const char *data_type);
static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_table_ddl_execute_alter_table_rebuild(mylite_stmt *stmt,
                                                 struct mylite_alter_table_model *model)
{
    struct mylite_statement_atomicity atomicity = {0};
    char *shadow_name = alter_table_shadow_physical_name(stmt->database, model->physical_name);
    int64_t copied_rows = 0;
    int status = MYLITE_OK;

    if (shadow_name == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_transaction_begin_statement_atomicity(stmt->database, &atomicity);
    if (status == MYLITE_OK) {
        status = create_alter_table_shadow_table(stmt, model, shadow_name);
    }
    if (status == MYLITE_OK) {
        status = copy_alter_table_rows(stmt, model, shadow_name, &copied_rows);
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_rewrite_alter_table_catalog(
            stmt->database, stmt->alter_table.schema_name, stmt->alter_table.table_name, model);
    }
    if (status == MYLITE_OK) {
        status = swap_alter_table_physical_table(stmt, shadow_name, model->physical_name);
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_append_alter_table_warnings(stmt->database, model);
    }
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_statement_atomicity(stmt->database, &atomicity);
        if (status == MYLITE_OK) {
            if (model->report_copied_rows) {
                stmt->affected_rows = copied_rows;
            } else {
                stmt->affected_rows = 0;
            }
            free(shadow_name);
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_statement_atomicity(stmt->database, &atomicity);
    mylite_diagnostics_clear_warnings(stmt->database);
    free(shadow_name);
    return status;
}

static int create_alter_table_shadow_table(mylite_stmt *stmt,
                                           const struct mylite_alter_table_model *model,
                                           const char *shadow_name)
{
    char *sql = build_alter_table_create_shadow_sql(stmt->database, model, shadow_name);
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_exec(stmt->database->sqlite, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(stmt->database);
}

static char *build_alter_table_create_shadow_sql(mylite_db *database,
                                                 const struct mylite_alter_table_model *model,
                                                 const char *shadow_name)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "CREATE TABLE \"%w\"(", shadow_name);
    for (size_t index = 0U; index < model->column_count; ++index) {
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "\"%w\" %s", model->columns[index].name,
                            sqlite_affinity_for_catalog_data_type(model->columns[index].data_type));
    }
    sqlite3_str_append(sql, ")", 1);
    return sqlite3_str_finish(sql);
}

static int copy_alter_table_rows(mylite_stmt *stmt, const struct mylite_alter_table_model *model,
                                 const char *shadow_name, int64_t *out_copied_rows)
{
    sqlite3_stmt *insert = NULL;
    char *sql = build_alter_table_copy_sql(stmt->database, model, shadow_name);
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    *out_copied_rows = 0;
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(stmt->database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert,
                            NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(stmt->database);
    }
    status = bind_alter_table_added_column_values(stmt, insert, model);
    if (status == MYLITE_OK) {
        rc = sqlite3_step(insert);
        if (rc != SQLITE_DONE) {
            status = mylite_diagnostics_set_sqlite_error(stmt->database);
        }
    }
    if (status == MYLITE_OK) {
        *out_copied_rows = sqlite3_changes64(stmt->database->sqlite);
    }
    sqlite3_finalize(insert);
    return status;
}

static char *build_alter_table_copy_sql(mylite_db *database,
                                        const struct mylite_alter_table_model *model,
                                        const char *shadow_name)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "INSERT INTO \"%w\"(", shadow_name);
    for (size_t index = 0U; index < model->column_count; ++index) {
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "\"%w\"", model->columns[index].name);
    }
    sqlite3_str_append(sql, ") SELECT ", (int)strlen(") SELECT "));
    for (size_t index = 0U; index < model->column_count; ++index) {
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        if (model->columns[index].source_name == NULL) {
            sqlite3_str_append(sql, "?", 1);
        } else {
            sqlite3_str_appendf(sql, "\"%w\"", model->columns[index].source_name);
        }
    }
    sqlite3_str_appendf(sql, " FROM \"%w\"", model->physical_name);
    return sqlite3_str_finish(sql);
}

static int bind_alter_table_added_column_values(mylite_stmt *stmt, sqlite3_stmt *insert,
                                                const struct mylite_alter_table_model *model)
{
    int bind_index = 1;

    for (size_t index = 0U; index < model->column_count; ++index) {
        struct mylite_insert_bound_value value = {0};
        int status = MYLITE_OK;

        if (model->columns[index].source_name != NULL) {
            continue;
        }

        status = mylite_table_ddl_resolve_alter_table_added_column_value(
            stmt->database, &model->columns[index], &value);
        if (status == MYLITE_OK &&
            mylite_dml_bind_insert_bound_value(insert, bind_index, &value) != SQLITE_OK) {
            status = mylite_diagnostics_set_sqlite_error(stmt->database);
        }
        mylite_dml_insert_bound_value_deinit(&value);
        if (status != MYLITE_OK) {
            return status;
        }
        ++bind_index;
    }
    return MYLITE_OK;
}

static int swap_alter_table_physical_table(mylite_stmt *stmt, const char *shadow_name,
                                           const char *physical_name)
{
    char *sql = sqlite3_mprintf("DROP TABLE \"%w\"; ALTER TABLE \"%w\" RENAME TO \"%w\"",
                                physical_name, shadow_name, physical_name);
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_exec(stmt->database->sqlite, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(stmt->database);
}

static char *alter_table_shadow_physical_name(mylite_db *database, const char *physical_name)
{
    enum { max_shadow_name_suffix = 1000U };

    for (unsigned int suffix = 1U; suffix < max_shadow_name_suffix; ++suffix) {
        char *candidate = sqlite3_mprintf("%s__alter_shadow_%u",
                                          physical_name == NULL ? "" : physical_name, suffix);

        if (candidate == NULL) {
            return NULL;
        }
        if (!sqlite_table_name_exists(database, candidate)) {
            return candidate;
        }
        sqlite3_free(candidate);
    }
    return NULL;
}

static bool sqlite_table_name_exists(mylite_db *database, const char *name)
{
    sqlite3_stmt *select = NULL;
    static const char sql[] = "SELECT 1 FROM sqlite_schema WHERE type = 'table' AND name = ?";
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    bool exists = false;

    if (rc != SQLITE_OK) {
        return true;
    }
    sqlite3_bind_text(select, 1, name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(select);
    exists = rc == SQLITE_ROW;
    sqlite3_finalize(select);
    return exists;
}

static const char *sqlite_affinity_for_catalog_data_type(const char *data_type)
{
    if (data_type == NULL) {
        return "TEXT";
    }
    if (mylite_ascii_case_equal(data_type, "tinyint") ||
        mylite_ascii_case_equal(data_type, "smallint") ||
        mylite_ascii_case_equal(data_type, "mediumint") ||
        mylite_ascii_case_equal(data_type, "int") || mylite_ascii_case_equal(data_type, "bigint") ||
        mylite_ascii_case_equal(data_type, "bool") ||
        mylite_ascii_case_equal(data_type, "boolean")) {
        return "INTEGER";
    }
    if (mylite_ascii_case_equal(data_type, "float") ||
        mylite_ascii_case_equal(data_type, "double")) {
        return "REAL";
    }
    if (mylite_ascii_case_equal(data_type, "decimal")) {
        return "NUMERIC";
    }
    if (mylite_ascii_case_equal(data_type, "binary") ||
        mylite_ascii_case_equal(data_type, "varbinary") ||
        mylite_ascii_case_equal(data_type, "tinyblob") ||
        mylite_ascii_case_equal(data_type, "blob") ||
        mylite_ascii_case_equal(data_type, "mediumblob") ||
        mylite_ascii_case_equal(data_type, "longblob")) {
        return "BLOB";
    }
    return "TEXT";
}

static sqlite3_destructor_type sqlite_transient_destructor(void)
{
    // SQLite's public macro intentionally uses this sentinel pointer value.
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
