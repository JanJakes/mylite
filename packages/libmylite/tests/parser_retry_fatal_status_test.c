#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_test_allocator.h"
#include "sql/mylite_parser.h"
#include "sql/mylite_parser_driver.h"
#include "sql/mylite_parser_placeholders.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    parser_allocation_sweep_limit = 2048,
    parser_growth_sweep_depth = 128,
};

struct parser_retry_case {
    const char *sql;
    enum mylite_sql_parse_status expected_status;
    size_t minimum_callback_count;
    size_t minimum_handled_count;
    const char *context;
};

static int test_direct_parser_allocation_failures(void);
static int expect_parser_allocation_sweep(const struct parser_retry_case *test_case);
static int expect_typed_retry_allocation_sweep(
    const char *sql,
    enum mylite_sql_parser_retry_kind kind,
    const char *context
);
static int test_growable_stack_allocation_failures(void);
static int expect_direct_growable_stack_allocation_sweep(const char *sql, size_t sql_length);
static int expect_runtime_growable_stack_allocation_sweep(const char *sql, size_t sql_length);
static char *make_nested_if_query(size_t depth, size_t *out_length);
static int test_runtime_parser_allocation_failures(void);

int main(void) {
    int failures = 0;

    failures += test_direct_parser_allocation_failures();
    failures += test_growable_stack_allocation_failures();
    failures += test_runtime_parser_allocation_failures();
    mylite_test_allocator_clear();
    return failures == 0 ? 0 : 1;
}

static int test_direct_parser_allocation_failures(void) {
    static const struct parser_retry_case cases[] = {
        {
            .sql = "SELECT FROM DUAL",
            .expected_status = MYLITE_SQL_PARSE_SYNTAX_ERROR,
            .minimum_callback_count = 6U,
            .context = "unhandled syntax retry context",
        },
        {
            .sql = "SELECT FROM (DUAL)",
            .expected_status = MYLITE_SQL_PARSE_OK,
            .minimum_callback_count = 6U,
            .minimum_handled_count = 1U,
            .context = "parenthesized syntax retry context",
        },
        {
            .sql = "SELECT SQL_BIG_RESULT DISTINCT 1",
            .expected_status = MYLITE_SQL_PARSE_OK,
            .minimum_callback_count = 2U,
            .minimum_handled_count = 1U,
            .context = "result option reorder retry",
        },
        {
            .sql = "SELECT (1, 2)",
            .expected_status = MYLITE_SQL_PARSE_OK,
            .minimum_callback_count = 3U,
            .minimum_handled_count = 1U,
            .context = "parenthesized row constructor retry",
        },
        {
            .sql = "SELECT * FROM t WHERE (a,b) = (1,2)",
            .expected_status = MYLITE_SQL_PARSE_OK,
            .minimum_callback_count = 1U,
            .minimum_handled_count = 1U,
            .context = "row constructor predicate retry",
        },
        {
            .sql = "SELECT (1,2) = (1,2)",
            .expected_status = MYLITE_SQL_PARSE_OK,
            .minimum_callback_count = 1U,
            .minimum_handled_count = 1U,
            .context = "unsupported utility retry",
        },
        {
            .sql = "SELECT 1 WHERE (1 + 1) > 1",
            .expected_status = MYLITE_SQL_PARSE_OK,
            .minimum_callback_count = 1U,
            .minimum_handled_count = 1U,
            .context = "parenthesized arithmetic predicate retry",
        },
        {
            .sql = "SELECT 1 FOR UPDATE FOR SHARE",
            .expected_status = MYLITE_SQL_PARSE_OK,
            .minimum_callback_count = 5U,
            .minimum_handled_count = 1U,
            .context = "repeated locking retry",
        },
        {
            .sql = "CREATE INDEX idx TYPE BTREE ON t (id)",
            .expected_status = MYLITE_SQL_PARSE_OK,
            .context = "primary legacy create index type",
        },
        {
            .sql = "ANALYZE TABLES t1",
            .expected_status = MYLITE_SQL_PARSE_OK,
            .minimum_callback_count = 6U,
            .minimum_handled_count = 1U,
            .context = "scanned placeholder retry",
        },
        {
            .sql = "ALTER TABLE t RENAME TO u, ALGORITHM=INPLACE",
            .expected_status = MYLITE_SQL_PARSE_OK,
            .minimum_callback_count = 6U,
            .minimum_handled_count = 1U,
            .context = "alter option tail retry",
        },
        {
            .sql = "CREATE TABLE t (id INT) PARTITION BY HASH(id) PARTITIONS 2",
            .expected_status = MYLITE_SQL_PARSE_OK,
            .minimum_callback_count = 6U,
            .minimum_handled_count = 1U,
            .context = "partition placeholder retry",
        },
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        failures += expect_parser_allocation_sweep(&cases[index]);
    }
    failures += expect_typed_retry_allocation_sweep(
        "SELECT 1 FOR UPDATE FOR SHARE",
        MYLITE_SQL_PARSER_RETRY_REPEATED_SELECT_LOCKING,
        "typed repeated locking retry"
    );
    return failures;
}

