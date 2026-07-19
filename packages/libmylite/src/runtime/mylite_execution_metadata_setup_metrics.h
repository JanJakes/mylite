#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_METADATA_SETUP_METRICS_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_METADATA_SETUP_METRICS_H

enum { MYLITE_EXECUTION_PERFORMANCE_SCHEMA_SETUP_METRIC_COLUMN_COUNT = 6 };

typedef int (*mylite_execution_metadata_row_callback)(void *context, const char *const *values);

int mylite_execution_for_each_performance_schema_setup_metric(
    mylite_execution_metadata_row_callback callback,
    void *context
);

#endif
