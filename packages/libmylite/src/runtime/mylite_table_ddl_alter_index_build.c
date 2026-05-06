#include "mylite_table_ddl_alter_index_build.h"

#include "mylite_diagnostics.h"
#include "mylite_span.h"
#include "mylite_table_ddl.h"
#include "mylite_table_ddl_alter.h"
#include "mylite_table_ddl_alter_index_model.h"
#include "mylite_table_ddl_alter_model.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int init_alter_table_index_part_from_key_part(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_key_part *source,
    struct mylite_alter_table_index_part *out_part
);

int mylite_table_ddl_init_alter_table_index_from_create_index(
    mylite_db *database,
    struct mylite_alter_table_model *model,
    const struct mylite_create_table_index *source,
    bool is_primary,
    struct mylite_alter_table_index *out_index
) {
    const char *index_type = "BTREE";
    const char *comment = "";
    const char *index_comment = source->comment == NULL ? "" : source->comment;
    const char *is_visible = "NO";
    int status = MYLITE_OK;

    if (source->is_visible) {
        is_visible = "YES";
    }

    *out_index = (struct mylite_alter_table_index){0};
    out_index->index_schema = mylite_copy_nonempty_cstring(model->schema_name);
    out_index->index_type = mylite_copy_span_text(index_type, strlen(index_type));
    out_index->comment = mylite_copy_span_text(comment, strlen(comment));
    out_index->index_comment = mylite_copy_span_text(index_comment, strlen(index_comment));
    out_index->is_visible = mylite_copy_span_text(is_visible, strlen(is_visible));
    out_index->non_unique = 1;
    if (source->is_unique || is_primary) {
        out_index->non_unique = 0;
    }
    out_index->changed = true;
    out_index->display_index_type =
        source->display_index_type && source->algorithm == MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE;
    if (out_index->index_schema == NULL || out_index->index_type == NULL ||
        out_index->comment == NULL || out_index->index_comment == NULL ||
        out_index->is_visible == NULL) {
        mylite_table_ddl_alter_table_index_deinit(out_index);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    for (size_t part = 0U; part < source->part_count; ++part) {
        struct mylite_alter_table_index_part table_part = {0};

        status = init_alter_table_index_part_from_key_part(
            database,
            model,
            &source->parts[part],
            &table_part
        );
        if (status == MYLITE_OK) {
            status = mylite_table_ddl_append_alter_table_index_part(out_index, table_part);
        }
        if (status != MYLITE_OK) {
            mylite_table_ddl_alter_table_index_part_deinit(&table_part);
            mylite_table_ddl_alter_table_index_deinit(out_index);
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_table_ddl_assign_alter_table_generated_index_name(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_index *source,
    char **out_name
) {
    const char *base = NULL;
    unsigned int suffix = 1U;

    *out_name = NULL;
    if (source->part_count == 0U || source->parts[0].column_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "Index has no key parts");
        return MYLITE_EXEC_ERROR;
    }

    base = source->parts[0].column_name;
    for (;;) {
        char *candidate = mylite_table_ddl_generated_index_name_candidate(base, suffix);

        if (candidate == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        if (!mylite_table_ddl_alter_table_index_name_exists(model, candidate)) {
            *out_name = candidate;
            return MYLITE_OK;
        }
        free(candidate);
        ++suffix;
    }
}

static int init_alter_table_index_part_from_key_part(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_key_part *source,
    struct mylite_alter_table_index_part *out_part
) {
    const struct mylite_alter_table_column *column =
        mylite_table_ddl_find_alter_table_column(model, source->column_name);
    const char *nullable = "";
    const char *collation = mylite_table_ddl_index_collation_for_order(source->order);

    if (column != NULL && column->nullable) {
        nullable = "YES";
    }

    *out_part = (struct mylite_alter_table_index_part){0};
    out_part->column_name = mylite_copy_nonempty_cstring(source->column_name);
    out_part->collation = mylite_copy_span_text(collation, strlen(collation));
    out_part->nullable = mylite_copy_span_text(nullable, strlen(nullable));
    if (out_part->column_name == NULL || out_part->collation == NULL ||
        out_part->nullable == NULL) {
        mylite_table_ddl_alter_table_index_part_deinit(out_part);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (source->has_prefix_length) {
        out_part->has_sub_part = true;
        out_part->sub_part = (int64_t)source->prefix_length;
    }
    return MYLITE_OK;
}
