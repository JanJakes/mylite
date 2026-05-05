#include "mylite_table_ddl_alter_index.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"
#include "mylite_table_ddl.h"
#include "mylite_table_ddl_alter_index_model.h"

#include <stdlib.h>
#include <string.h>

static int set_alter_table_column_nullable(mylite_db *database,
                                           struct mylite_alter_table_column *column, bool nullable);
static int set_alter_table_multiple_primary_key_error(mylite_db *database);
static int set_alter_table_missing_key_column_error(mylite_db *database, const char *column_name);

int mylite_table_ddl_validate_alter_table_added_index(mylite_db *database,
                                                      const struct mylite_alter_table_model *model,
                                                      const struct mylite_create_table_index *index,
                                                      const char *index_name, bool is_primary)
{
    if (index->has_with_parser) {
        (void)mylite_diagnostics_set_error_message(
            database, "WITH PARSER is only supported for FULLTEXT indexes");
        return MYLITE_EXEC_ERROR;
    }
    if (is_primary && mylite_table_ddl_alter_table_index_name_exists(model, "PRIMARY")) {
        return set_alter_table_multiple_primary_key_error(database);
    }
    if (!is_primary && mylite_ascii_case_equal(index_name, "PRIMARY")) {
        return mylite_table_ddl_set_alter_table_duplicate_key_name_error(database, index_name);
    }
    if (mylite_table_ddl_alter_table_index_name_exists(model, index_name)) {
        return mylite_table_ddl_set_alter_table_duplicate_key_name_error(database, index_name);
    }
    if (is_primary && !index->is_visible) {
        return mylite_table_ddl_set_alter_table_primary_invisible_error(database);
    }
    for (size_t part = 0U; part < index->part_count; ++part) {
        if (mylite_table_ddl_find_alter_table_column(model, index->parts[part].column_name) ==
            NULL) {
            return set_alter_table_missing_key_column_error(database,
                                                            index->parts[part].column_name);
        }
        for (size_t previous = 0U; previous < part; ++previous) {
            if (mylite_ascii_case_equal(index->parts[previous].column_name,
                                        index->parts[part].column_name)) {
                return mylite_table_ddl_set_alter_table_duplicate_column_error(
                    database, index->parts[part].column_name);
            }
        }
    }
    return MYLITE_OK;
}

int mylite_table_ddl_validate_alter_table_primary_key_values(
    const struct mylite_table_ddl_alter_callbacks *callbacks,
    const struct mylite_alter_table_model *model, const struct mylite_create_table_index *index)
{
    if (callbacks == NULL || callbacks->validate_primary_key_part_not_null == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t part = 0U; part < index->part_count; ++part) {
        int status = callbacks->validate_primary_key_part_not_null(callbacks->user_data, model,
                                                                   &index->parts[part]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_table_ddl_apply_alter_table_primary_key_column_nullability(
    mylite_db *database, struct mylite_alter_table_model *model,
    const struct mylite_create_table_index *index)
{
    for (size_t part = 0U; part < index->part_count; ++part) {
        struct mylite_alter_table_column *column = NULL;
        size_t column_index =
            mylite_table_ddl_alter_table_column_index(model, index->parts[part].column_name);
        int status = MYLITE_OK;

        if (column_index == model->column_count) {
            return MYLITE_MISUSE;
        }
        column = &model->columns[column_index];
        status = set_alter_table_column_nullable(database, column, false);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_table_ddl_set_alter_table_duplicate_key_name_error(mylite_db *database,
                                                              const char *index_name)
{
    int status = mylite_diagnostics_set_error_message_parts(database, "Duplicate key name '",
                                                            index_name, "'");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_DUP_KEYNAME,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_table_ddl_set_alter_table_primary_invisible_error(mylite_db *database)
{
    int status =
        mylite_diagnostics_set_error_message(database, "A primary key index cannot be invisible");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_PK_INDEX_CANT_BE_INVISIBLE,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_alter_table_column_nullable(mylite_db *database,
                                           struct mylite_alter_table_column *column, bool nullable)
{
    const char *nullable_text = "NO";
    char *copy = NULL;

    if (nullable) {
        nullable_text = "YES";
    }

    copy = mylite_copy_span_text(nullable_text, strlen(nullable_text));
    if (copy == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    free(column->is_nullable);
    column->is_nullable = copy;
    column->nullable = nullable;
    return MYLITE_OK;
}

static int set_alter_table_multiple_primary_key_error(mylite_db *database)
{
    int status = mylite_diagnostics_set_error_message(database, "Multiple primary key defined");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_MULTIPLE_PRI_KEY,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_alter_table_missing_key_column_error(mylite_db *database, const char *column_name)
{
    int status = mylite_diagnostics_set_error_message_parts(database, "Key column '", column_name,
                                                            "' doesn't exist in table");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_KEY_COLUMN_DOES_NOT_EXITS,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}