static int expect_parser_allocation_sweep(const struct parser_retry_case *test_case) {
    int failures = 0;
    bool completed_sweep = false;

    for (size_t allocation_index = 0U; allocation_index < parser_allocation_sweep_limit;
         ++allocation_index) {
        struct mylite_sql_parse_result result = {0};
        enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
        bool allocation_failed = false;

        mylite_test_allocator_fail_after(allocation_index);
        status = mylite_sql_parse(
            (struct mylite_sql_parse_config){
                .input = test_case->sql,
                .length = strlen(test_case->sql),
            },
            &result
        );
        allocation_failed = mylite_test_allocator_was_triggered();
        mylite_test_allocator_clear();

        if (!allocation_failed) {
            failures +=
                mylite_test_expect_int(status, test_case->expected_status, test_case->context);
            failures += mylite_test_expect_int(
                result.status,
                test_case->expected_status,
                test_case->context
            );
            failures += mylite_test_expect_true(
                result.retry_callback_count >= test_case->minimum_callback_count,
                test_case->context
            );
            failures += mylite_test_expect_true(
                result.retry_handled_count >= test_case->minimum_handled_count,
                test_case->context
            );
            completed_sweep = true;
            mylite_sql_parse_result_deinit(&result);
            break;
        }

        if (status != MYLITE_SQL_PARSE_NOMEM || result.status != MYLITE_SQL_PARSE_NOMEM) {
            fprintf(
                stderr,
                "%s allocation %zu: expected NOMEM/NOMEM, got %s/%s\n",
                test_case->context,
                allocation_index,
                mylite_sql_parse_status_name(status),
                mylite_sql_parse_status_name(result.status)
            );
            failures = 1;
        }
        mylite_sql_parse_result_deinit(&result);
    }

    failures += mylite_test_expect_true(completed_sweep, test_case->context);
    return failures;
}

static int expect_typed_retry_allocation_sweep(
    const char *sql,
    enum mylite_sql_parser_retry_kind kind,
    const char *context
) {
    int failures = 0;
    bool completed_sweep = false;

    for (size_t allocation_index = 0U; allocation_index < parser_allocation_sweep_limit;
         ++allocation_index) {
        struct mylite_sql_parse_config config = {
            .input = sql,
            .length = strlen(sql),
        };
        struct mylite_sql_parse_result result = {0};
        struct mylite_sql_parser_retry_context retry_context = {0};
        enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
        bool handled = false;
        bool allocation_failed = false;

        mylite_test_allocator_fail_after(allocation_index);
        status = mylite_sql_parser_parse_with_lemon(config, &result);
        if (status == MYLITE_SQL_PARSE_OK || status == MYLITE_SQL_PARSE_SYNTAX_ERROR) {
            status = mylite_sql_parser_retry_context_init(config, &retry_context);
        }
        if (status == MYLITE_SQL_PARSE_OK) {
            status = mylite_sql_parser_try_retry(kind, config, &result, &retry_context, &handled);
        }
        allocation_failed = mylite_test_allocator_was_triggered();
        mylite_test_allocator_clear();

        if (!allocation_failed) {
            failures += mylite_test_expect_int(status, MYLITE_SQL_PARSE_OK, context);
            failures += mylite_test_expect_true(handled, context);
            completed_sweep = true;
            mylite_sql_parser_retry_context_deinit(&retry_context);
            mylite_sql_parse_result_deinit(&result);
            break;
        }
        if (status != MYLITE_SQL_PARSE_NOMEM) {
            fprintf(
                stderr,
                "%s allocation %zu: expected NOMEM, got %s\n",
                context,
                allocation_index,
                mylite_sql_parse_status_name(status)
            );
            failures = 1;
        }
        mylite_sql_parser_retry_context_deinit(&retry_context);
        mylite_sql_parse_result_deinit(&result);
    }

    failures += mylite_test_expect_true(completed_sweep, context);
    return failures;
}

