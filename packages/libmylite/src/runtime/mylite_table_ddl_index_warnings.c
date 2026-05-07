#include "mylite_table_ddl_index_warnings.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"
#include "mylite_table_ddl.h"
#include "sqlite3.h"

#include <stdint.h>
#include <string.h>

static int append_index_hash_warning(mylite_db *database);

static int append_fulltext_doc_id_warning(mylite_db *database);

static int append_index_duplicate_warning(mylite_db *database, const char *index_name);

static int maybe_append_fulltext_doc_id_warning(
    mylite_db *database,
    const struct mylite_alter_table_model *model
);

static int maybe_append_duplicate_index_warning(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_index *index
);

static bool alter_table_index_matches_create_index(
    const struct mylite_alter_table_index *table_index,
    const struct mylite_create_table_index *create_index
);

int mylite_table_ddl_append_create_index_warnings(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_index *index
) {
    int status = MYLITE_OK;

    if (index->is_fulltext) {
        return maybe_append_fulltext_doc_id_warning(database, model);
    }
    if (index->algorithm == MYLITE_SQL_AST_INDEX_ALGORITHM_HASH) {
        status = append_index_hash_warning(database);
    }
    if (status == MYLITE_OK) {
        status = maybe_append_duplicate_index_warning(database, model, index);
    }
    return status;
}

static int append_index_hash_warning(mylite_db *database) {
    return mylite_diagnostics_append_warning(
        database,
        MYLITE_MYSQL_ER_WARN_USING_OTHER_HANDLER,
        "This storage engine does not support HASH indexes; using BTREE instead"
    );
}

static int append_fulltext_doc_id_warning(mylite_db *database) {
    return mylite_diagnostics_append_warning(
        database,
        MYLITE_MYSQL_ER_WARN_INNODB_FT_DOC_ID,
        "InnoDB rebuilding table to add column FTS_DOC_ID"
    );
}

static int append_index_duplicate_warning(mylite_db *database, const char *index_name) {
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

static int maybe_append_fulltext_doc_id_warning(
    mylite_db *database,
    const struct mylite_alter_table_model *model
) {
    for (size_t index = 0U; index < model->index_count; ++index) {
        if (mylite_ascii_case_equal(model->indexes[index].index_type, "FULLTEXT")) {
            return MYLITE_OK;
        }
    }
    return append_fulltext_doc_id_warning(database);
}

static int maybe_append_duplicate_index_warning(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_index *index
) {
    for (size_t table_index = 0U; table_index < model->index_count; ++table_index) {
        if (alter_table_index_matches_create_index(&model->indexes[table_index], index)) {
            return append_index_duplicate_warning(database, index->name);
        }
    }
    return MYLITE_OK;
}

static bool alter_table_index_matches_create_index(
    const struct mylite_alter_table_index *table_index,
    const struct mylite_create_table_index *create_index
) {
    int expected_non_unique = 1;

    if (create_index->is_unique) {
        expected_non_unique = 0;
    }
    if (table_index->non_unique != expected_non_unique ||
        table_index->part_count != create_index->part_count ||
        !mylite_ascii_case_equal(table_index->index_type, "BTREE")) {
        return false;
    }

    for (size_t part = 0U; part < create_index->part_count; ++part) {
        const struct mylite_alter_table_index_part *table_part = &table_index->parts[part];
        const struct mylite_create_table_key_part *create_part = &create_index->parts[part];

        if (!mylite_ascii_case_equal(table_part->column_name, create_part->column_name) ||
            strcmp(
                table_part->collation == NULL ? "" : table_part->collation,
                mylite_table_ddl_index_collation_for_order(create_part->order)
            ) != 0 ||
            table_part->has_sub_part != create_part->has_prefix_length ||
            (table_part->has_sub_part &&
             (uint64_t)table_part->sub_part != create_part->prefix_length)) {
            return false;
        }
    }
    return true;
}
