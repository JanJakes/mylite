#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_spatial.h"
#include "runtime/mylite_spatial_functions.h"

#include <math.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#  include <windows.h>
#else
#  include <pthread.h>
#  include <sched.h>
#endif

enum {
    internal_srid_size = 4,
    wkb_polygon_header_size = 9,
    wkb_ring_header_size = 4,
    wkb_point_size = 16,
    wkb_byte_order_offset = internal_srid_size,
    wkb_type_offset = wkb_byte_order_offset + 1,
    wkb_ring_count_offset = wkb_type_offset + 4,
    wkb_point_count_offset = wkb_ring_count_offset + 4,
    wkb_points_offset = wkb_point_count_offset + 4,
    wkb_header_size = 9,
    wkb_little_endian = 1,
    wkb_polygon_type = 3,
    wkb_multipolygon_type = 6,
    wkb_nested_ring_count_offset = 5,
    square_ring_point_count = 5,
    polygon_with_hole_ring_count = 2,
    spatial_byte_bit_count = 8,
    spatial_byte_mask = 0xff,
    test_context_capacity = 96,
    candidate_checks_per_segment_limit = 32,
    candidate_slope_tenths_limit = 25,
    candidate_slope_tenths_scale = 10,
    minimum_scaling_vertex_count = 8192,
    maximum_scaling_vertex_count = 65536,
    segment_count_trigger_check = 9,
    index_build_trigger_check = 40,
    after_sort_trigger_check = 50,
    candidate_scan_trigger_check = 80,
    maximum_validation_segment_count = 1048576,
    candidate_budget_comb_rows = 4097,
    component_scaling_count = 4096,
    interrupt_comb_rows = 20001,
    mysql_error_out_of_resources = 1041,
    mysql_error_query_interrupted = 1317,
    mysql_error_max_execution_time_exceeded = 3024,
};

static const double full_circle_radians = 6.283185307179586476925286766559;
static const double comb_exterior_x = 2.0;
static const double large_exterior_radius = 10.0;
static const double component_spacing = 2.0;
static const double component_exterior_margin = 2.0;
static const double component_half_size = 0.5;

struct generated_geometry {
    unsigned char *bytes;
    size_t byte_count;
};

struct injected_control {
    uint64_t check_count;
    uint64_t trigger_check;
    enum mylite_spatial_work_status trigger_status;
};

struct interrupt_thread_context {
    mylite_stmt *statement;
    atomic_bool work_entered;
    atomic_bool release_work;
    int step_rc;
};

static int test_scaling_operation_counts(void);
static int test_component_scaling_operation_counts(void);
static int test_injected_cancellation_and_deadline(void);
static int test_exact_work_limits(void);
static int test_resource_limits(void);
static int test_sql_deadline_and_interrupt(void);
static int expect_direct_validity(
    const struct generated_geometry *geometry,
    const struct mylite_spatial_work_control *control,
    bool should_succeed,
    int expected_error,
    const char *expected_sqlstate, // NOLINT(bugprone-easily-swappable-parameters)
    const char *context
);
static enum mylite_spatial_work_status injected_work_check(void *context);
static int execute_statement(
    mylite_db *database,
    const char *sql, // NOLINT(bugprone-easily-swappable-parameters)
    const char *context
);
static int expect_recovery_query(mylite_db *database, const char *context);
static struct generated_geometry make_regular_polygon(size_t vertex_count);
static struct generated_geometry make_comb_polygon(size_t row_count);
static struct generated_geometry make_large_exterior_hole_polygon(size_t exterior_vertex_count);
static struct generated_geometry make_many_hole_polygon(size_t hole_count);
static struct generated_geometry make_disjoint_multipolygon(size_t polygon_count);
static struct generated_geometry allocate_polygon(size_t point_count);
static void write_square_ring(
    unsigned char *bytes,
    size_t ring_offset, // NOLINT(bugprone-easily-swappable-parameters)
    double min_x,       // NOLINT(bugprone-easily-swappable-parameters)
    double min_y,       // NOLINT(bugprone-easily-swappable-parameters)
    double max_x,
    double max_y
);
static void write_polygon_point(
    struct generated_geometry *geometry,
    size_t point_index, // NOLINT(bugprone-easily-swappable-parameters)
    double coordinate_x,
    double coordinate_y
);
static void write_u32_le(unsigned char *destination, uint32_t value);
static void write_double_le(unsigned char *destination, double value);
static void wait_for_interrupt_request(void *argument);
static void yield_thread(void);
#ifdef _WIN32
static unsigned __stdcall run_statement_thread(void *argument);
#else
static void *run_statement_thread(void *argument);
#endif

