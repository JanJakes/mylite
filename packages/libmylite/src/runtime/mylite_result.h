#ifndef MYLITE_RUNTIME_MYLITE_RESULT_H
#define MYLITE_RUNTIME_MYLITE_RESULT_H

#include <mylite/mylite.h>

#include <stddef.h>
#include <stdint.h>

struct mylite_result {
    char **column_names;
    char **values;
    size_t column_count;
    size_t column_capacity;
    size_t row_count;
    size_t row_capacity;
    int64_t affected_rows;
    size_t warning_count;
};

int mylite_result_create(mylite_result **out_result);
int mylite_result_append_column(mylite_result *result, const char *name);
int mylite_result_append_text_row(mylite_result *result, const char *const *values);
void mylite_result_set_affected_rows(mylite_result *result, int64_t affected_rows);
void mylite_result_set_warning_count(mylite_result *result, size_t warning_count);

#endif
