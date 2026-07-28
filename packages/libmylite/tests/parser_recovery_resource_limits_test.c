#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "sql/mylite_parser.h"
#include "sql/mylite_parser_driver.h"
#include "sql/mylite_parser_resources.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    one_mib = 1024 * 1024,
};

static int test_primary_parse_avoids_recovery_resources(void);
static int test_ordinary_recovery_resources(void);
static int test_retry_success_resources(void);
static int test_configured_recovery_resources(void);
static int test_token_limit(void);
static int test_parenthesis_depth_limit(void);
static int test_workspace_limit(void);
static int test_lexer_pass_limit(void);
static int test_large_primary_literal(void);
static int test_public_budget_diagnostics(void);
static char *make_flat_malformed_query(size_t integer_count, size_t *out_length);
static char *make_nested_after_error_query(size_t depth, size_t *out_length);
static char *make_large_literal_query(size_t length);
static int expect_parse_resource_ceiling(
    const struct mylite_sql_parse_result *result,
    size_t input_length,
    const char *context
);
static size_t retry_workspace_limit_for_input(size_t input_length);
static bool token_text_equals(const struct mylite_sql_token *token, const char *expected);

int main(void) {
    int failures = 0;

    failures += test_primary_parse_avoids_recovery_resources();
    failures += test_ordinary_recovery_resources();
    failures += test_retry_success_resources();
    failures += test_configured_recovery_resources();
    failures += test_token_limit();
    failures += test_parenthesis_depth_limit();
    failures += test_workspace_limit();
    failures += test_lexer_pass_limit();
    failures += test_large_primary_literal();
    failures += test_public_budget_diagnostics();
    return failures == 0 ? 0 : 1;
}