int main(void) {
    int failures = 0;

    failures += test_scaling_operation_counts();
    failures += test_component_scaling_operation_counts();
    failures += test_injected_cancellation_and_deadline();
    failures += test_exact_work_limits();
    failures += test_resource_limits();
    failures += test_sql_deadline_and_interrupt();
    return failures == 0 ? 0 : 1;
}

static int test_scaling_operation_counts(void) {
    static const size_t vertex_counts[] = {
        minimum_scaling_vertex_count,
        16384U,
        32768U,
        maximum_scaling_vertex_count,
    };
    uint64_t previous_candidate_checks = 0U;
    int failures = 0;

    for (size_t index = 0U; index < sizeof(vertex_counts) / sizeof(vertex_counts[0]); ++index) {
        struct generated_geometry geometry = make_regular_polygon(vertex_counts[index]);
        struct mylite_spatial_work_statistics statistics = {0};
        const struct mylite_spatial_work_control control = {
            .statistics = &statistics,
        };
        char context[test_context_capacity];

        (void)snprintf(
            context,
            sizeof(context),
            "%zu-vertex validity operation count",
            vertex_counts[index]
        );
        failures += expect_direct_validity(&geometry, &control, true, 0, NULL, context);
        failures += mylite_test_expect_uint64(
            statistics.indexed_segment_count,
            vertex_counts[index],
            "indexed segment count matches vertices"
        );
        failures += mylite_test_expect_true(
            statistics.candidate_check_count <=
                statistics.indexed_segment_count * candidate_checks_per_segment_limit,
            "candidate count stays below per-segment limit"
        );
        if (index > 0U) {
            failures += mylite_test_expect_true(
                statistics.candidate_check_count * candidate_slope_tenths_scale <=
                    previous_candidate_checks * candidate_slope_tenths_limit,
                "candidate slope stays below 2.5x"
            );
        }
        previous_candidate_checks = statistics.candidate_check_count;
        free(geometry.bytes);
    }
    return failures;
}

static int test_component_scaling_operation_counts(void) {
    struct generated_geometry holes = make_many_hole_polygon(component_scaling_count);
    struct generated_geometry multipolygon = make_disjoint_multipolygon(component_scaling_count);
    struct mylite_spatial_work_statistics hole_statistics = {0};
    struct mylite_spatial_work_statistics multipolygon_statistics = {0};
    const struct mylite_spatial_work_control hole_control = {
        .statistics = &hole_statistics,
    };
    const struct mylite_spatial_work_control multipolygon_control = {
        .statistics = &multipolygon_statistics,
    };
    int failures = 0;

    failures +=
        expect_direct_validity(&holes, &hole_control, true, 0, NULL, "many disjoint polygon holes");
    failures += mylite_test_expect_true(
        hole_statistics.candidate_check_count <=
            hole_statistics.indexed_segment_count * candidate_checks_per_segment_limit,
        "disjoint hole candidate count stays bounded"
    );
    failures += expect_direct_validity(
        &multipolygon,
        &multipolygon_control,
        true,
        0,
        NULL,
        "many disjoint multipolygon members"
    );
    failures += mylite_test_expect_true(
        multipolygon_statistics.candidate_check_count <=
            multipolygon_statistics.indexed_segment_count * candidate_checks_per_segment_limit,
        "disjoint multipolygon candidate count stays bounded"
    );
    free(holes.bytes);
    free(multipolygon.bytes);
    return failures;
}

