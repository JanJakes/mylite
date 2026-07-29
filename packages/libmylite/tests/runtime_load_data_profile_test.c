#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_profile_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

enum {
    test_path_capacity = 1024,
    escaped_path_capacity = test_path_capacity * 2,
    sql_capacity = escaped_path_capacity + 128,
    profile_row_count = 10000,
    allocation_count_limit = 64,
    allocation_byte_limit = 1024 * 1024,
    sqlite_step_fixed_allowance = 32,
    profile_value_a_modulus = 1000,
    profile_value_b_modulus = 100,
};

static int test_load_data_allocations(void);
static int execute_sql(mylite_db *database, const char *sql, const char *context);
static int write_profile_fixture(const char *path);

int main(void) {
    return test_load_data_allocations() == 0 ? 0 : 1;
}

static int test_load_data_allocations(void) {
    struct mylite_profile_snapshot snapshot = {0};
    char path[test_path_capacity];
    char escaped_path[escaped_path_capacity];
    char sql[sql_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;
    int written = 0;

    if (mylite_test_make_path(path, sizeof(path), "load_data_profile.tsv") != 0) {
        return 1;
    }
    (void)remove(path);
    if (write_profile_fixture(path) != 0) {
        return 1;
    }
    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open load profile");
    failures += execute_sql(database, "CREATE DATABASE app", "create load profile schema");
    failures += execute_sql(database, "USE app", "use load profile schema");
    failures += execute_sql(
        database,
        "CREATE TABLE imported (id INT NOT NULL, value_a INT NOT NULL, value_b INT NOT NULL)",
        "create load profile table"
    );

    failures += mylite_test_expect_int(
        mylite_test_escape_sql_string(escaped_path, sizeof(escaped_path), path),
        0,
        "escape load profile path"
    );
    written = snprintf(sql, sizeof(sql), "LOAD DATA INFILE '%s' INTO TABLE imported", escaped_path);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "load profile SQL is too long\n");
        failures += 1;
    }

    failures += mylite_test_expect_int(
        mylite_profile_start(database),
        MYLITE_OK,
        "start load allocation profile"
    );
    failures += mylite_test_expect_int(
        mylite_execute(database, sql, (size_t)written, &result),
        MYLITE_OK,
        "execute profiled load"
    );
    failures += mylite_test_expect_int(
        mylite_profile_stop(database, &snapshot),
        MYLITE_OK,
        "stop load allocation profile"
    );
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        profile_row_count,
        "profiled load affected rows"
    );
    failures += mylite_test_expect_true(
        snapshot.allocation_count < allocation_count_limit,
        "profiled load allocation count is bounded"
    );
    failures += mylite_test_expect_true(
        snapshot.allocation_bytes < allocation_byte_limit,
        "profiled load allocation bytes are bounded"
    );
    failures += mylite_test_expect_true(
        snapshot.sqlite_step_count >= profile_row_count &&
            snapshot.sqlite_step_count <= profile_row_count + sqlite_step_fixed_allowance,
        "profiled load SQLite steps stay row-linear"
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_sql(database, "SELECT COUNT(*) FROM imported", "count profiled load rows");
    mylite_close(database);
    (void)remove(path);
    return failures;
}

static int execute_sql(mylite_db *database, const char *sql, const char *context) {
    mylite_result *result = NULL;
    int failures = mylite_test_expect_int(
        mylite_execute(database, sql, strlen(sql), &result),
        MYLITE_OK,
        context
    );

    mylite_result_free(result);
    return failures;
}

static int write_profile_fixture(const char *path) {
    FILE *file = fopen(path, "wb");

    if (file == NULL) {
        fprintf(stderr, "failed to open LOAD DATA profile fixture\n");
        return 1;
    }
    for (int row = 1; row <= profile_row_count; ++row) {
        if (fprintf(
                file,
                "%d\t%d\t%d\n",
                row,
                row % profile_value_a_modulus,
                row % profile_value_b_modulus
            ) < 0) {
            fprintf(stderr, "failed to write LOAD DATA profile fixture\n");
            (void)fclose(file);
            return 1;
        }
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close LOAD DATA profile fixture\n");
        return 1;
    }
    return 0;
}
