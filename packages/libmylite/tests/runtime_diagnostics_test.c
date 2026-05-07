#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "runtime/mylite_diagnostics.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int test_null_public_diagnostic_accessors(void);
static int test_live_diagnostics_set_read_reset_and_warning_order(void);
static int test_diagnostics_deinit_and_misuse_paths(void);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);

int main(void) {
    int failures = 0;

    failures += test_null_public_diagnostic_accessors();
    failures += test_live_diagnostics_set_read_reset_and_warning_order();
    failures += test_diagnostics_deinit_and_misuse_paths();

    return failures == 0 ? 0 : 1;
}

static int test_null_public_diagnostic_accessors(void) {
    int failures = 0;

    failures += expect_int(mylite_errcode(NULL), MYLITE_MISUSE, "NULL errcode");
    failures += expect_text(mylite_sqlstate(NULL), "HY000", "NULL SQLSTATE");
    failures +=
        expect_text(mylite_errmsg(NULL), "bad parameter or other API misuse", "NULL error message");

    return failures;
}

static int test_live_diagnostics_set_read_reset_and_warning_order(void) {
    enum {
        synthetic_warning_code = 1000,
        synthetic_second_warning_code = 1001,
    };

    mylite_db *database = NULL;
    struct mylite_diagnostics *diagnostics = NULL;
    const struct mylite_diagnostic_record *warning = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open handle");
    diagnostics = mylite_connection_diagnostics(database);
    failures += expect_true(diagnostics != NULL, "diagnostics exists");

    failures += expect_int(mylite_errcode(database), MYLITE_OK, "initial errcode");
    failures += expect_text(mylite_sqlstate(database), "00000", "initial SQLSTATE");
    failures += expect_text(mylite_errmsg(database), "not an error", "initial message");

    mylite_diagnostics_set_error(diagnostics, MYLITE_ERROR, "HY001", "synthetic error");
    failures += expect_int(mylite_errcode(database), MYLITE_ERROR, "set errcode");
    failures += expect_text(mylite_sqlstate(database), "HY001", "set SQLSTATE");
    failures += expect_text(mylite_errmsg(database), "synthetic error", "set message");

    failures += expect_int(
        mylite_diagnostics_append_warning(
            diagnostics,
            synthetic_warning_code,
            "01000",
            "first warning"
        ),
        MYLITE_OK,
        "append first warning"
    );
    failures += expect_int(
        mylite_diagnostics_append_warning(
            diagnostics,
            synthetic_second_warning_code,
            "01000",
            "second warning"
        ),
        MYLITE_OK,
        "append second warning"
    );
    failures += expect_size(mylite_diagnostics_warning_count(diagnostics), 2U, "warning count");

    warning = mylite_diagnostics_warning_at(diagnostics, 0U);
    failures += expect_true(warning != NULL, "first warning exists");
    if (warning != NULL) {
        failures += expect_int(warning->code, synthetic_warning_code, "first warning code");
        failures += expect_text(warning->message, "first warning", "first warning message");
    }
    warning = mylite_diagnostics_warning_at(diagnostics, 1U);
    failures += expect_true(warning != NULL, "second warning exists");
    if (warning != NULL) {
        failures += expect_int(warning->code, synthetic_second_warning_code, "second warning code");
        failures += expect_text(warning->message, "second warning", "second warning message");
    }

    mylite_diagnostics_reset(diagnostics);
    failures += expect_int(mylite_errcode(database), MYLITE_OK, "reset errcode");
    failures += expect_text(mylite_sqlstate(database), "00000", "reset SQLSTATE");
    failures += expect_text(mylite_errmsg(database), "not an error", "reset message");
    failures +=
        expect_size(mylite_diagnostics_warning_count(diagnostics), 0U, "reset warning count");

    mylite_close(database);

    return failures;
}

static int test_diagnostics_deinit_and_misuse_paths(void) {
    struct mylite_diagnostics zero_diagnostics = {0};
    struct mylite_diagnostics diagnostics;
    int failures = 0;

    mylite_diagnostics_deinit(&zero_diagnostics);

    failures += expect_int(
        mylite_diagnostics_append_warning(NULL, MYLITE_ERROR, "01000", "warning"),
        MYLITE_MISUSE,
        "append warning rejects NULL diagnostics"
    );

    mylite_diagnostics_init(&diagnostics);
    diagnostics.warning_count = SIZE_MAX;
    failures += expect_int(
        mylite_diagnostics_append_warning(&diagnostics, MYLITE_ERROR, "01000", "warning"),
        MYLITE_NOMEM,
        "append warning rejects count overflow"
    );
    failures += expect_int(
        mylite_diagnostics_errcode(&diagnostics),
        MYLITE_NOMEM,
        "warning overflow sets diagnostic"
    );
    mylite_diagnostics_deinit(&diagnostics);

    return failures;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
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

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", context, expected, actual);
        return 1;
    }

    return 0;
}
