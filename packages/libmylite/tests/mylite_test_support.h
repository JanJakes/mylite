#ifndef MYLITE_TEST_SUPPORT_H
#define MYLITE_TEST_SUPPORT_H

#include <mylite/mylite.h>

#include <stddef.h>
#include <stdint.h>

enum {
    mylite_test_temp_path_capacity = 1024,
};

int mylite_test_expect_int(int actual, int expected, const char *context);
int mylite_test_expect_size(size_t actual, size_t expected, const char *context);
int mylite_test_expect_int64(int64_t actual, int64_t expected, const char *context);
int mylite_test_expect_uint64(uint64_t actual, uint64_t expected, const char *context);
int mylite_test_expect_uint32(uint32_t actual, uint32_t expected, const char *context);
int mylite_test_expect_uint16(uint16_t actual, uint16_t expected, const char *context);
int mylite_test_expect_text(const char *actual, const char *expected, const char *context);
int mylite_test_expect_text_or_null(const char *actual, const char *expected, const char *context);
int mylite_test_expect_contains(const char *actual, const char *needle, const char *context);
int mylite_test_expect_true(int condition, const char *context);

int mylite_test_make_path(char *path, size_t path_size, const char *name);
int mylite_test_make_default_path(char *path, size_t path_size);
int mylite_test_make_path_with_suffix(
    char *path,
    size_t path_size,
    const char *name,
    const char *suffix
);
void mylite_test_remove_related_files(const char *path);
int mylite_test_register_temporary_path(const char *path);

int mylite_test_open_temporary(mylite_db **out_database);

int mylite_test_escape_sql_string(char *output, size_t output_size, const char *input);

#endif