static int test_injected_cancellation_and_deadline(void) {
    static const uint64_t interrupt_checks[] = {
        1U,
        segment_count_trigger_check,
        index_build_trigger_check,
        after_sort_trigger_check,
        candidate_scan_trigger_check,
    };
    static const char *const interrupt_contexts[] = {
        "interruption before validation build",
        "interruption during segment counting",
        "interruption during index build",
        "interruption after index sort",
        "interruption during candidate scan",
    };
    struct generated_geometry geometry = make_regular_polygon(maximum_scaling_vertex_count);
    struct generated_geometry containment_geometry =
        make_large_exterior_hole_polygon(maximum_scaling_vertex_count);
    struct injected_control deadline = {
        .trigger_check = candidate_scan_trigger_check,
        .trigger_status = MYLITE_SPATIAL_WORK_DEADLINE_EXCEEDED,
    };
    const struct mylite_spatial_work_control deadline_control = {
        .check = injected_work_check,
        .context = &deadline,
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(interrupt_checks) / sizeof(interrupt_checks[0]);
         ++index) {
        struct injected_control interrupted = {
            .trigger_check = interrupt_checks[index],
            .trigger_status = MYLITE_SPATIAL_WORK_INTERRUPTED,
        };
        const struct mylite_spatial_work_control interrupted_control = {
            .check = injected_work_check,
            .context = &interrupted,
        };

        failures += expect_direct_validity(
            &geometry,
            &interrupted_control,
            false,
            mysql_error_query_interrupted,
            "70100",
            interrupt_contexts[index]
        );
        failures += mylite_test_expect_uint64(
            interrupted.check_count,
            interrupt_checks[index],
            "interruption occurs at requested control check"
        );
    }
    failures += expect_direct_validity(
        &geometry,
        &deadline_control,
        false,
        mysql_error_max_execution_time_exceeded,
        "HY000",
        "injected validation deadline"
    );
    {
        struct mylite_spatial_work_statistics statistics = {0};
        const struct mylite_spatial_work_control statistics_control = {
            .statistics = &statistics,
        };

        failures += expect_direct_validity(
            &containment_geometry,
            &statistics_control,
            true,
            0,
            NULL,
            "collect containment control checks"
        );
        failures += mylite_test_expect_true(
            statistics.control_check_count > 1U,
            "containment validation has an injectable control check"
        );
        if (statistics.control_check_count > 1U) {
            struct injected_control containment = {
                .trigger_check = statistics.control_check_count - 1U,
                .trigger_status = MYLITE_SPATIAL_WORK_INTERRUPTED,
            };
            const struct mylite_spatial_work_control containment_control = {
                .check = injected_work_check,
                .context = &containment,
            };

            failures += expect_direct_validity(
                &containment_geometry,
                &containment_control,
                false,
                mysql_error_query_interrupted,
                "70100",
                "injected interruption during containment"
            );
        }
    }
    free(geometry.bytes);
    free(containment_geometry.bytes);
    return failures;
}

static int test_exact_work_limits(void) {
    struct generated_geometry geometry = make_regular_polygon(minimum_scaling_vertex_count);
    struct mylite_spatial_work_statistics baseline_statistics = {0};
    const struct mylite_spatial_work_control baseline_control = {
        .statistics = &baseline_statistics,
    };
    struct mylite_spatial_work_limits limits = {0};
    const struct mylite_spatial_work_control control = {
        .limits = &limits,
    };
    int failures = expect_direct_validity(
        &geometry,
        &baseline_control,
        true,
        0,
        NULL,
        "collect exact work-limit boundaries"
    );

    limits.maximum_segment_count = baseline_statistics.indexed_segment_count;
    failures +=
        expect_direct_validity(&geometry, &control, true, 0, NULL, "exact injected segment limit");
    --limits.maximum_segment_count;
    failures += expect_direct_validity(
        &geometry,
        &control,
        false,
        mysql_error_out_of_resources,
        "HY000",
        "one below required segment limit"
    );

    limits = (struct mylite_spatial_work_limits){
        .maximum_index_bytes = baseline_statistics.peak_index_bytes,
    };
    failures += expect_direct_validity(
        &geometry,
        &control,
        true,
        0,
        NULL,
        "exact injected index-memory limit"
    );
    --limits.maximum_index_bytes;
    failures += expect_direct_validity(
        &geometry,
        &control,
        false,
        mysql_error_out_of_resources,
        "HY000",
        "one below required index-memory limit"
    );

    limits = (struct mylite_spatial_work_limits){
        .maximum_candidate_count = baseline_statistics.candidate_check_count,
    };
    failures += expect_direct_validity(
        &geometry,
        &control,
        true,
        0,
        NULL,
        "exact injected candidate limit"
    );
    --limits.maximum_candidate_count;
    failures += expect_direct_validity(
        &geometry,
        &control,
        false,
        mysql_error_out_of_resources,
        "HY000",
        "one below required candidate limit"
    );
    free(geometry.bytes);
    return failures;
}

