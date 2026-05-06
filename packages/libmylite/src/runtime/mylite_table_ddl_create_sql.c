#include "mylite_table_ddl_create_sql.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_table_ddl.h"
#include "sqlite3.h"

#include <stdlib.h>

static char *build_create_physical_table_sql(mylite_db *database, const char *physical_name,
                                             const struct mylite_schema_default *schema_default,
                                             const struct mylite_create_table_plan *plan);
static const char *
sqlite_affinity_for_descriptor(const struct mylite_column_type_descriptor *descriptor);

int mylite_table_ddl_create_physical_table(mylite_db *database, const char *schema_name,
                                           const struct mylite_schema_default *schema_default,
                                           const struct mylite_create_table_plan *plan)
{
    char *physical_name = mylite_catalog_physical_table_name(schema_name, plan->table_name);
    char *sql = NULL;
    int rc = SQLITE_OK;

    if (physical_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    sql = build_create_physical_table_sql(database, physical_name, schema_default, plan);
    free(physical_name);
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static char *build_create_physical_table_sql(mylite_db *database, const char *physical_name,
                                             const struct mylite_schema_default *schema_default,
                                             const struct mylite_create_table_plan *plan)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "CREATE %sTABLE \"%w\"(", plan->temporary ? "TEMPORARY " : "",
                        physical_name);
    for (size_t index = 0U; index < plan->column_count; ++index) {
        struct mylite_column_type_descriptor descriptor;
        int status = mylite_table_ddl_describe_create_table_column(
            &plan->columns[index], schema_default, &plan->options, &descriptor);

        if (status != MYLITE_OK) {
            sqlite3_free(sqlite3_str_finish(sql));
            return NULL;
        }
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "\"%w\" %s", plan->columns[index].name,
                            sqlite_affinity_for_descriptor(&descriptor));
    }
    sqlite3_str_append(sql, ")", 1);
    return sqlite3_str_finish(sql);
}

static const char *
sqlite_affinity_for_descriptor(const struct mylite_column_type_descriptor *descriptor)
{
    if (descriptor->integer_type != MYLITE_COLUMN_INTEGER_NONE || descriptor->is_boolean_alias) {
        return "INTEGER";
    }
    if (descriptor->is_approximate_numeric) {
        return "REAL";
    }
    if (descriptor->is_exact_numeric) {
        return "NUMERIC";
    }
    if (descriptor->is_binary_string) {
        return "BLOB";
    }
    return "TEXT";
}
