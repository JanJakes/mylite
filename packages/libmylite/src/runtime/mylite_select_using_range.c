#include "mylite_select_using_range.h"

#include "mylite_span.h"

static bool
select_using_column_range_is_in_range(const struct mylite_select_join_using_column *column,
                                      struct mylite_select_table_range range);

size_t mylite_select_count_column_parts_using_matches(const struct mylite_select_plan *plan,
                                                      const char *column_name,
                                                      struct mylite_select_table_range range,
                                                      size_t *match_index)
{
    size_t match_count = 0U;

    if (plan == NULL || column_name == NULL || match_index == NULL) {
        return 0U;
    }

    for (size_t index = 0U; index < plan->using_column_count; ++index) {
        if (!mylite_ascii_case_equal(plan->using_columns[index].name, column_name) ||
            !select_using_column_range_is_in_range(&plan->using_columns[index], range)) {
            continue;
        }
        *match_index = plan->using_columns[index].coalesced_column_index;
        ++match_count;
    }
    return match_count;
}

bool mylite_select_column_index_is_using_column_in_range(const struct mylite_select_plan *plan,
                                                         size_t column_index,
                                                         struct mylite_select_table_range range)
{
    if (plan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < plan->using_column_count; ++index) {
        const struct mylite_select_join_using_column *column = &plan->using_columns[index];

        if (select_using_column_range_is_in_range(column, range) &&
            (column->left_column_index == column_index ||
             column->right_column_index == column_index)) {
            return true;
        }
    }
    return false;
}

static bool
select_using_column_range_is_in_range(const struct mylite_select_join_using_column *column,
                                      struct mylite_select_table_range range)
{
    size_t range_end = range.first_table + range.table_count;
    size_t column_end = column->first_table + column->table_count;

    if (column->first_table < range.first_table) {
        return false;
    }
    if (column_end > range_end) {
        return false;
    }
    return true;
}
