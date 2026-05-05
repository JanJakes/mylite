#include "mylite_table_ddl_alter_unique_validate.h"

#include "mylite_diagnostics.h"
#include "mylite_dml.h"
#include "mylite_runtime.h"
#include "mylite_table_ddl_alter.h"
#include "mylite_table_ddl_alter_column_value.h"
#include "sqlite3.h"

#include <string.h>

static int validate_alter_table_unique_index(mylite_db *database,
                                             const struct mylite_alter_table_model *model,
                                             const struct mylite_alter_table_index *index);
static int build_alter_table_unique_duplicate_sql(mylite_db *database,
                                                  const struct mylite_alter_table_model *model,
                                                  const struct mylite_alter_table_index *index,
                                                  char **out_sql);
static int
append_alter_table_unique_part_expression(mylite_db *database, sqlite3_str *sql,
                                          const struct mylite_alter_table_model *model,
                                          const struct mylite_alter_table_index_part *part);
static int append_alter_table_unique_part_not_null_expression(
    mylite_db *database, sqlite3_str *sql, const struct mylite_alter_table_model *model,
    const struct mylite_alter_table_index_part *part);
static int
append_alter_table_added_column_value_literal(mylite_db *database, sqlite3_str *sql,
                                              const struct mylite_alter_table_column *column);

int mylite_table_ddl_validate_alter_table_unique_indexes(
    mylite_db *database, const struct mylite_alter_table_model *model)
{
    if (database == NULL || model == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < model->index_count; ++index) {
        if (model->indexes[index].non_unique == 0) {
            int status = validate_alter_table_unique_index(database, model, &model->indexes[index]);

            if (status != MYLITE_OK) {
                return status;
            }
        }
    }
    return MYLITE_OK;
}

static int validate_alter_table_unique_index(mylite_db *database,
                                             const struct mylite_alter_table_model *model,
                                             const struct mylite_alter_table_index *index)
{
    char *sql = NULL;
    sqlite3_stmt *select = NULL;
    int status = build_alter_table_unique_duplicate_sql(database, model, index, &sql);
    int rc = SQLITE_OK;

    if (status != MYLITE_OK) {
        return status;
    }
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    rc = sqlite3_step(select);
    sqlite3_finalize(select);
    if (rc == SQLITE_ROW) {
        (void)mylite_diagnostics_set_error_message_parts(database, "Duplicate entry for key '",
                                                         index->name, "'");
        return MYLITE_EXEC_ERROR;
    }
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int build_alter_table_unique_duplicate_sql(mylite_db *database,
                                                  const struct mylite_alter_table_model *model,
                                                  const struct mylite_alter_table_index *index,
                                                  char **out_sql)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    int status = MYLITE_OK;

    *out_sql = NULL;
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    sqlite3_str_append(sql, "SELECT 1 FROM (SELECT ", (int)strlen("SELECT 1 FROM (SELECT "));
    for (size_t part = 0U; part < index->part_count; ++part) {
        if (part != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        status =
            append_alter_table_unique_part_expression(database, sql, model, &index->parts[part]);
        if (status != MYLITE_OK) {
            sqlite3_free(sqlite3_str_finish(sql));
            return status;
        }
    }
    sqlite3_str_appendf(sql, " FROM \"%w\" WHERE ", model->physical_name);
    for (size_t part = 0U; part < index->part_count; ++part) {
        if (part != 0U) {
            sqlite3_str_append(sql, " AND ", (int)strlen(" AND "));
        }
        status = append_alter_table_unique_part_not_null_expression(database, sql, model,
                                                                    &index->parts[part]);
        if (status != MYLITE_OK) {
            sqlite3_free(sqlite3_str_finish(sql));
            return status;
        }
    }
    sqlite3_str_append(sql, " GROUP BY ", (int)strlen(" GROUP BY "));
    for (size_t part = 0U; part < index->part_count; ++part) {
        if (part != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "%d", (int)(part + 1U));
    }
    sqlite3_str_append(sql, " HAVING COUNT(*) > 1) LIMIT 1",
                       (int)strlen(" HAVING COUNT(*) > 1) LIMIT 1"));
    *out_sql = sqlite3_str_finish(sql);
    if (*out_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int
append_alter_table_unique_part_expression(mylite_db *database, sqlite3_str *sql,
                                          const struct mylite_alter_table_model *model,
                                          const struct mylite_alter_table_index_part *part)
{
    const struct mylite_alter_table_column *column =
        mylite_table_ddl_find_alter_table_column(model, part->column_name);

    if (column == NULL) {
        return MYLITE_MISUSE;
    }
    if (part->has_sub_part) {
        sqlite3_str_append(sql, "substr(", (int)strlen("substr("));
    }
    if (column->source_name == NULL) {
        int status = append_alter_table_added_column_value_literal(database, sql, column);

        if (status != MYLITE_OK) {
            return status;
        }
    } else {
        sqlite3_str_appendf(sql, "\"%w\"", column->source_name);
    }
    if (part->has_sub_part) {
        sqlite3_str_appendf(sql, ",1,%lld)", (long long)part->sub_part);
    }
    return MYLITE_OK;
}

static int
append_alter_table_unique_part_not_null_expression(mylite_db *database, sqlite3_str *sql,
                                                   const struct mylite_alter_table_model *model,
                                                   const struct mylite_alter_table_index_part *part)
{
    const struct mylite_alter_table_column *column =
        mylite_table_ddl_find_alter_table_column(model, part->column_name);

    if (column == NULL) {
        return MYLITE_MISUSE;
    }
    if (column->source_name == NULL) {
        int status = append_alter_table_added_column_value_literal(database, sql, column);

        if (status != MYLITE_OK) {
            return status;
        }
    } else {
        sqlite3_str_appendf(sql, "\"%w\"", column->source_name);
    }
    sqlite3_str_append(sql, " IS NOT NULL", (int)strlen(" IS NOT NULL"));
    return MYLITE_OK;
}

static int
append_alter_table_added_column_value_literal(mylite_db *database, sqlite3_str *sql,
                                              const struct mylite_alter_table_column *column)
{
    struct mylite_insert_bound_value value = {0};
    int status = mylite_table_ddl_resolve_alter_table_added_column_value(database, column, &value);

    if (status != MYLITE_OK) {
        return status;
    }
    switch (value.kind) {
    case MYLITE_INSERT_BOUND_NULL:
        sqlite3_str_append(sql, "NULL", (int)strlen("NULL"));
        break;
    case MYLITE_INSERT_BOUND_INTEGER:
        sqlite3_str_appendf(sql, "%lld", (long long)value.integer_value);
        break;
    case MYLITE_INSERT_BOUND_REAL:
        sqlite3_str_appendf(sql, "%.15g", value.real_value);
        break;
    case MYLITE_INSERT_BOUND_TEXT:
        sqlite3_str_appendf(sql, "%Q", value.text_value);
        break;
    }
    mylite_dml_insert_bound_value_deinit(&value);
    return MYLITE_OK;
}
