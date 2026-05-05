#include "mylite_table_ddl_alter.h"

#include "mylite_span.h"

const struct mylite_alter_table_column *
mylite_table_ddl_find_alter_table_column(const struct mylite_alter_table_model *model,
                                         const char *name)
{
    size_t index = mylite_table_ddl_alter_table_column_index(model, name);

    return index == model->column_count ? NULL : &model->columns[index];
}

size_t mylite_table_ddl_alter_table_column_index(const struct mylite_alter_table_model *model,
                                                 const char *name)
{
    for (size_t index = 0U; index < model->column_count; ++index) {
        if (mylite_ascii_case_equal(model->columns[index].name, name)) {
            return index;
        }
    }
    return model->column_count;
}