static int test_resource_limits(void) {
    struct generated_geometry candidate_limit = make_comb_polygon(candidate_budget_comb_rows);
    struct generated_geometry segment_limit = {0};
    int failures = 0;

    failures += expect_direct_validity(
        &candidate_limit,
        NULL,
        false,
        mysql_error_out_of_resources,
        "HY000",
        "pathological candidate budget"
    );
    free(candidate_limit.bytes);

    segment_limit = make_regular_polygon(maximum_validation_segment_count);
    failures += expect_direct_validity(
        &segment_limit,
        NULL,
        true,
        0,
        NULL,
        "exact production validation segment limit"
    );
    free(segment_limit.bytes);

    segment_limit = make_regular_polygon(maximum_validation_segment_count + 1U);
    failures += expect_direct_validity(
        &segment_limit,
        NULL,
        false,
        mysql_error_out_of_resources,
        "HY000",
        "validation segment limit"
    );
    free(segment_limit.bytes);
    return failures;
}

static int test_sql_deadline_and_interrupt(void) {
    struct generated_geometry deadline_geometry =
        make_regular_polygon(maximum_scaling_vertex_count);
    struct generated_geometry interrupt_geometry = make_comb_polygon(interrupt_comb_rows);
    mylite_db *database = NULL;
    mylite_stmt *statement = NULL;
    struct interrupt_thread_context thread_context = {0};
    int failures = mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open spatial cancellation database"
    );

    failures += execute_statement(database, "SET max_execution_time = 1", "set spatial deadline");
    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT ST_IsValid(?)",
            strlen("SELECT ST_IsValid(?)"),
            &statement
        ),
        MYLITE_OK,
        "prepare deadline validation"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_blob(statement, 0U, deadline_geometry.bytes, deadline_geometry.byte_count),
        MYLITE_OK,
        "bind deadline geometry"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_step(statement),
        MYLITE_ERROR,
        "deadline validation returns error"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_errcode(statement),
        mysql_error_max_execution_time_exceeded,
        "deadline validation error code"
    );
    failures += mylite_test_expect_text(
        mylite_stmt_sqlstate(statement),
        "HY000",
        "deadline validation SQLSTATE"
    );
    failures += mylite_test_expect_true(
        strstr(mylite_stmt_errmsg(statement), "maximum statement execution time exceeded") != NULL,
        "deadline validation message"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(statement),
        MYLITE_OK,
        "finalize deadline statement"
    );
    statement = NULL;
    failures +=
        execute_statement(database, "SET max_execution_time = 0", "disable spatial deadline");
    failures += expect_recovery_query(database, "recovery after deadline");

    failures += execute_statement(
        database,
        "CREATE DATABASE spatial_deadline",
        "create spatial deadline database"
    );
    failures +=
        execute_statement(database, "USE spatial_deadline", "select spatial deadline database");
    failures += execute_statement(
        database,
        "CREATE TABLE spatial_deadline_values(g GEOMETRY)",
        "create row-backed spatial deadline table"
    );
    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "INSERT INTO spatial_deadline_values VALUES (?)",
            strlen("INSERT INTO spatial_deadline_values VALUES (?)"),
            &statement
        ),
        MYLITE_OK,
        "prepare row-backed deadline geometry"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_blob(statement, 0U, deadline_geometry.bytes, deadline_geometry.byte_count),
        MYLITE_OK,
        "bind row-backed deadline geometry"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_step(statement),
        MYLITE_DONE,
        "insert row-backed deadline geometry"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(statement),
        MYLITE_OK,
        "finalize row-backed deadline insert"
    );
    statement = NULL;
    failures += execute_statement(
        database,
        "SET max_execution_time = 1",
        "set row-backed spatial deadline"
    );
    {
        mylite_result *result = NULL;

        failures += mylite_test_expect_int(
            mylite_execute(
                database,
                "SELECT ST_IsValid(g) FROM spatial_deadline_values",
                strlen("SELECT ST_IsValid(g) FROM spatial_deadline_values"),
                &result
            ),
            MYLITE_ERROR,
            "row-backed deadline validation returns error"
        );
        failures += mylite_test_expect_int(
            mylite_errcode(database),
            mysql_error_max_execution_time_exceeded,
            "row-backed deadline validation error code"
        );
        failures += mylite_test_expect_text(
            mylite_sqlstate(database),
            "HY000",
            "row-backed deadline validation SQLSTATE"
        );
        mylite_result_free(result);
    }
    failures += execute_statement(
        database,
        "SET max_execution_time = 0",
        "disable row-backed spatial deadline"
    );
    failures += expect_recovery_query(database, "recovery after row-backed deadline");

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT ST_IsValid(?)",
            strlen("SELECT ST_IsValid(?)"),
            &statement
        ),
        MYLITE_OK,
        "prepare interrupt validation"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_blob(
            statement,
            0U,
            interrupt_geometry.bytes,
            interrupt_geometry.byte_count
        ),
        MYLITE_OK,
        "bind interrupt geometry"
    );
    thread_context.statement = statement;
    atomic_init(&thread_context.work_entered, false);
    atomic_init(&thread_context.release_work, false);
    mylite_spatial_set_sql_work_test_hook(wait_for_interrupt_request, &thread_context);
