#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SELECT_ANALYSIS_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SELECT_ANALYSIS_H

#include "mylite_execution_value.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_sql_ast_node;

struct mylite_select_analysis_session_key {
    uint64_t sql_mode;
    uint64_t last_insert_id;
    int time_zone_offset_minutes;
    bool sql_auto_is_null;
};

struct mylite_select_analysis_generation_key {
    uint64_t catalog;
    uint64_t sqlite_schema;
};

struct mylite_select_analysis_state {
    char *lowered_sql;
    enum mylite_stmt_binding_type *binding_types;
    struct mylite_select_analysis_session_key session_key;
    struct mylite_select_analysis_generation_key generation_key;
    size_t binding_type_count;
    bool parameter_values_are_reusable;
    bool valid;
};

int mylite_execution_select_parameters_are_plan_reusable(
    const struct mylite_sql_ast_node *statement,
    bool *out_reusable
);
bool mylite_execution_select_analysis_matches(
    const struct mylite_select_analysis_state *analysis,
    const struct mylite_stmt_binding *bindings,
    size_t binding_count,
    struct mylite_select_analysis_session_key session_key,
    struct mylite_select_analysis_generation_key generation_key
);
int mylite_execution_select_analysis_capture(
    struct mylite_select_analysis_state *analysis,
    const struct mylite_stmt_binding *bindings,
    size_t binding_count,
    struct mylite_select_analysis_session_key session_key,
    struct mylite_select_analysis_generation_key generation_key,
    bool parameter_values_are_reusable
);
void mylite_execution_select_analysis_deinit(struct mylite_select_analysis_state *analysis);

#endif
