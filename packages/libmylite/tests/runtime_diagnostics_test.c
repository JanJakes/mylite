#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "runtime/mylite_diagnostics.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int test_null_public_diagnostic_accessors(void);
static int test_live_diagnostics_set_read_reset_and_warning_order(void);
static int test_diagnostics_replace_copies_condition_and_warnings(void);
static int test_diagnostics_deinit_and_misuse_paths(void);

int main(void) {
    int failures = 0;

    failures += test_null_public_diagnostic_accessors();
    failures += test_live_diagnostics_set_read_reset_and_warning_order();
    failures += test_diagnostics_replace_copies_condition_and_warnings();
    failures += test_diagnostics_deinit_and_misuse_paths();

    return failures == 0 ? 0 : 1;
}

static int test_null_public_diagnostic_accessors(void) {
    int failures = 0;

    failures += mylite_test_expect_int(mylite_errcode(NULL), MYLITE_MISUSE, "NULL errcode");
    failures += mylite_test_expect_text(mylite_sqlstate(NULL), "HY000", "NULL SQLSTATE");
    failures += mylite_test_expect_text(
        mylite_errmsg(NULL),
        "bad parameter or other API misuse",
        "NULL error message"
    );

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

    failures += mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open handle");
    diagnostics = mylite_connection_diagnostics(database);
    failures += mylite_test_expect_true(diagnostics != NULL, "diagnostics exists");

    failures += mylite_test_expect_int(mylite_errcode(database), MYLITE_OK, "initial errcode");
    failures += mylite_test_expect_text(mylite_sqlstate(database), "00000", "initial SQLSTATE");
    failures += mylite_test_expect_text(mylite_errmsg(database), "not an error", "initial message");

    mylite_diagnostics_set_error(diagnostics, MYLITE_ERROR, "HY001", "synthetic error");
    failures += mylite_test_expect_int(mylite_errcode(database), MYLITE_ERROR, "set errcode");
    failures += mylite_test_expect_text(mylite_sqlstate(database), "HY001", "set SQLSTATE");
    failures += mylite_test_expect_text(mylite_errmsg(database), "synthetic error", "set message");

    failures += mylite_test_expect_int(
        mylite_diagnostics_append_warning(
            diagnostics,
            synthetic_warning_code,
            "01000",
            "first warning"
        ),
        MYLITE_OK,
        "append first warning"
    );
    failures += mylite_test_expect_int(
        mylite_diagnostics_append_warning(
            diagnostics,
            synthetic_second_warning_code,
            "01000",
            "second warning"
        ),
        MYLITE_OK,
        "append second warning"
    );
    failures +=
        mylite_test_expect_size(mylite_diagnostics_warning_count(diagnostics), 2U, "warning count");

    warning = mylite_diagnostics_warning_at(diagnostics, 0U);
    failures += mylite_test_expect_true(warning != NULL, "first warning exists");
    if (warning != NULL) {
        failures +=
            mylite_test_expect_int(warning->code, synthetic_warning_code, "first warning code");
        failures +=
            mylite_test_expect_text(warning->message, "first warning", "first warning message");
    }
    warning = mylite_diagnostics_warning_at(diagnostics, 1U);
    failures += mylite_test_expect_true(warning != NULL, "second warning exists");
    if (warning != NULL) {
        failures += mylite_test_expect_int(
            warning->code,
            synthetic_second_warning_code,
            "second warning code"
        );
        failures +=
            mylite_test_expect_text(warning->message, "second warning", "second warning message");
    }

    mylite_diagnostics_reset(diagnostics);
    failures += mylite_test_expect_int(mylite_errcode(database), MYLITE_OK, "reset errcode");
    failures += mylite_test_expect_text(mylite_sqlstate(database), "00000", "reset SQLSTATE");
    failures += mylite_test_expect_text(mylite_errmsg(database), "not an error", "reset message");
    failures += mylite_test_expect_size(
        mylite_diagnostics_warning_count(diagnostics),
        0U,
        "reset warning count"
    );

    mylite_close(database);

    return failures;
}

static int test_diagnostics_deinit_and_misuse_paths(void) {
    struct mylite_diagnostics zero_diagnostics = {0};
    struct mylite_diagnostics diagnostics;
    int failures = 0;

    mylite_diagnostics_deinit(&zero_diagnostics);

    failures += mylite_test_expect_int(
        mylite_diagnostics_append_warning(NULL, MYLITE_ERROR, "01000", "warning"),
        MYLITE_MISUSE,
        "append warning rejects NULL diagnostics"
    );

    mylite_diagnostics_init(&diagnostics);
    diagnostics.warning_count = SIZE_MAX;
    failures += mylite_test_expect_int(
        mylite_diagnostics_append_warning(&diagnostics, MYLITE_ERROR, "01000", "warning"),
        MYLITE_NOMEM,
        "append warning rejects count overflow"
    );
    failures += mylite_test_expect_int(
        mylite_diagnostics_errcode(&diagnostics),
        MYLITE_NOMEM,
        "warning overflow sets diagnostic"
    );
    mylite_diagnostics_deinit(&diagnostics);

    return failures;
}

static int test_diagnostics_replace_copies_condition_and_warnings(void) {
    enum { copied_warning_code = 1287 };

    struct mylite_diagnostics source;
    struct mylite_diagnostics destination;
    const struct mylite_diagnostic_record *warning = NULL;
    int failures = 0;

    mylite_diagnostics_init(&source);
    mylite_diagnostics_init(&destination);

    mylite_diagnostics_set_error(&source, MYLITE_ERROR, "42000", "source error");
    failures += mylite_test_expect_int(
        mylite_diagnostics_append_warning(&source, copied_warning_code, "HY000", "source warning"),
        MYLITE_OK,
        "append source warning"
    );
    failures += mylite_test_expect_int(
        mylite_diagnostics_replace(&destination, &source),
        MYLITE_OK,
        "replace diagnostics"
    );

    mylite_diagnostics_reset(&source);
    failures += mylite_test_expect_int(
        mylite_diagnostics_errcode(&destination),
        MYLITE_ERROR,
        "copied error code"
    );
    failures += mylite_test_expect_text(
        mylite_diagnostics_sqlstate(&destination),
        "42000",
        "copied SQLSTATE"
    );
    failures += mylite_test_expect_text(
        mylite_diagnostics_errmsg(&destination),
        "source error",
        "copied message"
    );
    failures += mylite_test_expect_size(
        mylite_diagnostics_warning_count(&destination),
        1U,
        "copied warning count"
    );
    warning = mylite_diagnostics_warning_at(&destination, 0U);
    failures += mylite_test_expect_true(warning != NULL, "copied warning exists");
    if (warning != NULL) {
        failures +=
            mylite_test_expect_int(warning->code, copied_warning_code, "copied warning code");
        failures += mylite_test_expect_text(warning->sqlstate, "HY000", "copied warning SQLSTATE");
        failures +=
            mylite_test_expect_text(warning->message, "source warning", "copied warning message");
    }

    failures += mylite_test_expect_int(
        mylite_diagnostics_replace(NULL, &source),
        MYLITE_MISUSE,
        "replace rejects NULL destination"
    );
    failures += mylite_test_expect_int(
        mylite_diagnostics_replace(&destination, NULL),
        MYLITE_MISUSE,
        "replace rejects NULL source"
    );

    mylite_diagnostics_deinit(&destination);
    mylite_diagnostics_deinit(&source);

    return failures;
}
