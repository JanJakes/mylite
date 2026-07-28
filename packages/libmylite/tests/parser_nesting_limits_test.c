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
    enum nested_shape shape;
    size_t depth;
    enum mylite_sql_parse_status expected_status;
    const char *context;
};

static int test_shallow_parser_stack(void);
static int test_growable_parser_stack(void);
static int test_parser_stack_ceiling(void);
static int expect_nested_parse(struct nesting_case test_case);
static char *make_nested_sql(enum nested_shape shape, size_t depth, size_t *out_length);
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
        mylite_test_expect_int((int)status, (int)MYLITE_SQL_PARSE_OK, "shallow parser status");

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
        {nested_shape_parentheses, 507U, MYLITE_SQL_PARSE_OK, "old parenthesis success boundary"},
        {nested_shape_parentheses, 508U, MYLITE_SQL_PARSE_OK, "old parenthesis overflow boundary"},
        {nested_shape_parentheses, 1024U, MYLITE_SQL_PARSE_OK, "1024 parentheses"},
        {nested_shape_parentheses, 4096U, MYLITE_SQL_PARSE_OK, "4096 parentheses"},
        {
            nested_shape_parentheses,
            16384U,
            MYLITE_SQL_PARSE_OK,
            "MySQL parenthesis compatibility floor",
        },
        {nested_shape_if, 84U, MYLITE_SQL_PARSE_OK, "old IF success boundary"},
        {nested_shape_if, 85U, MYLITE_SQL_PARSE_OK, "old IF overflow boundary"},
        {nested_shape_if, 512U, MYLITE_SQL_PARSE_OK, "512 IF calls"},
        {nested_shape_if, 1024U, MYLITE_SQL_PARSE_OK, "1024 IF calls"},
        {nested_shape_if, 1732U, MYLITE_SQL_PARSE_OK, "MySQL IF compatibility floor"},
        {nested_shape_corpus_if, 85U, MYLITE_SQL_PARSE_OK, "old corpus IF overflow boundary"},
        {nested_shape_corpus_if, 512U, MYLITE_SQL_PARSE_OK, "deep corpus IF shape"},
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
            nested_shape_parentheses,
            32768U,
            MYLITE_SQL_PARSE_STACK_OVERFLOW,
            "parenthesis stack ceiling",
        },
        {
            nested_shape_if,
            8192U,
            MYLITE_SQL_PARSE_STACK_OVERFLOW,
            "IF stack ceiling",
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
    char *sql = make_nested_sql(test_case.shape, test_case.depth, &sql_length);
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

static char *make_nested_sql(enum nested_shape shape, size_t depth, size_t *out_length) {
    static const char prefix[] = "SELECT ";
    const char *open = nested_open_text(shape);
    const size_t prefix_length = sizeof(prefix) - 1U;
    size_t open_length = 0U;
    size_t length = 0U;
    char *sql = NULL;
    char *cursor = NULL;

    if (open == NULL || out_length == NULL) {
        return NULL;
    }
    open_length = strlen(open);
    if (depth > (SIZE_MAX - prefix_length - 1U) / (open_length + 1U)) {
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
