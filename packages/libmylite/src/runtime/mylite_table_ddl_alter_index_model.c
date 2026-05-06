#include "mylite_table_ddl_alter_index_model.h"

#include <mylite/mylite.h>

#include "mylite_span.h"
#include "mylite_table_ddl.h"

#include <stdlib.h>

bool mylite_table_ddl_alter_table_index_name_exists(
    const struct mylite_alter_table_model *model,
    const char *name
) {
    return mylite_table_ddl_alter_table_index_index(model, name) < model->index_count;
}

size_t mylite_table_ddl_alter_table_index_index(
    const struct mylite_alter_table_model *model,
    const char *name
) {
    for (size_t index = 0U; index < model->index_count; ++index) {
        if (mylite_ascii_case_equal(model->indexes[index].name, name)) {
            return index;
        }
    }
    return model->index_count;
}

int mylite_table_ddl_insert_alter_table_index(
    struct mylite_alter_table_model *model,
    struct mylite_alter_table_index table_index,
    size_t position
) {
    struct mylite_alter_table_index *indexes = NULL;

    if (position > model->index_count) {
        return MYLITE_MISUSE;
    }

    indexes = realloc(model->indexes, (model->index_count + 1U) * sizeof(*model->indexes));
    if (indexes == NULL) {
        return MYLITE_NOMEM;
    }
    model->indexes = indexes;
    for (size_t index = model->index_count; index > position; --index) {
        model->indexes[index] = model->indexes[index - 1U];
    }
    model->indexes[position] = table_index;
    ++model->index_count;
    return MYLITE_OK;
}

int mylite_table_ddl_remove_alter_table_index(
    struct mylite_alter_table_model *model,
    size_t index
) {
    if (index >= model->index_count) {
        return MYLITE_MISUSE;
    }

    mylite_table_ddl_alter_table_index_deinit(&model->indexes[index]);
    for (size_t next = index + 1U; next < model->index_count; ++next) {
        model->indexes[next - 1U] = model->indexes[next];
    }
    --model->index_count;
    model->indexes[model->index_count] = (struct mylite_alter_table_index){0};
    return MYLITE_OK;
}