#ifdef _WIN32
    {
        uintptr_t thread =
            _beginthreadex(NULL, 0U, run_statement_thread, &thread_context, 0U, NULL);

        failures += mylite_test_expect_true(thread != 0U, "create interrupt thread");
        while (thread != 0U &&
               !atomic_load_explicit(&thread_context.work_entered, memory_order_acquire)) {
            yield_thread();
        }
        if (thread != 0U) {
            mylite_interrupt(database);
            atomic_store_explicit(&thread_context.release_work, true, memory_order_release);
        }
        if (thread != 0U) {
            failures += mylite_test_expect_int(
                (int)WaitForSingleObject((HANDLE)thread, INFINITE),
                (int)WAIT_OBJECT_0,
                "join interrupt thread"
            );
            (void)CloseHandle((HANDLE)thread);
        }
    }
#else
    {
        pthread_t thread;
        bool thread_created =
            pthread_create(&thread, NULL, run_statement_thread, &thread_context) == 0;

        failures += mylite_test_expect_true(thread_created, "create interrupt thread");
        while (thread_created &&
               !atomic_load_explicit(&thread_context.work_entered, memory_order_acquire)) {
            yield_thread();
        }
        if (thread_created) {
            mylite_interrupt(database);
            atomic_store_explicit(&thread_context.release_work, true, memory_order_release);
        }
        if (thread_created) {
            failures +=
                mylite_test_expect_int(pthread_join(thread, NULL), 0, "join interrupt thread");
        }
    }
#endif
    mylite_spatial_set_sql_work_test_hook(NULL, NULL);
    failures += mylite_test_expect_int(
        thread_context.step_rc,
        MYLITE_ERROR,
        "interrupted validation returns error"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_errcode(statement),
        mysql_error_query_interrupted,
        "interrupted validation error code"
    );
    failures += mylite_test_expect_text(
        mylite_stmt_sqlstate(statement),
        "70100",
        "interrupted validation SQLSTATE"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(statement),
        MYLITE_OK,
        "finalize interrupted statement"
    );
    failures += expect_recovery_query(database, "recovery after interrupt");

    mylite_close(database);
    free(deadline_geometry.bytes);
    free(interrupt_geometry.bytes);
    return failures;
}

static int expect_direct_validity(
    const struct generated_geometry *geometry,
    const struct mylite_spatial_work_control *control,
    bool should_succeed,
    int expected_error,
    const char *expected_sqlstate, // NOLINT(bugprone-easily-swappable-parameters)
    const char *context
) {
    const struct mylite_spatial_argument argument = {
        .bytes = geometry == NULL ? NULL : geometry->bytes,
        .byte_count = geometry == NULL ? 0U : geometry->byte_count,
    };
    struct mylite_spatial_result result = {0};
    struct mylite_spatial_error error = {0};
    int rc = mylite_spatial_evaluate_with_control(
        MYLITE_SPATIAL_FUNCTION_ST_ISVALID,
        &argument,
        1U,
        &result,
        &error,
        control
    );
    int failures = 0;

    if (should_succeed) {
        failures += mylite_test_expect_int(rc, 0, context);
        failures += mylite_test_expect_int(
            (int)result.kind,
            MYLITE_SPATIAL_RESULT_INTEGER,
            "direct validity result kind"
        );
        failures += mylite_test_expect_int64(result.integer, 1, "direct validity result");
    } else {
        failures += mylite_test_expect_int(rc, -1, context);
        failures +=
            mylite_test_expect_int(error.code, expected_error, "direct validity error code");
        failures +=
            mylite_test_expect_text(error.sqlstate, expected_sqlstate, "direct validity SQLSTATE");
        failures += mylite_test_expect_int(
            (int)result.kind,
            MYLITE_SPATIAL_RESULT_NULL,
            "failed direct validity publishes no result"
        );
    }
    mylite_spatial_result_deinit(&result);
    return failures;
}

