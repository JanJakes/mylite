#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "runtime/mylite_diagnostics.h"
#include "runtime/mylite_statement_context.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int test_statement_context_lifecycle_and_diagnostic_boundaries(void);
static int expect_wrapper_state(
    enum mylite_statement_wrapper_transaction_state actual,
    enum mylite_statement_wrapper_transaction_state expected,
    const char *context
);
static int expect_backend_status(
    enum mylite_statement_backend_status actual,
    enum mylite_statement_backend_status expected,
    const char *context
);
static int expect_bool(bool actual, bool expected, const char *context);

int main(void) {
    return test_statement_context_lifecycle_and_diagnostic_boundaries() == 0 ? 0 : 1;
}

static int test_statement_context_lifecycle_and_diagnostic_boundaries(void) {
    enum {
        expected_affected_rows = 12,
        expected_previous_row_count = 7,
        expected_insert_id = 42,
        synthetic_display_length = 9,
        synthetic_warning_code = 1000,
    };

    static const char *first_sql = "synthetic first statement";
    static const char *second_sql = "synthetic second statement";

    mylite_db *database = NULL;
    struct mylite_diagnostics *diagnostics = NULL;
    struct mylite_statement_context context = {0};
    struct mylite_result_metadata *metadata = NULL;
    struct mylite_result_column_descriptor descriptor = {
        .label = "synthetic",
        .schema_name = "",
        .table_name = "",
        .origin_schema_name = "",
        .origin_table_name = "",
        .origin_column_name = "",
        .logical_type = MYLITE_RESULT_LOGICAL_TYPE_STRING,
        .flags = 0U,
        .charset_id = 0U,
        .collation_id = 0U,
        .display_length = synthetic_display_length,
        .decimals = 0U,
        .nullable = true,
    };
    int failures = 0;

    mylite_statement_context_deinit(&context);

    failures += mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open handle");
    diagnostics = mylite_connection_diagnostics(database);

    mylite_diagnostics_set_error(diagnostics, MYLITE_ERROR, "HY001", "previous error");
    failures += mylite_test_expect_int(
        mylite_diagnostics_append_warning(
            diagnostics,
            synthetic_warning_code,
            "01000",
            "previous warning"
        ),
        MYLITE_OK,
        "append previous warning"
    );

    failures += mylite_test_expect_int(
        mylite_statement_context_begin(&context, database, first_sql, strlen(first_sql)),
        MYLITE_OK,
        "begin first statement"
    );
    failures += expect_bool(
        mylite_statement_context_is_active(&context),
        true,
        "first statement is active"
    );
    failures += mylite_test_expect_text(
        mylite_statement_context_sql(&context),
        first_sql,
        "first SQL text"
    );
    failures += mylite_test_expect_size(
        mylite_statement_context_sql_size(&context),
        strlen(first_sql),
        "first SQL size"
    );
    failures += mylite_test_expect_true(
        mylite_statement_context_time(&context) != (time_t)0,
        "statement time is captured"
    );
    failures += mylite_test_expect_int(mylite_errcode(database), MYLITE_OK, "begin resets errcode");
    failures += mylite_test_expect_size(
        mylite_diagnostics_warning_count(diagnostics),
        0U,
        "begin resets warnings"
    );

    metadata = mylite_statement_context_result_metadata(&context);
    failures += mylite_test_expect_int(
        mylite_result_metadata_append(metadata, &descriptor),
        MYLITE_OK,
        "append statement metadata"
    );
    failures += mylite_test_expect_size(
        mylite_result_metadata_column_count(metadata),
        1U,
        "statement metadata count"
    );

    mylite_statement_context_set_affected_rows(&context, expected_affected_rows);
    mylite_statement_context_set_previous_row_count(&context, expected_previous_row_count);
    mylite_statement_context_set_first_insert_id(&context, expected_insert_id);
    mylite_statement_context_set_wrapper_transaction_state(
        &context,
        MYLITE_STATEMENT_WRAPPER_TRANSACTION_ACTIVE
    );
    failures += mylite_test_expect_int64(
        mylite_statement_context_affected_rows(&context),
        expected_affected_rows,
        "affected rows"
    );
    failures += mylite_test_expect_int64(
        mylite_statement_context_previous_row_count(&context),
        expected_previous_row_count,
        "previous row count"
    );
    failures += expect_bool(
        mylite_statement_context_has_first_insert_id(&context),
        true,
        "first insert id is set"
    );
    failures += mylite_test_expect_uint64(
        mylite_statement_context_first_insert_id(&context),
        expected_insert_id,
        "first insert id"
    );
    failures += expect_wrapper_state(
        mylite_statement_context_wrapper_transaction_state(&context),
        MYLITE_STATEMENT_WRAPPER_TRANSACTION_ACTIVE,
        "wrapper transaction state"
    );
    failures += expect_backend_status(
        mylite_statement_context_backend_status(&context),
        MYLITE_STATEMENT_BACKEND_RUNNING,
        "backend running"
    );

    mylite_diagnostics_set_error(diagnostics, MYLITE_ERROR, "HY001", "completion error");
    failures += mylite_test_expect_int(
        mylite_diagnostics_append_warning(
            diagnostics,
            synthetic_warning_code,
            "01000",
            "completion warning"
        ),
        MYLITE_OK,
        "append completion warning"
    );
    failures += mylite_test_expect_int(
        mylite_statement_context_end(&context, MYLITE_OK),
        MYLITE_OK,
        "end first statement"
    );
    failures += expect_bool(
        mylite_statement_context_is_active(&context),
        false,
        "first statement is inactive"
    );
    failures += expect_backend_status(
        mylite_statement_context_backend_status(&context),
        MYLITE_STATEMENT_BACKEND_DONE,
        "backend done"
    );
    failures += mylite_test_expect_text(
        mylite_errmsg(database),
        "completion error",
        "completion diagnostics remain readable"
    );
    failures += mylite_test_expect_size(
        mylite_diagnostics_warning_count(diagnostics),
        1U,
        "completion warning remains readable"
    );

    failures += mylite_test_expect_int(
        mylite_statement_context_begin(&context, database, second_sql, strlen(second_sql)),
        MYLITE_OK,
        "begin second statement"
    );
    failures += mylite_test_expect_text(
        mylite_statement_context_sql(&context),
        second_sql,
        "second SQL text"
    );
    failures +=
        mylite_test_expect_int(mylite_errcode(database), MYLITE_OK, "second begin resets errcode");
    failures += mylite_test_expect_size(
        mylite_diagnostics_warning_count(diagnostics),
        0U,
        "second begin resets warnings"
    );
    failures += expect_bool(
        mylite_statement_context_has_first_insert_id(&context),
        false,
        "second statement insert id reset"
    );
    failures += mylite_test_expect_size(
        mylite_result_metadata_column_count(mylite_statement_context_result_metadata(&context)),
        0U,
        "second statement metadata reset"
    );

    failures += mylite_test_expect_int(
        mylite_statement_context_end(&context, MYLITE_ERROR),
        MYLITE_ERROR,
        "end failed statement"
    );
    failures += expect_backend_status(
        mylite_statement_context_backend_status(&context),
        MYLITE_STATEMENT_BACKEND_FAILED,
        "backend failed"
    );

    mylite_statement_context_deinit(&context);
    mylite_close(database);

    return failures;
}

static int expect_wrapper_state(
    enum mylite_statement_wrapper_transaction_state actual,
    enum mylite_statement_wrapper_transaction_state expected,
    const char *context
) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, (int)expected, (int)actual);
        return 1;
    }

    return 0;
}

static int expect_backend_status(
    enum mylite_statement_backend_status actual,
    enum mylite_statement_backend_status expected,
    const char *context
) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, (int)expected, (int)actual);
        return 1;
    }

    return 0;
}

static int expect_bool(bool actual, bool expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, (int)expected, (int)actual);
        return 1;
    }

    return 0;
}
