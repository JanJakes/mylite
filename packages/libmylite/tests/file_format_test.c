#include "mylite_test_support.h"

#include "storage/mylite_file_format.h"

#include <stdio.h>
#include <string.h>

enum { invalid_lifecycle_state_byte = 0xffU };

static int test_preamble_init_sets_format_fields(void);
static int test_preamble_lifecycle_states(void);
static int test_legacy_preamble_is_committed(void);
static int test_version_two_preamble_retains_lifecycle(void);
static int test_preamble_validate_rejects_corruption(void);
static int test_preamble_u16_is_big_endian(void);
static int expect_u16(unsigned int actual, unsigned int expected, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_preamble_init_sets_format_fields();
    failures += test_preamble_lifecycle_states();
    failures += test_legacy_preamble_is_committed();
    failures += test_version_two_preamble_retains_lifecycle();
    failures += test_preamble_validate_rejects_corruption();
    failures += test_preamble_u16_is_big_endian();

    return failures == 0 ? 0 : 1;
}

static int test_preamble_init_sets_format_fields(void) {
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    static const unsigned char zeroes[MYLITE_FILE_RESERVED_SIZE];
    int failures = 0;

    mylite_file_preamble_init(preamble);

    failures +=
        expect_bytes(preamble, MYLITE_FILE_MAGIC_TEXT, MYLITE_FILE_MAGIC_SIZE, "magic text");
    failures += expect_u16(
        mylite_file_preamble_get_u16(preamble, MYLITE_FILE_FORMAT_VERSION_OFFSET),
        MYLITE_FILE_FORMAT_VERSION,
        "format version"
    );
    failures += mylite_test_expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_COMMITTED,
        "committed lifecycle state"
    );
    failures += expect_bytes(
        &preamble[MYLITE_FILE_RESERVED_OFFSET],
        zeroes,
        MYLITE_FILE_RESERVED_SIZE,
        "reserved bytes"
    );
    failures +=
        mylite_test_expect_int(mylite_file_preamble_validate(preamble), 1, "validate initialized");

    return failures;
}

static int test_preamble_lifecycle_states(void) {
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    int failures = 0;

    mylite_file_preamble_init_with_state(preamble, MYLITE_FILE_LIFECYCLE_INITIALIZING);
    failures += mylite_test_expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_INITIALIZING,
        "initializing lifecycle state"
    );
    failures += mylite_test_expect_int(
        mylite_file_preamble_validate(preamble),
        0,
        "initializing not openable"
    );

    mylite_file_preamble_init_with_state(preamble, MYLITE_FILE_LIFECYCLE_RECOVERY_REQUIRED);
    failures += mylite_test_expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_RECOVERY_REQUIRED,
        "recovery-required lifecycle state"
    );
    failures += mylite_test_expect_int(
        mylite_file_preamble_validate(preamble),
        0,
        "recovery-required not openable"
    );

    mylite_file_preamble_init(preamble);
    preamble[MYLITE_FILE_LIFECYCLE_STATE_OFFSET] = invalid_lifecycle_state_byte;
    failures += mylite_test_expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_INVALID,
        "invalid lifecycle state"
    );

    return failures;
}

static int test_legacy_preamble_is_committed(void) {
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    int failures = 0;

    memset(preamble, 0, sizeof(preamble));
    memcpy(preamble, MYLITE_FILE_MAGIC_TEXT, MYLITE_FILE_MAGIC_SIZE);
    preamble[MYLITE_FILE_FORMAT_VERSION_OFFSET + 1U] = MYLITE_FILE_LEGACY_FORMAT_VERSION;
    failures += mylite_test_expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_COMMITTED,
        "legacy lifecycle state"
    );
    failures += mylite_test_expect_int(
        mylite_file_preamble_validate(preamble),
        1,
        "legacy preamble validates"
    );

    preamble[MYLITE_FILE_LIFECYCLE_STATE_OFFSET] = 1U;
    failures += mylite_test_expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_INVALID,
        "legacy reserved bytes remain zero"
    );

    return failures;
}

static int test_version_two_preamble_retains_lifecycle(void) {
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    int failures = 0;

    mylite_file_preamble_init(preamble);
    preamble[MYLITE_FILE_FORMAT_VERSION_OFFSET] = 0U;
    preamble[MYLITE_FILE_FORMAT_VERSION_OFFSET + 1U] = MYLITE_FILE_LIFECYCLE_FORMAT_VERSION;
    failures += mylite_test_expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_COMMITTED,
        "version two committed lifecycle"
    );
    failures += expect_u16(
        mylite_file_preamble_format_version(preamble),
        MYLITE_FILE_LIFECYCLE_FORMAT_VERSION,
        "version two format"
    );

    preamble[MYLITE_FILE_LIFECYCLE_STATE_OFFSET] = MYLITE_FILE_LIFECYCLE_INITIALIZING;
    failures += mylite_test_expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_INITIALIZING,
        "version two initializing lifecycle"
    );

    return failures;
}

static int test_preamble_validate_rejects_corruption(void) {
    enum { invalid_magic_mask = MYLITE_FILE_BYTE_MASK };

    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    int failures = 0;

    mylite_file_preamble_init(preamble);
    preamble[0] ^= invalid_magic_mask;
    failures +=
        mylite_test_expect_int(mylite_file_preamble_validate(preamble), 0, "reject invalid magic");

    mylite_file_preamble_init(preamble);
    preamble[MYLITE_FILE_FORMAT_VERSION_OFFSET + 1U] =
        (unsigned char)(MYLITE_FILE_FORMAT_VERSION + 1);
    failures += mylite_test_expect_int(
        mylite_file_preamble_validate(preamble),
        0,
        "reject invalid version"
    );

    mylite_file_preamble_init(preamble);
    preamble[MYLITE_FILE_RESERVED_OFFSET] = 1U;
    failures +=
        mylite_test_expect_int(mylite_file_preamble_validate(preamble), 0, "reject reserved byte");

    return failures;
}

static int test_preamble_u16_is_big_endian(void) {
    enum {
        expected_first_u16 = 0x1234U,
        expected_second_u16 = 0x3456U,
    };

    static const unsigned char bytes[] = {0x12U, 0x34U, 0x56U};
    int failures = 0;

    failures +=
        expect_u16(mylite_file_preamble_get_u16(bytes, 0U), expected_first_u16, "first u16");
    failures +=
        expect_u16(mylite_file_preamble_get_u16(bytes, 1U), expected_second_u16, "second u16");

    return failures;
}

static int expect_u16(unsigned int actual, unsigned int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected 0x%04x, got 0x%04x\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte sequence mismatch\n", context);
        return 1;
    }

    return 0;
}
