#include "mylite_table_ddl_alter.h"

#include "mylite_diagnostics.h"
#include "mylite_dml.h"
#include "mylite_error_codes.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <string.h>

static int validate_alter_table_unique_index(mylite_db *database,
                                             const struct mylite_alter_table_model *model,
                                             const struct mylite_alter_table_index *index);
static int validate_alter_table_columns(mylite_db *database, struct mylite_alter_table_model *model,
                                        bool *out_has_visible_column,
                                        size_t *out_auto_increment_count);
static int validate_alter_table_source_not_null(mylite_db *database,
                                                const struct mylite_alter_table_model *model,
                                                const struct mylite_alter_table_column *column);
static int validate_alter_table_auto_increment_shape(mylite_db *database,
                                                     struct mylite_alter_table_model *model,
                                                     size_t auto_increment_count);
static const struct mylite_alter_table_column *
find_alter_table_auto_increment_column(const struct mylite_alter_table_model *model);
static bool
alter_table_auto_increment_column_is_indexed(const struct mylite_alter_table_model *model,
                                             const struct mylite_alter_table_column *auto_column);
static int set_alter_table_all_invisible_error(mylite_db *database);
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

int mylite_table_ddl_validate_alter_table_final_model(mylite_db *database,
                                                      struct mylite_alter_table_model *model)
{
    bool has_visible_column = false;
    size_t auto_increment_count = 0U;
    int status = MYLITE_OK;

    if (database == NULL || model == NULL) {
        return MYLITE_MISUSE;
    }
    if (model->column_count == 0U) {
        return mylite_table_ddl_set_alter_table_cant_remove_all_columns_error(database);
    }
    status =
        validate_alter_table_columns(database, model, &has_visible_column, &auto_increment_count);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!has_visible_column) {
        return set_alter_table_all_invisible_error(database);
    }
    return validate_alter_table_auto_increment_shape(database, model, auto_increment_count);
}

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

static int validate_alter_table_columns(mylite_db *database, struct mylite_alter_table_model *model,
                                        bool *out_has_visible_column,
                                        size_t *out_auto_increment_count)
{
    *out_has_visible_column = false;
    *out_auto_increment_count = 0U;
    if (model->columns == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t column = 0U; column < model->column_count; ++column) {
        for (size_t next = column + 1U; next < model->column_count; ++next) {
            if (mylite_ascii_case_equal(model->columns[column].name, model->columns[next].name)) {
                return mylite_table_ddl_set_alter_table_duplicate_column_error(
                    database, model->columns[next].name);
            }
        }
        if (model->columns[column].visible) {
            *out_has_visible_column = true;
        }
        if (model->columns[column].auto_increment) {
            ++*out_auto_increment_count;
        }
        if (!model->columns[column].nullable && model->columns[column].source_name != NULL) {
            int status =
                validate_alter_table_source_not_null(database, model, &model->columns[column]);

            if (status != MYLITE_OK) {
                return status;
            }
        }
    }

    return MYLITE_OK;
}

static int validate_alter_table_source_not_null(mylite_db *database,
                                                const struct mylite_alter_table_model *model,
                                                const struct mylite_alter_table_column *column)
{
    char *sql = sqlite3_mprintf("SELECT 1 FROM \"%w\" WHERE \"%w\" IS NULL LIMIT 1",
                                model->physical_name, column->source_name);
    sqlite3_stmt *select = NULL;
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
    rc = sqlite3_step(select);
    sqlite3_finalize(select);
    if (rc == SQLITE_ROW) {
        return mylite_dml_set_not_null_column_error(database, column->name);
    }
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int validate_alter_table_auto_increment_shape(mylite_db *database,
                                                     struct mylite_alter_table_model *model,
                                                     size_t auto_increment_count)
{
    if (auto_increment_count > 1U) {
        return mylite_table_ddl_set_alter_table_wrong_auto_increment_error(database);
    }
    if (auto_increment_count == 1U) {
        const struct mylite_alter_table_column *auto_column =
            find_alter_table_auto_increment_column(model);

        if (auto_column == NULL || auto_column->nullable ||
            !alter_table_auto_increment_column_is_indexed(model, auto_column)) {
            return mylite_table_ddl_set_alter_table_wrong_auto_increment_error(database);
        }
    }
    if (auto_increment_count == 0U) {
        model->clear_auto_increment = true;
    }
    return MYLITE_OK;
}

static const struct mylite_alter_table_column *
find_alter_table_auto_increment_column(const struct mylite_alter_table_model *model)
{
    if (model->columns == NULL) {
        return NULL;
    }
    for (size_t column = 0U; column < model->column_count; ++column) {
        if (model->columns[column].auto_increment) {
            return &model->columns[column];
        }
    }
    return NULL;
}

static bool
alter_table_auto_increment_column_is_indexed(const struct mylite_alter_table_model *model,
                                             const struct mylite_alter_table_column *auto_column)
{
    for (size_t index = 0U; index < model->index_count; ++index) {
        for (size_t part = 0U; part < model->indexes[index].part_count; ++part) {
            if (mylite_ascii_case_equal(model->indexes[index].parts[part].column_name,
                                        auto_column->name)) {
                return true;
            }
        }
    }
    return false;
}

static int set_alter_table_all_invisible_error(mylite_db *database)
{
    int status = mylite_diagnostics_set_error_message(
        database, "A table must have at least one visible column");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database,
                                             MYLITE_MYSQL_ER_INVISIBLE_NOT_NULL_WITHOUT_DEFAULT,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_table_ddl_resolve_alter_table_added_column_value(
    mylite_db *database, const struct mylite_alter_table_column *column,
    struct mylite_insert_bound_value *out_value)
{
    struct mylite_insert_table_column insert_column = {0};

    if (database == NULL || column == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    insert_column = (struct mylite_insert_table_column){
        .name = column->name,
        .default_text = column->column_default,
        .data_type = column->data_type,
        .extra = column->extra,
        .nullable = column->nullable,
        .auto_increment = column->auto_increment,
        .generated_default = mylite_text_contains_word(column->extra, "DEFAULT_GENERATED"),
    };

    if (column->auto_increment) {
        return mylite_table_ddl_set_alter_table_wrong_auto_increment_error(database);
    }
    if (column->column_default != NULL) {
        return mylite_dml_resolve_insert_default_bound_value(database, &insert_column, 0U, NULL,
                                                             out_value);
    }
    if (column->nullable) {
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        return MYLITE_OK;
    }
    return mylite_dml_resolve_insert_implicit_expression_default(database, &insert_column,
                                                                 out_value);
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