static int test_primary_parse_avoids_recovery_resources(void) {
    static const char sql[] = "SELECT 1";
    struct mylite_sql_parse_result result = {0};
    enum mylite_sql_parse_status status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = sizeof(sql) - 1U,
        },
        &result
    );
    int failures = 0;

    failures += mylite_test_expect_int((int)status, MYLITE_SQL_PARSE_OK, "primary parse status");
    failures += mylite_test_expect_size(result.lexer_pass_count, 1U, "primary parse lexer passes");
    failures += mylite_test_expect_size(
        result.retry_tokenization_count,
        0U,
        "primary parse retry tokenizations"
    );
    failures += mylite_test_expect_size(result.retry_token_count, 0U, "primary parse retry tokens");
    failures += mylite_test_expect_size(result.retry_callback_count, 0U, "primary parse callbacks");
    failures += mylite_test_expect_size(
        result.retry_allocation_count,
        0U,
        "primary parse retry allocations"
    );
    failures += mylite_test_expect_size(
        result.retry_workspace_peak_bytes,
        0U,
        "primary parse retry workspace"
    );
    failures += mylite_test_expect_true(
        !result.retry_budget_exhausted,
        "primary parse budget remains available"
    );
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_ordinary_recovery_resources(void) {
    static const char sql[] = "SELECT FROM DUAL";
    struct mylite_sql_parse_result result = {0};
    enum mylite_sql_parse_status status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = sizeof(sql) - 1U,
        },
        &result
    );
    int failures = 0;

    failures +=
        mylite_test_expect_int((int)status, MYLITE_SQL_PARSE_SYNTAX_ERROR, "ordinary retry status");
    failures += mylite_test_expect_size(result.lexer_pass_count, 2U, "ordinary retry lexer passes");
    failures +=
        mylite_test_expect_size(result.retry_tokenization_count, 1U, "ordinary retry tokenization");
    failures +=
        mylite_test_expect_size(result.retry_token_count, 3U, "ordinary retry stored tokens");
    failures += mylite_test_expect_size(
        result.retry_callback_count,
        mylite_sql_parser_retry_callback_limit,
        "ordinary retry callbacks"
    );
    failures += mylite_test_expect_true(
        !result.retry_budget_exhausted,
        "ordinary retry budget remains available"
    );
    failures += expect_parse_resource_ceiling(&result, sizeof(sql) - 1U, "ordinary retry");
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_retry_success_resources(void) {
    static const char row_sql[] = "SELECT (1, 2) = (1, 2)";
    static const char prefix_sql[] = "SELECT 1 FOR UPDATE FOR SHARE";
    static const char version_comment_sql[] = "SELECT 1 1 /*!80000 (1) */ +";
    struct mylite_sql_parse_result result = {0};
    enum mylite_sql_parse_status status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = row_sql,
            .length = sizeof(row_sql) - 1U,
        },
        &result
    );
    int failures = 0;

    failures += mylite_test_expect_int((int)status, MYLITE_SQL_PARSE_OK, "row retry status");
    failures += mylite_test_expect_size(result.retry_handled_count, 1U, "row retry handled count");
    failures += mylite_test_expect_true(
        result.retry_callback_count >= 1U &&
            result.retry_callback_count <= (size_t)mylite_sql_parser_retry_callback_limit,
        "row retry callback ceiling"
    );
    failures += mylite_test_expect_true(
        result.retry_workspace_peak_bytes >
            result.retry_token_count * sizeof(struct mylite_sql_token),
        "row retry builds parenthesis indexes lazily"
    );
    failures += expect_parse_resource_ceiling(&result, sizeof(row_sql) - 1U, "row retry");
    mylite_sql_parse_result_deinit(&result);

    status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = prefix_sql,
            .length = sizeof(prefix_sql) - 1U,
        },
        &result
    );
    failures += mylite_test_expect_int((int)status, MYLITE_SQL_PARSE_OK, "prefix retry status");
    failures += mylite_test_expect_true(
        result.lexer_pass_count >= 3U &&
            result.lexer_pass_count <= (size_t)mylite_sql_parser_lexer_pass_limit,
        "prefix retry lexer-pass ceiling"
    );
    failures += mylite_test_expect_true(
        !result.retry_budget_exhausted,
        "prefix retry budget remains available"
    );
    failures += expect_parse_resource_ceiling(&result, sizeof(prefix_sql) - 1U, "prefix retry");
    mylite_sql_parse_result_deinit(&result);

    status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = version_comment_sql,
            .length = sizeof(version_comment_sql) - 1U,
        },
        &result
    );
    failures += mylite_test_expect_int(
        (int)status,
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "version-comment retry status"
    );
    failures += mylite_test_expect_size(
        result.lexer_pass_count,
        2U,
        "version-comment logical lexer passes"
    );
    failures += mylite_test_expect_true(
        !result.retry_budget_exhausted,
        "version-comment retry budget remains available"
    );
    failures += expect_parse_resource_ceiling(
        &result,
        sizeof(version_comment_sql) - 1U,
        "version-comment retry"
    );
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_configured_recovery_resources(void) {
    static const char parameter_sql[] = "SELECT (1, ?)";
    static const char mode_sql[] = "SELECT FROM DUAL";
    struct mylite_sql_parse_result result = {0};
    enum mylite_sql_parse_status status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = parameter_sql,
            .length = sizeof(parameter_sql) - 1U,
            .allow_parameters = true,
        },
        &result
    );
    int failures = 0;

    failures += mylite_test_expect_int((int)status, MYLITE_SQL_PARSE_OK, "parameter retry status");
    failures +=
        mylite_test_expect_size(result.parameter_count, 1U, "parameter retry parameter count");
    failures += mylite_test_expect_true(
        result.retry_callback_count > 0U,
        "parameter parse exercises retry resources"
    );
    failures +=
        expect_parse_resource_ceiling(&result, sizeof(parameter_sql) - 1U, "parameter retry");
    mylite_sql_parse_result_deinit(&result);

    status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = mode_sql,
            .length = sizeof(mode_sql) - 1U,
            .modes = MYLITE_SQL_MODE_PIPES_AS_CONCAT,
        },
        &result
    );
    failures +=
        mylite_test_expect_int((int)status, MYLITE_SQL_PARSE_SYNTAX_ERROR, "mode retry status");
    failures += mylite_test_expect_size(
        result.retry_callback_count,
        mylite_sql_parser_retry_callback_limit,
        "mode retry callbacks"
    );
    failures += expect_parse_resource_ceiling(&result, sizeof(mode_sql) - 1U, "mode retry");
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_token_limit(void) {
    size_t exact_length = 0U;
    size_t above_length = 0U;
    char *exact_sql =
        make_flat_malformed_query((size_t)mylite_sql_parser_retry_token_limit - 2U, &exact_length);
    char *above_sql =
        make_flat_malformed_query((size_t)mylite_sql_parser_retry_token_limit - 1U, &above_length);
    struct mylite_sql_parse_result result = {0};
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
    int failures = 0;

    if (exact_sql == NULL || above_sql == NULL) {
        fprintf(stderr, "failed to allocate token-limit fixtures\n");
        free(exact_sql);
        free(above_sql);
        return 1;
    }

    status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = exact_sql,
            .length = exact_length,
        },
        &result
    );
    failures += mylite_test_expect_int(
        (int)status,
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "exact token limit status"
    );
    failures += mylite_test_expect_size(
        result.retry_token_count,
        mylite_sql_parser_retry_token_limit,
        "exact token limit stored tokens"
    );
    failures += mylite_test_expect_size(
        result.retry_callback_count,
        mylite_sql_parser_retry_callback_limit,
        "exact token limit callbacks"
    );
    failures +=
        mylite_test_expect_true(!result.retry_budget_exhausted, "exact token limit is admitted");
    failures += expect_parse_resource_ceiling(&result, exact_length, "exact token limit");
    mylite_sql_parse_result_deinit(&result);

    status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = above_sql,
            .length = above_length,
        },
        &result
    );
    failures += mylite_test_expect_int(
        (int)status,
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "above token limit status"
    );
    failures += mylite_test_expect_size(
        result.retry_token_count,
        mylite_sql_parser_retry_token_limit,
        "above token limit stored tokens"
    );
    failures +=
        mylite_test_expect_size(result.retry_callback_count, 0U, "above token limit callbacks");
    failures +=
        mylite_test_expect_true(result.retry_budget_exhausted, "above token limit is rejected");
    failures += mylite_test_expect_true(
        result.error_token.offset == strlen("SELECT 1 "),
        "above token limit preserves initial error offset"
    );
    failures += mylite_test_expect_true(
        token_text_equals(&result.error_token, "1"),
        "above token limit preserves initial error token"
    );
    failures += expect_parse_resource_ceiling(&result, above_length, "above token limit");
    mylite_sql_parse_result_deinit(&result);

    free(exact_sql);
    free(above_sql);
    return failures;
}