static enum mylite_spatial_work_status injected_work_check(void *context) {
    struct injected_control *control = context;

    if (control == NULL) {
        return MYLITE_SPATIAL_WORK_CONTINUE;
    }
    ++control->check_count;
    if (control->check_count >= control->trigger_check) {
        return control->trigger_status;
    }
    return MYLITE_SPATIAL_WORK_CONTINUE;
}

static int execute_statement(
    mylite_db *database,
    const char *sql, // NOLINT(bugprone-easily-swappable-parameters)
    const char *context
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = mylite_test_expect_int(rc, MYLITE_OK, context);

    mylite_result_free(result);
    return failures;
}

static int expect_recovery_query(mylite_db *database, const char *context) {
    mylite_result *result = NULL;
    int failures = mylite_test_expect_int(
        mylite_execute(database, "SELECT 1", strlen("SELECT 1"), &result),
        MYLITE_OK,
        context
    );

    if (result != NULL) {
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, 0U, 0U),
            "1",
            "recovery query value"
        );
    }
    mylite_result_free(result);
    return failures;
}

static struct generated_geometry make_regular_polygon(size_t vertex_count) {
    struct generated_geometry geometry = allocate_polygon(vertex_count + 1U);

    if (geometry.bytes == NULL) {
        return geometry;
    }
    for (size_t index = 0U; index < vertex_count; ++index) {
        double angle = full_circle_radians * (double)index / (double)vertex_count;

        write_polygon_point(&geometry, index, cos(angle), sin(angle));
    }
    write_polygon_point(&geometry, vertex_count, 1.0, 0.0);
    return geometry;
}

static struct generated_geometry make_comb_polygon(size_t row_count) {
    size_t point_count = row_count > (SIZE_MAX - 4U) / 2U ? 0U : (row_count * 2U) + 4U;
    struct generated_geometry geometry = allocate_polygon(point_count);
    size_t point_index = 0U;
    double current_x = 0.0;

    if (geometry.bytes == NULL || row_count == 0U || row_count % 2U == 0U) {
        free(geometry.bytes);
        return (struct generated_geometry){0};
    }
    write_polygon_point(&geometry, point_index++, current_x, 0.0);
    for (size_t row = 0U; row < row_count; ++row) {
        if (row > 0U) {
            write_polygon_point(&geometry, point_index++, current_x, (double)row);
        }
        current_x = current_x == 0.0 ? 1.0 : 0.0;
        write_polygon_point(&geometry, point_index++, current_x, (double)row);
    }
    write_polygon_point(&geometry, point_index++, comb_exterior_x, (double)(row_count - 1U));
    write_polygon_point(&geometry, point_index++, comb_exterior_x, -1.0);
    write_polygon_point(&geometry, point_index++, 0.0, -1.0);
    write_polygon_point(&geometry, point_index, 0.0, 0.0);
    return geometry;
}

static struct generated_geometry make_large_exterior_hole_polygon(size_t exterior_vertex_count) {
    struct generated_geometry geometry = {0};
    size_t exterior_point_count = exterior_vertex_count + 1U;
    size_t fixed_byte_count = internal_srid_size + wkb_polygon_header_size +
                              (polygon_with_hole_ring_count * wkb_ring_header_size) +
                              (square_ring_point_count * wkb_point_size);
    size_t exterior_ring_offset = internal_srid_size + wkb_polygon_header_size;
    size_t hole_ring_offset = 0U;