static int test_growable_stack_allocation_failures(void) {
    size_t sql_length = 0U;
    char *sql = make_nested_if_query(parser_growth_sweep_depth, &sql_length);
    int failures = mylite_test_expect_true(sql != NULL, "allocate growable parser stack sweep SQL");

    if (sql == NULL) {
        return failures;
    }
    failures += expect_direct_growable_stack_allocation_sweep(sql, sql_length);
    failures += expect_runtime_growable_stack_allocation_sweep(sql, sql_length);
    free(sql);
    return failures;
}

static int expect_direct_growable_stack_allocation_sweep(const char *sql, size_t sql_length) {
    int failures = 0;
    bool completed_sweep = false;

    for (size_t allocation_index = 0U; allocation_index < parser_allocation_sweep_limit;
         ++allocation_index) {
        struct mylite_sql_parse_result result = {0};
        enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
        bool allocation_failed = false;

        mylite_test_allocator_fail_after(allocation_index);
        status = mylite_sql_parse(
            (struct mylite_sql_parse_config){
                .input = sql,
                .length = sql_length,
            },
            &result
        );
        allocation_failed = mylite_test_allocator_was_triggered();
        mylite_test_allocator_clear();

        if (!allocation_failed) {
            failures += mylite_test_expect_int(
                status,
                MYLITE_SQL_PARSE_OK,
                "complete growable parser stack sweep"
            );
            failures += mylite_test_expect_true(
                result.parser_stack_growth_count > 1U,
                "growable parser sweep performs multiple growth operations"
            );
            completed_sweep = true;
            mylite_sql_parse_result_deinit(&result);
            break;
        }
        if (status != MYLITE_SQL_PARSE_NOMEM || result.status != MYLITE_SQL_PARSE_NOMEM) {
            fprintf(
                stderr,
                "growable parser allocation %zu: expected NOMEM/NOMEM, got %s/%s\n",
                allocation_index,
                mylite_sql_parse_status_name(status),
                mylite_sql_parse_status_name(result.status)
            );
            failures = 1;
        }
        mylite_sql_parse_result_deinit(&result);
    }

    failures +=
        mylite_test_expect_true(completed_sweep, "complete direct growable parser stack sweep");
    return failures;
}

static int expect_runtime_growable_stack_allocation_sweep(const char *sql, size_t sql_length) {
    static const char recovery_sql[] = "SELECT 9";
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open growable parser stack runtime sweep"
    );
    bool completed_sweep = false;
    size_t required_failure_count = 0U;

    if (database == NULL) {
        return failures;
    }
    for (size_t allocation_index = 0U; allocation_index < parser_allocation_sweep_limit;
         ++allocation_index) {
        mylite_result *result = NULL;
        int rc = MYLITE_OK;
        bool allocation_failed = false;

        mylite_test_allocator_fail_after(allocation_index);
        rc = mylite_execute(database, sql, sql_length, &result);
        allocation_failed = mylite_test_allocator_was_triggered();
        mylite_test_allocator_clear();

        if (!allocation_failed) {
            failures += mylite_test_expect_int(
                rc,
                MYLITE_OK,
                "complete runtime growable parser stack sweep"
            );
            failures += mylite_test_expect_true(
                result != NULL,
                "complete runtime growable parser stack result"
            );
            completed_sweep = true;
            mylite_result_free(result);
            break;
        }
        if (rc == MYLITE_OK) {
            failures += mylite_test_expect_true(
                result != NULL,
                "optional runtime allocation fallback result"
            );
            mylite_result_free(result);
            continue;
        }
        if (rc != MYLITE_NOMEM || mylite_errcode(database) != MYLITE_NOMEM ||
            strcmp(mylite_sqlstate(database), "HY001") != 0 ||
            strcmp(mylite_errmsg(database), "out of memory") != 0) {
            fprintf(
                stderr,
                "runtime growable parser allocation %zu: expected NOMEM/HY001, "
                "got %d/%d/%s/%s\n",
                allocation_index,
                rc,
                mylite_errcode(database),
                mylite_sqlstate(database),
                mylite_errmsg(database)
            );
            failures = 1;
        } else {
            ++required_failure_count;
        }
        failures +=
            mylite_test_expect_true(result == NULL, "failed growable parser sweep has no result");
        mylite_result_free(result);
        result = NULL;
        failures += mylite_test_expect_int(
            mylite_execute(database, recovery_sql, sizeof(recovery_sql) - 1U, &result),
            MYLITE_OK,
            "runtime growable parser sweep recovery"
        );
        failures +=
            mylite_test_expect_true(result != NULL, "runtime growable parser recovery result");
        mylite_result_free(result);
    }

    failures +=
        mylite_test_expect_true(completed_sweep, "complete runtime growable parser stack sweep");
    failures += mylite_test_expect_true(
        required_failure_count > 1U,
        "runtime sweep reaches multiple required parser allocations"
    );
    mylite_close(database);
    return failures;
}

