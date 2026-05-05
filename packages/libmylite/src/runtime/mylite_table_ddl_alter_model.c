#include "mylite_table_ddl_alter_model.h"

#include <stdlib.h>

int mylite_table_ddl_add_alter_table_column(struct mylite_alter_table_model *model,
                                            struct mylite_alter_table_column column)
{
    struct mylite_alter_table_column *columns =
        realloc(model->columns, (model->column_count + 1U) * sizeof(*model->columns));

    if (columns == NULL) {
        return MYLITE_NOMEM;
    }

    model->columns = columns;
    model->columns[model->column_count++] = column;
    return MYLITE_OK;
}

int mylite_table_ddl_append_alter_table_index_part(struct mylite_alter_table_index *index,
                                                   struct mylite_alter_table_index_part part)
{
    struct mylite_alter_table_index_part *parts =
        realloc(index->parts, (index->part_count + 1U) * sizeof(*index->parts));

    if (parts == NULL) {
        return MYLITE_NOMEM;
    }
    index->parts = parts;
    index->parts[index->part_count++] = part;
    return MYLITE_OK;
}
