#include "mylite_table_ddl_alter_warnings.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"
#include "mylite_table_ddl_types.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static bool alter_table_indexes_are_duplicate_warning_candidates(
    const struct mylite_alter_table_index *index,
    const struct mylite_alter_table_index *previous
);

static bool alter_table_index_parts_match(
    const struct mylite_alter_table_index_part *left,
    const struct mylite_alter_table_index_part *right
);

static int append_using_hash_warning(mylite_db *database);

static int append_duplicate_index_warning(mylite_db *database, const char *index_name);

int mylite_table_ddl_append_alter_table_warnings(
    mylite_db *database,
    const struct mylite_alter_table_model *model
) {
    for (size_t index = 0U; index < model->index_count; ++index) {
        if (model->indexes[index].hash_fallback_warning) {
            int status = append_using_hash_warning(database);

            if (status != MYLITE_OK) {
                return status;
            }
        }
    }
    for (size_t index = 0U; index < model->index_count; ++index) {
        for (size_t previous = 0U; previous < index; ++previous) {
            if (alter_table_indexes_are_duplicate_warning_candidates(
                    &model->indexes[index],
                    &model->indexes[previous]
                )) {
                return append_duplicate_index_warning(database, model->indexes[index].name);
            }
        }
    }
    return MYLITE_OK;
}

static bool alter_table_indexes_are_duplicate_warning_candidates(
    const struct mylite_alter_table_index *index,
    const struct mylite_alter_table_index *previous
) {
    if (!index->changed && !previous->changed) {
        return false;
    }
    if (index->non_unique != previous->non_unique || index->part_count != previous->part_count) {
        return false;
    }

    for (size_t part = 0U; part < index->part_count; ++part) {
        if (!alter_table_index_parts_match(&index->parts[part], &previous->parts[part])) {
            return false;
        }
    }
    return true;
}

static bool alter_table_index_parts_match(
    const struct mylite_alter_table_index_part *left,
    const struct mylite_alter_table_index_part *right
) {
    const char *left_collation = left->collation == NULL ? "" : left->collation;
    const char *right_collation = right->collation == NULL ? "" : right->collation;

    if (!mylite_ascii_case_equal(left->column_name, right->column_name)) {
        return false;
    }
    if (strcmp(left_collation, right_collation) != 0) {
        return false;
    }
    if (left->has_sub_part != right->has_sub_part) {
        return false;
    }
    if (!left->has_sub_part) {
        return true;
    }
    return left->sub_part == right->sub_part;
}

static int append_using_hash_warning(mylite_db *database) {
    return mylite_diagnostics_append_warning(
        database,
        MYLITE_MYSQL_ER_WARN_USING_OTHER_HANDLER,
        "This storage engine does not support HASH indexes; using BTREE instead"
    );
}

static int append_duplicate_index_warning(mylite_db *database, const char *index_name) {
    char *message = sqlite3_mprintf(
        "Duplicate index '%q' defined on the table. This is deprecated and will be disallowed in "
        "a future release.",
        index_name
    );
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_warning(database, MYLITE_MYSQL_ER_DUP_INDEX, message);
    sqlite3_free(message);
    return status;
}