static char *make_nested_if_query(size_t depth, size_t *out_length) {
    static const char prefix[] = "SELECT ";
    static const char open[] = "IF(1,1,";
    const size_t prefix_length = sizeof(prefix) - 1U;
    const size_t open_length = sizeof(open) - 1U;
    size_t length = 0U;
    char *sql = NULL;
    char *cursor = NULL;

    if (out_length == NULL || depth > (SIZE_MAX - prefix_length - 1U) / (open_length + 1U)) {
        return NULL;
    }
    length = prefix_length + depth * open_length + 1U + depth;
    sql = (char *)malloc(length + 1U);
    if (sql == NULL) {
        return NULL;
    }
    cursor = sql;
    memcpy(cursor, prefix, prefix_length);
    cursor += prefix_length;
    for (size_t index = 0U; index < depth; ++index) {
        memcpy(cursor, open, open_length);
        cursor += open_length;
    }
    *cursor++ = '0';
    memset(cursor, ')', depth);
    cursor += depth;
    *cursor = '\0';
    *out_length = length;
    return sql;
}

static int test_runtime_parser_allocation_failures(void) {
    static const char query[] = "SELECT (1, 2) = (1, 2)";
    static const size_t parser_failure_indexes[] = {1U, 2U, 4U};
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open runtime retry sweep"
    );

    for (size_t index = 0U; database != NULL && index < sizeof(parser_failure_indexes) /
                                                            sizeof(parser_failure_indexes[0]);
         ++index) {
        mylite_result *result = NULL;
        int rc = MYLITE_OK;
        size_t allocation_index = parser_failure_indexes[index];

        mylite_test_allocator_fail_after(allocation_index);
        rc = mylite_execute(database, query, strlen(query), &result);
        failures += mylite_test_expect_true(
            mylite_test_allocator_was_triggered(),
            "runtime retry failpoint reached"
        );
        mylite_test_allocator_clear();

        if (rc != MYLITE_NOMEM) {
            fprintf(
                stderr,
                "runtime retry allocation %zu: expected NOMEM, got %d/%d/%s/%s\n",
                allocation_index,
                rc,
                mylite_errcode(database),
                mylite_sqlstate(database),
                mylite_errmsg(database)
            );
            failures = 1;
        } else {
            failures += mylite_test_expect_int(
                mylite_errcode(database),
                MYLITE_NOMEM,
                "runtime retry NOMEM error code"
            );
            failures +=
                mylite_test_expect_text(mylite_sqlstate(database), "HY001", "runtime retry state");
            failures += mylite_test_expect_text(
                mylite_errmsg(database),
                "out of memory",
                "runtime retry message"
            );
        }
        failures +=
            mylite_test_expect_true(result == NULL, "failed runtime retry leaves no result");
        failures += mylite_test_expect_int(
            mylite_execute(database, query, strlen(query), &result),
            MYLITE_OK,
            "runtime retry recovery"
        );
        failures += mylite_test_expect_true(result != NULL, "runtime retry recovery result");
        mylite_result_free(result);
    }

    if (database != NULL) {
        mylite_result *result = NULL;

        failures += mylite_test_expect_int(
            mylite_execute(database, query, strlen(query), &result),
            MYLITE_OK,
            "complete runtime retry execution"
        );
        failures += mylite_test_expect_true(result != NULL, "complete runtime retry result");
        mylite_result_free(result);
    }
    mylite_close(database);
    return failures;
}
