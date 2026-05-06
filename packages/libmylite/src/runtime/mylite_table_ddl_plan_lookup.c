#include "mylite_table_ddl_plan_lookup.h"

#include "mylite_span.h"
#include "mylite_table_ddl_types.h"

#include <stddef.h>

const struct mylite_create_table_column *mylite_table_ddl_find_create_table_column(
    const struct mylite_create_table_plan *plan,
    const char *name
) {
    for (size_t index = 0U; index < plan->column_count; ++index) {
        if (mylite_ascii_case_equal(plan->columns[index].name, name)) {
            return &plan->columns[index];
        }
    }
    return NULL;
}