    if (exterior_vertex_count < 3U || exterior_point_count == 0U ||
        exterior_point_count > UINT32_MAX ||
        exterior_point_count > (SIZE_MAX - fixed_byte_count) / wkb_point_size) {
        return geometry;
    }
    geometry.byte_count = fixed_byte_count + (exterior_point_count * wkb_point_size);
    geometry.bytes = (unsigned char *)calloc(geometry.byte_count, 1U);
    if (geometry.bytes == NULL) {
        geometry.byte_count = 0U;
        return geometry;
    }
    geometry.bytes[wkb_byte_order_offset] = wkb_little_endian;
    write_u32_le(&geometry.bytes[wkb_type_offset], wkb_polygon_type);
    write_u32_le(&geometry.bytes[wkb_ring_count_offset], polygon_with_hole_ring_count);
    write_u32_le(&geometry.bytes[exterior_ring_offset], (uint32_t)exterior_point_count);
    for (size_t index = 0U; index < exterior_vertex_count; ++index) {
        double angle = full_circle_radians * (double)index / (double)exterior_vertex_count;
        size_t point_offset =
            exterior_ring_offset + wkb_ring_header_size + (index * wkb_point_size);

        write_double_le(&geometry.bytes[point_offset], large_exterior_radius * cos(angle));
        write_double_le(
            &geometry.bytes[point_offset + sizeof(double)],
            large_exterior_radius * sin(angle)
        );
    }
    {
        size_t point_offset =
            exterior_ring_offset + wkb_ring_header_size + (exterior_vertex_count * wkb_point_size);

        write_double_le(&geometry.bytes[point_offset], large_exterior_radius);
        write_double_le(&geometry.bytes[point_offset + sizeof(double)], 0.0);
    }
    hole_ring_offset =
        exterior_ring_offset + wkb_ring_header_size + (exterior_point_count * wkb_point_size);
    write_square_ring(geometry.bytes, hole_ring_offset, -1.0, -1.0, 1.0, 1.0);
    return geometry;
}

static struct generated_geometry make_many_hole_polygon(size_t hole_count) {
    struct generated_geometry geometry = {0};
    size_t ring_count = hole_count + 1U;
    size_t ring_size = wkb_ring_header_size + (square_ring_point_count * wkb_point_size);
    size_t byte_count = 0U;

    if (ring_count == 0U || ring_count > UINT32_MAX ||
        ring_count > (SIZE_MAX - internal_srid_size - wkb_polygon_header_size) / ring_size) {
        return geometry;
    }
    byte_count = internal_srid_size + wkb_polygon_header_size + (ring_count * ring_size);
    geometry.bytes = (unsigned char *)calloc(byte_count, 1U);
    if (geometry.bytes == NULL) {
        return geometry;
    }
    geometry.byte_count = byte_count;
    geometry.bytes[wkb_byte_order_offset] = wkb_little_endian;
    write_u32_le(&geometry.bytes[wkb_type_offset], wkb_polygon_type);
    write_u32_le(&geometry.bytes[wkb_ring_count_offset], (uint32_t)ring_count);
    write_square_ring(
        geometry.bytes,
        internal_srid_size + wkb_polygon_header_size,
        -component_exterior_margin,
        -component_exterior_margin,
        ((double)hole_count * component_spacing) + component_exterior_margin,
        component_exterior_margin
    );
    for (size_t index = 0U; index < hole_count; ++index) {
        double min_x = (double)index * component_spacing;

        write_square_ring(
            geometry.bytes,
            internal_srid_size + wkb_polygon_header_size + ((index + 1U) * ring_size),
            min_x,
            -component_half_size,
            min_x + component_half_size,
            component_half_size
        );
    }
    return geometry;
}

static struct generated_geometry make_disjoint_multipolygon(size_t polygon_count) {
    struct generated_geometry geometry = {0};
    size_t polygon_size =
        wkb_polygon_header_size + wkb_ring_header_size + (square_ring_point_count * wkb_point_size);
    size_t byte_count = 0U;

    if (polygon_count == 0U || polygon_count > UINT32_MAX ||
        polygon_count > (SIZE_MAX - internal_srid_size - wkb_header_size) / polygon_size) {
        return geometry;
    }
    byte_count = internal_srid_size + wkb_header_size + (polygon_count * polygon_size);
    geometry.bytes = (unsigned char *)calloc(byte_count, 1U);
    if (geometry.bytes == NULL) {
        return geometry;
    }
    geometry.byte_count = byte_count;
    geometry.bytes[wkb_byte_order_offset] = wkb_little_endian;
    write_u32_le(&geometry.bytes[wkb_type_offset], wkb_multipolygon_type);
    write_u32_le(&geometry.bytes[wkb_ring_count_offset], (uint32_t)polygon_count);
    for (size_t index = 0U; index < polygon_count; ++index) {
        size_t polygon_offset = internal_srid_size + wkb_header_size + (index * polygon_size);
        double min_x = (double)index * component_spacing;

        geometry.bytes[polygon_offset] = wkb_little_endian;
        write_u32_le(&geometry.bytes[polygon_offset + 1U], wkb_polygon_type);
        write_u32_le(&geometry.bytes[polygon_offset + wkb_nested_ring_count_offset], 1U);
        write_square_ring(
            geometry.bytes,
            polygon_offset + wkb_polygon_header_size,
            min_x,
            0.0,
            min_x + 1.0,
            1.0
        );
    }
    return geometry;
}

