#ifndef MYLITE_BENCHMARK_CSV_H
#define MYLITE_BENCHMARK_CSV_H

#include <stddef.h>

struct mylite_benchmark_owned_query {
    char *sql;
    size_t length;
};

struct mylite_benchmark_owned_query_list {
    struct mylite_benchmark_owned_query *items;
    size_t count;
    size_t capacity;
};

int mylite_benchmark_load_csv_queries(
    const char *path,
    struct mylite_benchmark_owned_query_list *out_queries
);
int mylite_benchmark_parse_single_column_csv(
    const char *data,
    size_t length,
    struct mylite_benchmark_owned_query_list *out_queries
);
void mylite_benchmark_owned_query_list_deinit(struct mylite_benchmark_owned_query_list *queries);

#endif
