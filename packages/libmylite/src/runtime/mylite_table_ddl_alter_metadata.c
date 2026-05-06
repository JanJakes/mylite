#include "mylite_table_ddl_alter.h"

#include "mylite_diagnostics.h"
#include "mylite_span.h"

#include <stdlib.h>
#include <string.h>

static const char *alter_table_column_key(
    const struct mylite_alter_table_model *model,
    const char *column_name
);

static int refresh_alter_table_column_keys(struct mylite_alter_table_model *model);

static int refresh_alter_table_index_nullability(struct mylite_alter_table_model *model);

int mylite_table_ddl_refresh_alter_table_index_metadata(
    mylite_db *database,
    struct mylite_alter_table_model *model
) {
    int status = MYLITE_OK;

    if (database == NULL || model == NULL) {
        return MYLITE_MISUSE;
    }
    status = refresh_alter_table_column_keys(model);
    if (status == MYLITE_OK) {
        status = refresh_alter_table_index_nullability(model);
    }
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    return status;
}

static const char *alter_table_column_key(
    const struct mylite_alter_table_model *model,
    const char *column_name
) {
    bool indexed = false;
    bool unique = false;

    for (size_t index = 0U; index < model->index_count; ++index) {
        for (size_t part = 0U; part < model->indexes[index].part_count; ++part) {
            if (!mylite_ascii_case_equal(
                    model->indexes[index].parts[part].column_name,
                    column_name
                )) {
                continue;
            }
            indexed = true;
            if (mylite_ascii_case_equal(model->indexes[index].name, "PRIMARY")) {
                return "PRI";
            }
            if (model->indexes[index].non_unique == 0 && part == 0U) {
                unique = true;
            }
        }
    }
    if (unique) {
        return "UNI";
    }
    if (indexed) {
        return "MUL";
    }
    return "";
}

static int refresh_alter_table_column_keys(struct mylite_alter_table_model *model) {
    for (size_t column = 0U; column < model->column_count; ++column) {
        const char *key = alter_table_column_key(model, model->columns[column].name);
        char *copy = mylite_copy_span_text(key, strlen(key));

        if (copy == NULL) {
            return MYLITE_NOMEM;
        }
        free(model->columns[column].column_key);
        model->columns[column].column_key = copy;
    }
    return MYLITE_OK;
}

static int refresh_alter_table_index_nullability(struct mylite_alter_table_model *model) {
    for (size_t index = 0U; index < model->index_count; ++index) {
        for (size_t part = 0U; part < model->indexes[index].part_count; ++part) {
            struct mylite_alter_table_index_part *index_part = &model->indexes[index].parts[part];
            const struct mylite_alter_table_column *column =
                mylite_table_ddl_find_alter_table_column(model, index_part->column_name);
            const char *nullable = "";
            char *copy = NULL;

            if (column != NULL && column->nullable) {
                nullable = "YES";
            }

            copy = mylite_copy_span_text(nullable, strlen(nullable));
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            free(index_part->nullable);
            index_part->nullable = copy;
        }
    }
    return MYLITE_OK;
}