static int test_parenthesis_depth_limit(void) {
    size_t exact_length = 0U;
    size_t above_length = 0U;
    char *exact_sql = make_nested_after_error_query(
        mylite_sql_parser_retry_parenthesis_depth_limit,
        &exact_length
    );
    char *above_sql = make_nested_after_error_query(
        (size_t)mylite_sql_parser_retry_parenthesis_depth_limit + 1U,
        &above_length
    );
    struct mylite_sql_parse_result result = {0};
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
    int failures = 0;

    if (exact_sql == NULL || above_sql == NULL) {
        fprintf(stderr, "failed to allocate parenthesis-depth fixtures\n");
        free(exact_sql);
        free(above_sql);
        return 1;
    }

    status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = exact_sql,
            .length = exact_length,
        },
        &result
    );
    failures += mylite_test_expect_int(
        (int)status,
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "exact parenthesis depth status"
    );
    failures += mylite_test_expect_true(
        !result.retry_budget_exhausted,
        "exact parenthesis depth is admitted"
    );
    failures += mylite_test_expect_size(
        result.retry_callback_count,
        mylite_sql_parser_retry_callback_limit,
        "exact parenthesis depth callbacks"
    );
    failures += expect_parse_resource_ceiling(&result, exact_length, "exact parenthesis depth");
    mylite_sql_parse_result_deinit(&result);

    status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = above_sql,
            .length = above_length,
        },
        &result
    );
    failures += mylite_test_expect_int(
        (int)status,
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "above parenthesis depth status"
    );
    failures += mylite_test_expect_true(
        result.retry_budget_exhausted,
        "above parenthesis depth is rejected"
    );
    failures += mylite_test_expect_size(
        result.retry_callback_count,
        0U,
        "above parenthesis depth callbacks"
    );
    failures += mylite_test_expect_true(
        result.error_token.offset == strlen("SELECT 1 "),
        "above parenthesis depth preserves initial error offset"
    );
    failures += expect_parse_resource_ceiling(&result, above_length, "above parenthesis depth");
    mylite_sql_parse_result_deinit(&result);

    free(exact_sql);
    free(above_sql);
    return failures;
}