static struct generated_geometry allocate_polygon(size_t point_count) {
    struct generated_geometry geometry = {0};
    size_t byte_count = 0U;

    if (point_count < 4U || point_count > UINT32_MAX ||
        point_count >
            (SIZE_MAX - internal_srid_size - wkb_polygon_header_size - wkb_ring_header_size) /
                wkb_point_size) {
        return geometry;
    }
    byte_count = internal_srid_size + wkb_polygon_header_size + wkb_ring_header_size +
                 (point_count * wkb_point_size);
    geometry.bytes = (unsigned char *)calloc(byte_count, 1U);
    if (geometry.bytes == NULL) {
        return geometry;
    }
    geometry.byte_count = byte_count;
    geometry.bytes[wkb_byte_order_offset] = wkb_little_endian;
    write_u32_le(&geometry.bytes[wkb_type_offset], wkb_polygon_type);
    write_u32_le(&geometry.bytes[wkb_ring_count_offset], 1U);
    write_u32_le(&geometry.bytes[wkb_point_count_offset], (uint32_t)point_count);
    return geometry;
}

static void write_square_ring(
    unsigned char *bytes,
    size_t ring_offset, // NOLINT(bugprone-easily-swappable-parameters)
    double min_x,       // NOLINT(bugprone-easily-swappable-parameters)
    double min_y,       // NOLINT(bugprone-easily-swappable-parameters)
    double max_x,
    double max_y
) {
    static const double normalized_coordinates[square_ring_point_count][2U] = {
        {0.0, 0.0},
        {1.0, 0.0},
        {1.0, 1.0},
        {0.0, 1.0},
        {0.0, 0.0},
    };

    write_u32_le(&bytes[ring_offset], square_ring_point_count);
    for (size_t index = 0U; index < square_ring_point_count; ++index) {
        double coordinate_x = normalized_coordinates[index][0] == 0.0 ? min_x : max_x;
        double coordinate_y = normalized_coordinates[index][1] == 0.0 ? min_y : max_y;
        size_t point_offset = ring_offset + wkb_ring_header_size + (index * wkb_point_size);

        write_double_le(&bytes[point_offset], coordinate_x);
        write_double_le(&bytes[point_offset + sizeof(double)], coordinate_y);
    }
}

static void write_polygon_point(
    struct generated_geometry *geometry,
    size_t point_index, // NOLINT(bugprone-easily-swappable-parameters)
    double coordinate_x,
    double coordinate_y
) {
    size_t offset = wkb_points_offset + (point_index * wkb_point_size);

    write_double_le(&geometry->bytes[offset], coordinate_x);
    write_double_le(&geometry->bytes[offset + sizeof(double)], coordinate_y);
}

static void write_u32_le(unsigned char *destination, uint32_t value) {
    for (size_t index = 0U; index < sizeof(value); ++index) {
        destination[index] =
            (unsigned char)((value >> (index * spatial_byte_bit_count)) & spatial_byte_mask);
    }
}

static void write_double_le(unsigned char *destination, double value) {
    uint64_t bits = 0U;

    memcpy(&bits, &value, sizeof(bits));
    for (size_t index = 0U; index < sizeof(bits); ++index) {
        destination[index] =
            (unsigned char)((bits >> (index * spatial_byte_bit_count)) & spatial_byte_mask);
    }
}

static void wait_for_interrupt_request(void *argument) {
    struct interrupt_thread_context *context = argument;

    atomic_store_explicit(&context->work_entered, true, memory_order_release);
    while (!atomic_load_explicit(&context->release_work, memory_order_acquire)) {
        yield_thread();
    }
}

static void yield_thread(void) {
#ifdef _WIN32
    (void)SwitchToThread();
#else
    (void)sched_yield();
#endif
}

#ifdef _WIN32
static unsigned __stdcall run_statement_thread(void *argument) {
#else
static void *run_statement_thread(void *argument) {
#endif
    struct interrupt_thread_context *context = argument;

    context->step_rc = mylite_stmt_step(context->statement);
#ifdef _WIN32
    return 0U;
#else
    return NULL;
#endif
}
