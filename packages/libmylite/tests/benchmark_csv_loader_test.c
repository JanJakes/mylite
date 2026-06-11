#include "mylite_benchmark_csv.h"

#include <stdio.h>
#include <string.h>

static int test_doubled_quote_fields(void);
static int test_backslash_quote_fields(void);
static int test_plain_and_blank_rows(void);
static int expect_parse(
    const char *csv,
    struct mylite_benchmark_owned_query_list *out_queries,
    const char *context
);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_query(
    const struct mylite_benchmark_owned_query_list *queries,
    size_t index,
    const char *expected,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_doubled_quote_fields();
    failures += test_backslash_quote_fields();
    failures += test_plain_and_blank_rows();

    return failures == 0 ? 0 : 1;
}

static int test_doubled_quote_fields(void) {
    static const char csv[] =
        "\"select \"\"$id2\"\", \"\"$$$\"\" from t where t.\"\"$id\"\" = 0\"\n"
        "\"CREATE TABLE \"\" quoted name\"\" (i INT)\"\n";
    struct mylite_benchmark_owned_query_list queries = {0};
    int failures = expect_parse(csv, &queries, "doubled quote parse");

    failures += expect_size(queries.count, 2U, "doubled quote query count");
    failures += expect_query(
        &queries,
        0U,
        "select \"$id2\", \"$$$\" from t where t.\"$id\" = 0",
        "doubled quote SELECT"
    );
    failures += expect_query(
        &queries,
        1U,
        "CREATE TABLE \" quoted name\" (i INT)",
        "doubled quote CREATE TABLE"
    );
    mylite_benchmark_owned_query_list_deinit(&queries);
    return failures;
}

static int test_backslash_quote_fields(void) {
    static const char csv[] =
        "\"INSERT INTO t1 VALUES (1,'color=\\\"STB,NPG\\\"\\r\\nmodel=\\\"ACCORD\\\"')\"\n"
        "\"SELECT DISTINCT\n"
        "(IF( LOCATE( 'year=\\\"', dyninfo ) = 1,\n"
        "LOCATE('\\\"\\r', dyninfo), '' )) AS year\n"
        "FROM t1\"\n";
    struct mylite_benchmark_owned_query_list queries = {0};
    int failures = expect_parse(csv, &queries, "backslash quote parse");

    failures += expect_size(queries.count, 2U, "backslash quote query count");
    failures += expect_query(
        &queries,
        0U,
        "INSERT INTO t1 VALUES (1,'color=\\\"STB,NPG\\\"\\r\\nmodel=\\\"ACCORD\\\"')",
        "backslash quote INSERT"
    );
    failures += expect_query(
        &queries,
        1U,
        "SELECT DISTINCT\n"
        "(IF( LOCATE( 'year=\\\"', dyninfo ) = 1,\n"
        "LOCATE('\\\"\\r', dyninfo), '' )) AS year\n"
        "FROM t1",
        "backslash quote multiline SELECT"
    );
    mylite_benchmark_owned_query_list_deinit(&queries);
    return failures;
}

static int test_plain_and_blank_rows(void) {
    static const char csv[] = "SELECT 1\n\n\"SELECT 2\"\r\n";
    struct mylite_benchmark_owned_query_list queries = {0};
    int failures = expect_parse(csv, &queries, "plain row parse");

    failures += expect_size(queries.count, 2U, "plain row query count");
    failures += expect_query(&queries, 0U, "SELECT 1", "plain row");
    failures += expect_query(&queries, 1U, "SELECT 2", "quoted row");
    mylite_benchmark_owned_query_list_deinit(&queries);
    return failures;
}

static int expect_parse(
    const char *csv,
    struct mylite_benchmark_owned_query_list *out_queries,
    const char *context
) {
    int rc = mylite_benchmark_parse_single_column_csv(csv, strlen(csv), out_queries);

    if (rc != 0) {
        fprintf(stderr, "%s: expected parse success, got %d\n", context, rc);
        return 1;
    }
    return 0;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_query(
    const struct mylite_benchmark_owned_query_list *queries,
    size_t index,
    const char *expected,
    const char *context
) {
    if (index >= queries->count) {
        fprintf(stderr, "%s: missing query %zu\n", context, index);
        return 1;
    }
    if (strcmp(queries->items[index].sql, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected,
            queries->items[index].sql
        );
        return 1;
    }
    return expect_size(queries->items[index].length, strlen(expected), context);
}