static int test_workspace_limit(void) {
    struct mylite_sql_parser_resource_tracker tracker = {0};
    int failures = 0;

    mylite_sql_parser_resource_tracker_init(&tracker, 1U);
    failures += mylite_test_expect_size(
        tracker.retry_workspace_limit,
        mylite_sql_parser_retry_workspace_minimum,
        "minimum retry workspace allowance"
    );

    mylite_sql_parser_resource_tracker_init(&tracker, one_mib);
    failures += mylite_test_expect_size(
        tracker.retry_workspace_limit,
        mylite_sql_parser_retry_workspace_limit,
        "absolute retry workspace allowance"
    );
    failures += mylite_test_expect_true(
        mylite_sql_parser_resource_workspace_fits(
            &tracker,
            0U,
            mylite_sql_parser_retry_workspace_limit
        ),
        "exact workspace limit is admitted"
    );
    mylite_sql_parser_resource_record_workspace(
        &tracker,
        0U,
        mylite_sql_parser_retry_workspace_limit
    );
    failures += mylite_test_expect_size(
        tracker.retry_workspace_peak_bytes,
        mylite_sql_parser_retry_workspace_limit,
        "exact workspace limit peak"
    );
    failures += mylite_test_expect_true(
        !mylite_sql_parser_resource_workspace_fits(
            &tracker,
            mylite_sql_parser_retry_workspace_limit,
            (size_t)mylite_sql_parser_retry_workspace_limit + 1U
        ),
        "workspace above limit is rejected"
    );
    failures +=
        mylite_test_expect_true(tracker.retry_budget_exhausted, "workspace limit exhaustion");

    mylite_sql_parser_resource_tracker_init(&tracker, SIZE_MAX);
    failures += mylite_test_expect_size(
        tracker.retry_workspace_limit,
        mylite_sql_parser_retry_workspace_limit,
        "overflowing workspace allowance saturates"
    );
    return failures;
}

static int test_lexer_pass_limit(void) {
    static const char sql[] = "SELECT 1";
    struct mylite_sql_parser_resource_tracker tracker = {0};
    struct mylite_sql_parse_config config = {
        .input = sql,
        .length = sizeof(sql) - 1U,
        .resource_tracker = &tracker,
    };
    struct mylite_sql_parse_result result = {0};
    int failures = 0;

    mylite_sql_parser_resource_tracker_init(&tracker, config.length);
    for (size_t pass = 0U; pass < (size_t)mylite_sql_parser_lexer_pass_limit; ++pass) {
        enum mylite_sql_parse_status status = mylite_sql_parser_parse_with_lemon(config, &result);

        failures +=
            mylite_test_expect_int((int)status, MYLITE_SQL_PARSE_OK, "lexer pass within limit");
        failures +=
            mylite_test_expect_true(!tracker.retry_budget_exhausted, "lexer pass within budget");
        mylite_sql_parse_result_deinit(&result);
    }
    failures += mylite_test_expect_size(
        tracker.lexer_pass_count,
        mylite_sql_parser_lexer_pass_limit,
        "exact lexer pass limit"
    );
    failures += mylite_test_expect_int(
        (int)mylite_sql_parser_parse_with_lemon(config, &result),
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "lexer pass above limit"
    );
    failures +=
        mylite_test_expect_true(tracker.retry_budget_exhausted, "lexer pass limit exhaustion");
    failures += mylite_test_expect_size(
        tracker.lexer_pass_count,
        mylite_sql_parser_lexer_pass_limit,
        "rejected lexer pass is not counted"
    );
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_large_primary_literal(void) {
    char *sql = make_large_literal_query(one_mib);
    struct mylite_sql_parse_result result = {0};
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
    int failures = 0;

    if (sql == NULL) {
        fprintf(stderr, "failed to allocate large primary literal fixture\n");
        return 1;
    }
    status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = one_mib,
        },
        &result
    );
    failures +=
        mylite_test_expect_int((int)status, MYLITE_SQL_PARSE_OK, "large primary literal status");
    failures +=
        mylite_test_expect_size(result.lexer_pass_count, 1U, "large primary literal lexer passes");
    failures +=
        mylite_test_expect_size(result.retry_token_count, 0U, "large primary literal retry tokens");
    failures += mylite_test_expect_size(
        result.retry_workspace_peak_bytes,
        0U,
        "large primary literal retry workspace"
    );
    failures += mylite_test_expect_true(
        !result.retry_budget_exhausted,
        "large primary literal avoids the retry budget"
    );
    mylite_sql_parse_result_deinit(&result);
    free(sql);
    return failures;
}

