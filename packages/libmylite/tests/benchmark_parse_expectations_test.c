#include "mylite_benchmark_parse_expectations.h"

#include <stdio.h>
#include <string.h>

static int test_valid_manifest(void);
static int test_manifest_errors(void);
static int expect_parse_error(const char *manifest, const char *context);

int main(void) {
    int failures = 0;

    failures += test_valid_manifest();
    failures += test_manifest_errors();

    return failures == 0 ? 0 : 1;
}

static int test_valid_manifest(void) {
    enum {
        incomplete_alter_query_index = 508,
        ansi_quotes_query_index = 13982,
    };

    static const char manifest[] = "# query\tstatus\ttoken\treason\n"
                                   "508\tsyntax_error\teof\tincomplete ALTER\n"
                                   "13982\tsyntax_error\tstring\tANSI_QUOTES row\n";
    struct mylite_benchmark_expected_parse_failure_list expectations = {0};
    const struct mylite_benchmark_expected_parse_failure *first = NULL;
    const struct mylite_benchmark_expected_parse_failure *second = NULL;
    int failures = 0;

    if (mylite_benchmark_parse_expected_parse_failures(
            manifest,
            strlen(manifest),
            "valid",
            &expectations
        ) != 0) {
        fprintf(stderr, "valid manifest failed to parse\n");
        return 1;
    }
    if (expectations.count != 2U) {
        fprintf(stderr, "expected 2 manifest rows, got %zu\n", expectations.count);
        ++failures;
    }
    first =
        mylite_benchmark_expected_parse_failure_find(&expectations, incomplete_alter_query_index);
    second = mylite_benchmark_expected_parse_failure_find(&expectations, ansi_quotes_query_index);
    if (first == NULL || second == NULL) {
        fprintf(stderr, "expected manifest lookup rows to exist\n");
        ++failures;
    }
    if (first != NULL && !mylite_benchmark_expected_parse_failure_matches(
                             first,
                             MYLITE_SQL_PARSE_SYNTAX_ERROR,
                             MYLITE_SQL_TOKEN_EOF
                         )) {
        fprintf(stderr, "expected first manifest row to match syntax/eof\n");
        ++failures;
    }
    if (second != NULL && !mylite_benchmark_expected_parse_failure_matches(
                              second,
                              MYLITE_SQL_PARSE_SYNTAX_ERROR,
                              MYLITE_SQL_TOKEN_STRING
                          )) {
        fprintf(stderr, "expected second manifest row to match syntax/string\n");
        ++failures;
    }
    if (second != NULL && mylite_benchmark_expected_parse_failure_matches(
                              second,
                              MYLITE_SQL_PARSE_SYNTAX_ERROR,
                              MYLITE_SQL_TOKEN_KEYWORD
                          )) {
        fprintf(stderr, "manifest row matched the wrong token kind\n");
        ++failures;
    }
    if (mylite_benchmark_expected_parse_failure_find(&expectations, 1U) != NULL) {
        fprintf(stderr, "unexpected manifest lookup row exists\n");
        ++failures;
    }
    mylite_benchmark_expected_parse_failure_list_deinit(&expectations);
    return failures;
}

static int test_manifest_errors(void) {
    int failures = 0;

    failures += expect_parse_error("0\tsyntax_error\teof\tzero index\n", "zero index");
    failures += expect_parse_error("1\tok\teof\tok status\n", "ok status");
    failures += expect_parse_error("1\tbogus\teof\tbad status\n", "bad status");
    failures += expect_parse_error("1\tsyntax_error\tbogus\tbad token\n", "bad token");
    failures += expect_parse_error("1\tsyntax_error\teof\tone\n1\tsyntax_error\teof\ttwo\n", "dup");
    failures += expect_parse_error("1\tsyntax_error\teof\n", "short row");
    failures += expect_parse_error("1\tsyntax_error\teof\t\n", "empty reason");

    return failures;
}

static int expect_parse_error(const char *manifest, const char *context) {
    struct mylite_benchmark_expected_parse_failure_list expectations = {0};
    int rc = mylite_benchmark_parse_expected_parse_failures(
        manifest,
        strlen(manifest),
        context,
        &expectations
    );

    mylite_benchmark_expected_parse_failure_list_deinit(&expectations);
    if (rc == 0) {
        fprintf(stderr, "%s: expected manifest parse error\n", context);
        return 1;
    }
    return 0;
}
