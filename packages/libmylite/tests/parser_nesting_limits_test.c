#include "mylite_test_support.h"

#include "sql/mylite_parser.h"
#include "sql/mylite_parser_resources.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum nested_shape {
    nested_shape_parentheses = 0,
    nested_shape_if,
    nested_shape_corpus_if,
};

struct nesting_case {
    size_t depth;
    const char *context;
    enum nested_shape shape;
    enum mylite_sql_parse_status expected_status;
};

static int test_shallow_parser_stack(void);
static int test_growable_parser_stack(void);
static int test_parser_stack_ceiling(void);
static int expect_nested_parse(struct nesting_case test_case);
static char *make_nested_sql(struct nesting_case test_case, size_t *out_length);
static const char *nested_open_text(enum nested_shape shape);

int main(void) {
    int failures = 0;

    failures += test_shallow_parser_stack();
    failures += test_growable_parser_stack();
    failures += test_parser_stack_ceiling();
    return failures == 0 ? 0 : 1;
}

static int test_shallow_parser_stack(void) {
    static const char sql[] = "SELECT 1";
    struct mylite_sql_parse_result result = {0};
    enum mylite_sql_parse_status status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = sizeof(sql) - 1U,
        },
        &result
    );
    int failures =
        mylite_test_expect_int((int)status, MYLITE_SQL_PARSE_OK, "shallow parser status");

    failures += mylite_test_expect_size(
        result.parser_stack_growth_count,
        0U,
        "shallow parser has no stack growth"
    );
    failures += mylite_test_expect_size(
        result.parser_stack_allocation_peak_bytes,
        0U,
        "shallow parser has no stack allocation"
    );
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_growable_parser_stack(void) {
    static const struct nesting_case cases[] = {
        {
            .depth = 507U,
            .context = "old parenthesis success boundary",
            .shape = nested_shape_parentheses,
            .expected_status = MYLITE_SQL_PARSE_OK,
        },
        {
            .depth = 508U,
            .context = "old parenthesis overflow boundary",
            .shape = nested_shape_parentheses,
            .expected_status = MYLITE_SQL_PARSE_OK,
        },
        {
            .depth = 1024U,
            .context = "1024 parentheses",
            .shape = nested_shape_parentheses,
            .expected_status = MYLITE_SQL_PARSE_OK,
        },
        {
            .depth = 4096U,
            .context = "4096 parentheses",
            .shape = nested_shape_parentheses,
            .expected_status = MYLITE_SQL_PARSE_OK,
        },
        {
            .depth = 16384U,
            .context = "MySQL parenthesis compatibility floor",
            .shape = nested_shape_parentheses,
            .expected_status = MYLITE_SQL_PARSE_OK,
        },
        {
            .depth = 84U,
            .context = "old IF success boundary",
            .shape = nested_shape_if,
            .expected_status = MYLITE_SQL_PARSE_OK,
        },
        {
            .depth = 85U,
            .context = "old IF overflow boundary",
            .shape = nested_shape_if,
            .expected_status = MYLITE_SQL_PARSE_OK,
        },
        {
            .depth = 512U,
            .context = "512 IF calls",
            .shape = nested_shape_if,
            .expected_status = MYLITE_SQL_PARSE_OK,
        },
        {
            .depth = 1024U,
            .context = "1024 IF calls",
            .shape = nested_shape_if,
            .expected_status = MYLITE_SQL_PARSE_OK,
        },
        {
            .depth = 1732U,
            .context = "MySQL IF compatibility floor",
            .shape = nested_shape_if,
            .expected_status = MYLITE_SQL_PARSE_OK,
        },
        {
            .depth = 85U,
            .context = "old corpus IF overflow boundary",
            .shape = nested_shape_corpus_if,
            .expected_status = MYLITE_SQL_PARSE_OK,
        },
        {
            .depth = 512U,
            .context = "deep corpus IF shape",
            .shape = nested_shape_corpus_if,
            .expected_status = MYLITE_SQL_PARSE_OK,
        },
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        failures += expect_nested_parse(cases[index]);
    }
    return failures;
}

static int test_parser_stack_ceiling(void) {
    static const struct nesting_case cases[] = {
        {
            .depth = 32768U,
            .context = "parenthesis stack ceiling",
            .shape = nested_shape_parentheses,
            .expected_status = MYLITE_SQL_PARSE_STACK_OVERFLOW,
        },
        {
            .depth = 8192U,
            .context = "IF stack ceiling",
            .shape = nested_shape_if,
            .expected_status = MYLITE_SQL_PARSE_STACK_OVERFLOW,
        },
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        failures += expect_nested_parse(cases[index]);
    }
    failures += test_shallow_parser_stack();
    return failures;
}

static int expect_nested_parse(struct nesting_case test_case) {
    struct mylite_sql_parse_result result = {0};
    size_t sql_length = 0U;
    char *sql = make_nested_sql(test_case, &sql_length);
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_MISUSE;
    int failures = mylite_test_expect_true(sql != NULL, test_case.context);

    if (sql == NULL) {
        return failures;
    }
    status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = sql_length,
        },
        &result
    );
    failures +=
        mylite_test_expect_int((int)status, (int)test_case.expected_status, test_case.context);
    failures += mylite_test_expect_int(
        (int)result.status,
        (int)test_case.expected_status,
        test_case.context
    );
    failures += mylite_test_expect_true(
        result.parser_stack_growth_count > 0U,
        "nested parse grows parser stack"
    );
    failures += mylite_test_expect_true(
        result.parser_stack_allocation_peak_bytes > 0U &&
            result.parser_stack_allocation_peak_bytes < (size_t)mylite_sql_parser_stack_byte_limit,
        "nested parser allocation obeys byte ceiling"
    );
    mylite_sql_parse_result_deinit(&result);
    free(sql);
    return failures;
}

static char *make_nested_sql(struct nesting_case test_case, size_t *out_length) {
    static const char prefix[] = "SELECT ";
    const char *open = nested_open_text(test_case.shape);
    const size_t prefix_length = sizeof(prefix) - 1U;
    size_t open_length = 0U;
    size_t length = 0U;
    char *sql = NULL;
    char *cursor = NULL;

    if (open == NULL || out_length == NULL) {
        return NULL;
    }
    open_length = strlen(open);
    if (test_case.depth > (SIZE_MAX - prefix_length - 1U) / (open_length + 1U)) {
        return NULL;
    }
    length = prefix_length + test_case.depth * open_length + 1U + test_case.depth;
    sql = (char *)malloc(length + 1U);
    if (sql == NULL) {
        return NULL;
    }
    cursor = sql;
    memcpy(cursor, prefix, prefix_length);
    cursor += prefix_length;
    for (size_t index = 0U; index < test_case.depth; ++index) {
        memcpy(cursor, open, open_length);
        cursor += open_length;
    }
    *cursor++ = '0';
    memset(cursor, ')', test_case.depth);
    cursor += test_case.depth;
    *cursor = '\0';
    *out_length = length;
    return sql;
}

static const char *nested_open_text(enum nested_shape shape) {
    switch (shape) {
    case nested_shape_parentheses:
        return "(";
    case nested_shape_if:
        return "IF(1,1,";
    case nested_shape_corpus_if:
        return "IF((ROUND(1,2)=1),2,";
    }
    return NULL;
}