static int test_public_budget_diagnostics(void) {
    size_t sql_length = 0U;
    char *sql =
        make_flat_malformed_query((size_t)mylite_sql_parser_retry_token_limit - 1U, &sql_length);
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    mylite_stmt *statement = NULL;
    int failures = 0;

    if (sql == NULL) {
        fprintf(stderr, "failed to allocate public budget fixture\n");
        return 1;
    }
    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open public budget database"
    );
    if (database == NULL) {
        free(sql);
        return failures == 0 ? 1 : failures;
    }

    failures += mylite_test_expect_int(
        mylite_execute(database, sql, sql_length, &result),
        MYLITE_ERROR,
        "execute above token limit"
    );
    failures += mylite_test_expect_int(
        mylite_errcode(database),
        mysql_error_parse,
        "execute above token limit error code"
    );
    failures += mylite_test_expect_text(
        mylite_sqlstate(database),
        "42000",
        "execute above token limit SQLSTATE"
    );
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        "syntax",
        "execute above token limit message"
    );
    failures +=
        mylite_test_expect_true(result == NULL, "execute above token limit returns no result");
    mylite_result_free(result);
    result = NULL;

    failures += mylite_test_expect_int(
        mylite_prepare(database, sql, sql_length, &statement),
        MYLITE_ERROR,
        "prepare above token limit"
    );
    failures += mylite_test_expect_int(
        mylite_errcode(database),
        mysql_error_parse,
        "prepare above token limit error code"
    );
    failures += mylite_test_expect_text(
        mylite_sqlstate(database),
        "42000",
        "prepare above token limit SQLSTATE"
    );
    failures += mylite_test_expect_true(
        statement == NULL,
        "prepare above token limit returns no statement"
    );
    if (statement != NULL) {
        failures += mylite_stmt_finalize(statement);
        statement = NULL;
    }

    failures += mylite_test_expect_int(
        mylite_prepare_buffered(database, sql, sql_length, &statement),
        MYLITE_ERROR,
        "prepare buffered above token limit"
    );
    failures += mylite_test_expect_int(
        mylite_errcode(database),
        mysql_error_parse,
        "prepare buffered above token limit error code"
    );
    failures += mylite_test_expect_text(
        mylite_sqlstate(database),
        "42000",
        "prepare buffered above token limit SQLSTATE"
    );
    failures += mylite_test_expect_true(
        statement == NULL,
        "prepare buffered above token limit returns no statement"
    );
    if (statement != NULL) {
        failures += mylite_stmt_finalize(statement);
        statement = NULL;
    }

    failures += mylite_test_expect_int(
        mylite_execute(database, "SELECT 1", strlen("SELECT 1"), &result),
        MYLITE_OK,
        "connection recovery after budget error"
    );
    mylite_result_free(result);
    mylite_close(database);
    free(sql);
    return failures;
}

static char *make_flat_malformed_query(size_t integer_count, size_t *out_length) {
    static const char prefix[] = "SELECT";
    static const char item[] = " 1";
    static const char suffix[] = " +";
    size_t length = 0U;
    char *sql = NULL;
    char *cursor = NULL;

    if (out_length == NULL ||
        integer_count >
            (SIZE_MAX - (sizeof(prefix) - 1U) - (sizeof(suffix) - 1U)) / (sizeof(item) - 1U)) {
        return NULL;
    }
    length = (sizeof(prefix) - 1U) + integer_count * (sizeof(item) - 1U) + (sizeof(suffix) - 1U);
    sql = (char *)malloc(length + 1U);
    if (sql == NULL) {
        return NULL;
    }
    cursor = sql;
    memcpy(cursor, prefix, sizeof(prefix) - 1U);
    cursor += sizeof(prefix) - 1U;
    for (size_t index = 0U; index < integer_count; ++index) {
        memcpy(cursor, item, sizeof(item) - 1U);
        cursor += sizeof(item) - 1U;
    }
    memcpy(cursor, suffix, sizeof(suffix));
    *out_length = length;
    return sql;
}

static char *make_nested_after_error_query(size_t depth, size_t *out_length) {
    static const char prefix[] = "SELECT 1 1 ";
    size_t length = 0U;
    char *sql = NULL;
    char *cursor = NULL;

    if (out_length == NULL || depth > (SIZE_MAX - (sizeof(prefix) - 1U) - 1U) / 2U) {
        return NULL;
    }
    length = (sizeof(prefix) - 1U) + depth * 2U + 1U;
    sql = (char *)malloc(length + 1U);
    if (sql == NULL) {
        return NULL;
    }
    cursor = sql;
    memcpy(cursor, prefix, sizeof(prefix) - 1U);
    cursor += sizeof(prefix) - 1U;
    memset(cursor, '(', depth);
    cursor += depth;
    *cursor++ = '1';
    memset(cursor, ')', depth);
    cursor += depth;
    *cursor = '\0';
    *out_length = length;
    return sql;
}

static char *make_large_literal_query(size_t length) {
    static const char prefix[] = "SELECT '";
    static const char suffix[] = "'";
    size_t framing = (sizeof(prefix) - 1U) + (sizeof(suffix) - 1U);
    char *sql = NULL;

    if (length < framing) {
        return NULL;
    }
    sql = (char *)malloc(length + 1U);
    if (sql == NULL) {
        return NULL;
    }
    memcpy(sql, prefix, sizeof(prefix) - 1U);
    memset(sql + sizeof(prefix) - 1U, 'a', length - framing);
    memcpy(sql + length - (sizeof(suffix) - 1U), suffix, sizeof(suffix));
    return sql;
}

static int expect_parse_resource_ceiling(
    const struct mylite_sql_parse_result *result,
    size_t input_length,
    const char *context
) {
    size_t workspace_limit = retry_workspace_limit_for_input(input_length);
    int failures = 0;

    if (result == NULL) {
        fprintf(stderr, "%s: parse result is null\n", context);
        return 1;
    }
    failures += mylite_test_expect_true(
        result->retry_token_count <= (size_t)mylite_sql_parser_retry_token_limit,
        context
    );
    failures += mylite_test_expect_true(
        result->lexer_pass_count <= (size_t)mylite_sql_parser_lexer_pass_limit,
        context
    );
    failures += mylite_test_expect_true(
        result->retry_callback_count <= (size_t)mylite_sql_parser_retry_callback_limit,
        context
    );
    failures +=
        mylite_test_expect_true(result->retry_workspace_peak_bytes <= workspace_limit, context);
    return failures;
}

static size_t retry_workspace_limit_for_input(size_t input_length) {
    size_t limit = mylite_sql_parser_retry_workspace_limit;

    if (input_length <= SIZE_MAX / (size_t)mylite_sql_parser_retry_workspace_input_multiplier) {
        limit = input_length * (size_t)mylite_sql_parser_retry_workspace_input_multiplier;
        if (limit < (size_t)mylite_sql_parser_retry_workspace_minimum) {
            limit = mylite_sql_parser_retry_workspace_minimum;
        }
        if (limit > (size_t)mylite_sql_parser_retry_workspace_limit) {
            limit = mylite_sql_parser_retry_workspace_limit;
        }
    }
    return limit;
}

static bool token_text_equals(const struct mylite_sql_token *token, const char *expected) {
    size_t expected_length = expected == NULL ? 0U : strlen(expected);

    return token != NULL && token->text != NULL && expected != NULL &&
           token->length == expected_length && memcmp(token->text, expected, expected_length) == 0;
}
