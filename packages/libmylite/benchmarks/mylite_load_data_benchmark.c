#include <mylite/mylite.h>

#ifdef MYLITE_ENABLE_PROFILING
#  include "runtime/mylite_profile_internal.h"
#endif

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#  include <process.h>
#else
#  include <sys/resource.h>
#  include <unistd.h>
#endif

enum {
    default_row_count = 100000,
    default_sample_count = 5,
    default_warmup_count = 1,
    index_count_five = 5,
    path_capacity = 1024,
    sql_capacity = path_capacity * 2,
    wide_field_size = 1024,
    escaped_field_repetitions = 16,
    many_field_count = 15,
    value_modulus = 100000,
    value_a_multiplier = 48271,
    value_b_modulus = 1000,
    decimal_base = 10,
    alphabet_character_count = 26,
    bits_per_byte = 8,
    nanoseconds_per_second = 1000000000ULL,
    nanoseconds_per_millisecond = 1000000,
};

static const uint64_t fnv_offset_basis = 1469598103934665603ULL;
static const uint64_t fnv_prime = 1099511628211ULL;
static const char escaped_field_unit[] = "ab\\tcd\\\\ef";

enum load_shape {
    load_shape_narrow,
    load_shape_wide,
    load_shape_many,
    load_shape_escaped,
};

struct benchmark_options {
    enum load_shape shape;
    size_t index_count;
    size_t row_count;
    size_t sample_count;
    size_t warmup_count;
    const char *output_path;
    bool show_help;
    bool list_shapes;
};

struct named_option {
    const char *name;
    const char *value;
};

struct fixture_summary {
    uint64_t input_bytes;
    uint64_t values[4];
};

struct load_measurement {
    uint64_t elapsed_ns;
    uint64_t peak_rss_kib;
    uint64_t affected_rows;
    uint64_t result_checksum;
    uint64_t allocation_count;
    uint64_t allocation_bytes;
    uint64_t sqlite_steps;
    uint64_t metadata_steps;
};

static int parse_options(int argc, char **argv, struct benchmark_options *out_options);
static bool option_takes_value(const char *argument);
static int parse_option(const struct named_option *option, struct benchmark_options *out_options);
static int parse_size(const char *value, size_t *out_value);
static int parse_size_allow_zero(const char *value, size_t *out_value);
static int parse_shape(const char *value, enum load_shape *out_shape);
static void print_usage(FILE *output, const char *program);
static void print_shapes(FILE *output);
static int write_fixture(
    const char *path,
    const struct benchmark_options *options,
    struct fixture_summary *out_summary
);
static int write_fixture_row(
    FILE *file,
    const struct benchmark_options *options,
    size_t row,
    struct fixture_summary *summary
);
static int write_narrow_row(FILE *file, size_t row, struct fixture_summary *summary);
static int write_wide_row(FILE *file, size_t row, struct fixture_summary *summary);
static int write_many_row(FILE *file, size_t row, struct fixture_summary *summary);
static int write_escaped_row(FILE *file, size_t row, struct fixture_summary *summary);
static int add_written_bytes(int written, struct fixture_summary *summary);
static int add_summary_value(uint64_t *value, uint64_t addition);
static int open_database(const struct benchmark_options *options, mylite_db **out_database);
static const char *table_schema(const struct benchmark_options *options);
static int execute_sql(mylite_db *database, const char *sql);
static int run_benchmark(
    FILE *output,
    mylite_db *database,
    const char *fixture_path,
    const struct benchmark_options *options,
    const struct fixture_summary *fixture
);
static int run_import(
    mylite_db *database,
    const char *fixture_path,
    const struct benchmark_options *options,
    const struct fixture_summary *fixture,
    bool collect_profile,
    struct load_measurement *out_measurement
);
static int begin_transaction(mylite_db *database);
static int execute_load(
    mylite_db *database,
    const char *fixture_path,
    size_t row_count,
    uint64_t *out_affected_rows
);
static int escape_sql_string(const char *input, char *output, size_t output_size);
static int validate_results(
    mylite_db *database,
    enum load_shape shape,
    const struct fixture_summary *fixture,
    uint64_t *out_checksum
);
static int parse_result_uint64(const mylite_result *result, size_t column, uint64_t *out_value);
static uint64_t hash_values(const uint64_t values[4]);
static void hash_uint64(uint64_t *hash, uint64_t value);
static int rollback_transaction(mylite_db *database);
static int validate_empty_table(mylite_db *database);
static uint64_t monotonic_now_ns(void);
static uint64_t process_peak_rss_kib(void);
static void print_header(FILE *output);
static void print_measurement(
    FILE *output,
    const struct benchmark_options *options,
    const struct fixture_summary *fixture,
    size_t sample,
    const struct load_measurement *measurement
);
static const char *shape_name(enum load_shape shape);

int main(int argc, char **argv) {
    struct benchmark_options options = {0};
    struct fixture_summary fixture = {0};
    char fixture_path[path_capacity] = {0};
    mylite_db *database = NULL;
    FILE *output = stdout;
    bool fixture_path_ready = false;
    bool output_owned = false;
    int result = 1;

    if (parse_options(argc, argv, &options) != 0) {
        print_usage(stderr, argv[0]);
        return 2;
    }
    if (options.show_help) {
        print_usage(stdout, argv[0]);
        return 0;
    }
    if (options.list_shapes) {
        print_shapes(stdout);
        return 0;
    }
    if (options.index_count == index_count_five && options.shape != load_shape_narrow) {
        fprintf(stderr, "load-data-benchmark: five indexes require the narrow shape\n");
        return 2;
    }
#if defined(_WIN32)
    int written =
        snprintf(fixture_path, sizeof(fixture_path), "mylite-load-data-%d.tsv", _getpid());
#else
    int written = snprintf(
        fixture_path,
        sizeof(fixture_path),
        "/tmp/mylite-load-data-%ld.tsv",
        (long)getpid()
    );
#endif

    if (written < 0 || (size_t)written >= sizeof(fixture_path)) {
        fprintf(stderr, "load-data-benchmark: failed to create fixture path\n");
        goto cleanup;
    }
    fixture_path_ready = true;
    if (write_fixture(fixture_path, &options, &fixture) != 0) {
        fprintf(stderr, "load-data-benchmark: failed to create fixture\n");
        goto cleanup;
    }
    if (options.output_path != NULL) {
        output = fopen(options.output_path, "wb");
        if (output == NULL) {
            fprintf(stderr, "load-data-benchmark: failed to open output %s\n", options.output_path);
            goto cleanup;
        }
        output_owned = true;
    }
    if (open_database(&options, &database) != 0) {
        goto cleanup;
    }
    result = run_benchmark(output, database, fixture_path, &options, &fixture);

cleanup:
    if (database != NULL) {
        mylite_close(database);
    }
    if (output_owned && fclose(output) != 0) {
        result = 1;
    }
    if (fixture_path_ready && remove(fixture_path) != 0 && errno != ENOENT) {
        fprintf(stderr, "load-data-benchmark: failed to remove fixture %s\n", fixture_path);
        result = 1;
    }
    return result;
}

static int parse_options(int argc, char **argv, struct benchmark_options *out_options) {
    *out_options = (struct benchmark_options){
        .shape = load_shape_narrow,
        .index_count = 0U,
        .row_count = default_row_count,
        .sample_count = default_sample_count,
        .warmup_count = default_warmup_count,
        .output_path = NULL,
        .show_help = false,
        .list_shapes = false,
    };
    for (int index = 1; index < argc; ++index) {
        const char *argument = argv[index];

        if (strcmp(argument, "--help") == 0) {
            out_options->show_help = true;
            continue;
        }
        if (strcmp(argument, "--list-shapes") == 0) {
            out_options->list_shapes = true;
            continue;
        }
        if (!option_takes_value(argument) || index + 1 >= argc) {
            fprintf(stderr, "load-data-benchmark: invalid option %s\n", argument);
            return 1;
        }
        ++index;
        const struct named_option option = {
            .name = argument,
            .value = argv[index],
        };

        if (parse_option(&option, out_options) != 0) {
            fprintf(stderr, "load-data-benchmark: invalid option %s\n", argument);
            return 1;
        }
    }
    return 0;
}

static bool option_takes_value(const char *argument) {
    return strcmp(argument, "--shape") == 0 || strcmp(argument, "--indexes") == 0 ||
           strcmp(argument, "--rows") == 0 || strcmp(argument, "--samples") == 0 ||
           strcmp(argument, "--warmup") == 0 || strcmp(argument, "--output") == 0;
}

static int parse_option(const struct named_option *option, struct benchmark_options *out_options) {
    if (strcmp(option->name, "--shape") == 0) {
        return parse_shape(option->value, &out_options->shape);
    }
    if (strcmp(option->name, "--indexes") == 0) {
        if (parse_size_allow_zero(option->value, &out_options->index_count) != 0 ||
            (out_options->index_count != 0U && out_options->index_count != index_count_five)) {
            return 1;
        }
        return 0;
    }
    if (strcmp(option->name, "--rows") == 0) {
        return parse_size(option->value, &out_options->row_count);
    }
    if (strcmp(option->name, "--samples") == 0) {
        return parse_size(option->value, &out_options->sample_count);
    }
    if (strcmp(option->name, "--warmup") == 0) {
        return parse_size_allow_zero(option->value, &out_options->warmup_count);
    }
    if (strcmp(option->name, "--output") == 0) {
        out_options->output_path = option->value;
        return option->value[0] == '\0' ? 1 : 0;
    }
    return 1;
}

static int parse_size(const char *value, size_t *out_value) {
    char *end = NULL;
    unsigned long long parsed = 0ULL;

    errno = 0;
    parsed = strtoull(value, &end, decimal_base);
    if (errno != 0 || end == value || *end != '\0' || parsed == 0ULL ||
        parsed > (unsigned long long)SIZE_MAX || parsed > (unsigned long long)INT64_MAX) {
        return 1;
    }
    *out_value = (size_t)parsed;
    return 0;
}

static int parse_size_allow_zero(const char *value, size_t *out_value) {
    char *end = NULL;
    unsigned long long parsed = 0ULL;

    errno = 0;
    parsed = strtoull(value, &end, decimal_base);
    if (errno != 0 || end == value || *end != '\0' || parsed > (unsigned long long)SIZE_MAX) {
        return 1;
    }
    *out_value = (size_t)parsed;
    return 0;
}

static int parse_shape(const char *value, enum load_shape *out_shape) {
    static const struct {
        const char *name;
        enum load_shape shape;
    } shapes[] = {
        {"narrow", load_shape_narrow},
        {"wide", load_shape_wide},
        {"many", load_shape_many},
        {"escaped", load_shape_escaped},
    };

    for (size_t index = 0U; index < sizeof(shapes) / sizeof(shapes[0]); ++index) {
        if (strcmp(value, shapes[index].name) == 0) {
            *out_shape = shapes[index].shape;
            return 0;
        }
    }
    return 1;
}

static void print_usage(FILE *output, const char *program) {
    fprintf(
        output,
        "Usage: %s [options]\n"
        "  --shape narrow|wide|many|escaped\n"
        "  --indexes 0|5\n"
        "  --rows N\n"
        "  --samples N\n"
        "  --warmup N\n"
        "  --output PATH\n"
        "  --list-shapes\n"
        "  --help\n",
        program
    );
}

static void print_shapes(FILE *output) {
    fprintf(
        output,
        "narrow\t3 integer fields\n"
        "wide\tid plus one 1024-byte unescaped field\n"
        "many\t16 integer fields\n"
        "escaped\tid plus one 160-byte escape-dense raw field\n"
    );
}

static int write_fixture(
    const char *path,
    const struct benchmark_options *options,
    struct fixture_summary *out_summary
) {
    FILE *file = fopen(path, "wb");
    int result = 1;

    *out_summary = (struct fixture_summary){0};
    if (file == NULL) {
        return 1;
    }
    for (size_t row_index = 0U; row_index < options->row_count; ++row_index) {
        size_t row = row_index + 1U;

        if (write_fixture_row(file, options, row, out_summary) != 0) {
            goto cleanup;
        }
    }
    result = 0;

cleanup:
    if (fclose(file) != 0) {
        result = 1;
    }
    return result;
}

static int write_fixture_row(
    FILE *file,
    const struct benchmark_options *options,
    size_t row,
    struct fixture_summary *summary
) {
    if (add_summary_value(&summary->values[0], 1U) != 0 ||
        add_summary_value(&summary->values[1], (uint64_t)row) != 0) {
        return 1;
    }
    switch (options->shape) {
    case load_shape_narrow:
        return write_narrow_row(file, row, summary);
    case load_shape_wide:
        return write_wide_row(file, row, summary);
    case load_shape_many:
        return write_many_row(file, row, summary);
    case load_shape_escaped:
        return write_escaped_row(file, row, summary);
    }
    return 1;
}

static int write_narrow_row(FILE *file, size_t row, struct fixture_summary *summary) {
    uint64_t value_a = ((uint64_t)row * value_a_multiplier) % value_modulus;
    uint64_t value_b = (uint64_t)row % value_b_modulus;
    int written = fprintf(file, "%zu\t%" PRIu64 "\t%" PRIu64 "\n", row, value_a, value_b);

    if (add_written_bytes(written, summary) != 0 ||
        add_summary_value(&summary->values[2], value_a) != 0 ||
        add_summary_value(&summary->values[3], value_b) != 0) {
        return 1;
    }
    return 0;
}

static int write_wide_row(FILE *file, size_t row, struct fixture_summary *summary) {
    static char payload[wide_field_size];
    static bool initialized = false;
    int written = fprintf(file, "%zu\t", row);

    if (!initialized) {
        for (size_t index = 0U; index < sizeof(payload); ++index) {
            payload[index] = (char)('a' + (index % alphabet_character_count));
        }
        initialized = true;
    }
    if (add_written_bytes(written, summary) != 0 ||
        fwrite(payload, 1U, sizeof(payload), file) != sizeof(payload) || fputc('\n', file) == EOF ||
        add_summary_value(&summary->input_bytes, sizeof(payload) + 1U) != 0 ||
        add_summary_value(&summary->values[2], sizeof(payload)) != 0) {
        return 1;
    }
    summary->values[3] = sizeof(payload);
    return 0;
}

static int write_many_row(FILE *file, size_t row, struct fixture_summary *summary) {
    uint64_t first_value = 0U;
    uint64_t last_value = 0U;
    int written = fprintf(file, "%zu", row);

    if (add_written_bytes(written, summary) != 0) {
        return 1;
    }
    for (size_t field = 1U; field <= many_field_count; ++field) {
        uint64_t value = ((uint64_t)row + field) % value_modulus;

        written = fprintf(file, "\t%" PRIu64, value);
        if (add_written_bytes(written, summary) != 0) {
            return 1;
        }
        if (field == 1U) {
            first_value = value;
        }
        last_value = value;
    }
    if (fputc('\n', file) == EOF || add_summary_value(&summary->input_bytes, 1U) != 0 ||
        add_summary_value(&summary->values[2], first_value) != 0 ||
        add_summary_value(&summary->values[3], last_value) != 0) {
        return 1;
    }
    return 0;
}

static int write_escaped_row(FILE *file, size_t row, struct fixture_summary *summary) {
    const uint64_t decoded_size =
        (uint64_t)escaped_field_repetitions * (sizeof(escaped_field_unit) - 3U);
    int written = fprintf(file, "%zu\t", row);

    if (add_written_bytes(written, summary) != 0) {
        return 1;
    }
    for (size_t index = 0U; index < escaped_field_repetitions; ++index) {
        if (fwrite(escaped_field_unit, 1U, sizeof(escaped_field_unit) - 1U, file) !=
                sizeof(escaped_field_unit) - 1U ||
            add_summary_value(&summary->input_bytes, sizeof(escaped_field_unit) - 1U) != 0) {
            return 1;
        }
    }
    if (fputc('\n', file) == EOF || add_summary_value(&summary->input_bytes, 1U) != 0 ||
        add_summary_value(&summary->values[2], decoded_size) != 0) {
        return 1;
    }
    summary->values[3] = decoded_size;
    return 0;
}

static int add_written_bytes(int written, struct fixture_summary *summary) {
    return written < 0 ? 1 : add_summary_value(&summary->input_bytes, (uint64_t)written);
}

static int add_summary_value(uint64_t *value, uint64_t addition) {
    if (*value > UINT64_MAX - addition) {
        return 1;
    }
    *value += addition;
    return 0;
}

static int open_database(const struct benchmark_options *options, mylite_db **out_database) {
    if (mylite_open_memory(out_database) != MYLITE_OK) {
        fprintf(stderr, "load-data-benchmark: failed to open MyLite database\n");
        return 1;
    }
    if (execute_sql(*out_database, "CREATE DATABASE perf") != 0 ||
        execute_sql(*out_database, "USE perf") != 0 ||
        execute_sql(*out_database, table_schema(options)) != 0) {
        return 1;
    }
    return 0;
}

static const char *table_schema(const struct benchmark_options *options) {
    static const char narrow_zero[] =
        "CREATE TABLE imported (id BIGINT NOT NULL, value_a BIGINT NOT NULL,"
        "value_b BIGINT NOT NULL)";
    static const char narrow_five[] =
        "CREATE TABLE imported (id BIGINT NOT NULL, value_a BIGINT NOT NULL,"
        "value_b BIGINT NOT NULL, PRIMARY KEY (id), KEY idx_a (value_a),"
        "KEY idx_b (value_b), KEY idx_ab (value_a, value_b),"
        "KEY idx_ba (value_b, value_a))";
    static const char wide[] =
        "CREATE TABLE imported (id BIGINT NOT NULL, value_text LONGTEXT NOT NULL)";
    static const char many[] = "CREATE TABLE imported (id BIGINT NOT NULL,"
                               "c01 BIGINT NOT NULL,c02 BIGINT NOT NULL,c03 BIGINT NOT NULL,"
                               "c04 BIGINT NOT NULL,c05 BIGINT NOT NULL,c06 BIGINT NOT NULL,"
                               "c07 BIGINT NOT NULL,c08 BIGINT NOT NULL,c09 BIGINT NOT NULL,"
                               "c10 BIGINT NOT NULL,c11 BIGINT NOT NULL,c12 BIGINT NOT NULL,"
                               "c13 BIGINT NOT NULL,c14 BIGINT NOT NULL,c15 BIGINT NOT NULL)";

    switch (options->shape) {
    case load_shape_narrow:
        return options->index_count == index_count_five ? narrow_five : narrow_zero;
    case load_shape_wide:
    case load_shape_escaped:
        return wide;
    case load_shape_many:
        return many;
    }
    return "";
}

static int execute_sql(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    mylite_result_free(result);
    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "load-data-benchmark: SQL failed: %s\nSQL: %s\n",
            mylite_errmsg(database),
            sql
        );
        return 1;
    }
    return 0;
}

static int run_benchmark(
    FILE *output,
    mylite_db *database,
    const char *fixture_path,
    const struct benchmark_options *options,
    const struct fixture_summary *fixture
) {
    struct load_measurement measurement = {0};

    for (size_t warmup = 0U; warmup < options->warmup_count; ++warmup) {
        if (run_import(database, fixture_path, options, fixture, false, &measurement) != 0) {
            return 1;
        }
    }
    print_header(output);
    for (size_t sample = 1U; sample <= options->sample_count; ++sample) {
        if (run_import(database, fixture_path, options, fixture, true, &measurement) != 0) {
            return 1;
        }
        print_measurement(output, options, fixture, sample, &measurement);
    }
    return ferror(output) == 0 ? 0 : 1;
}

static int run_import(
    mylite_db *database,
    const char *fixture_path,
    const struct benchmark_options *options,
    const struct fixture_summary *fixture,
    bool collect_profile,
    struct load_measurement *out_measurement
) {
    uint64_t started = 0U;
    bool transaction_started = false;
#ifdef MYLITE_ENABLE_PROFILING
    struct mylite_profile_snapshot profile = {0};
    bool profile_started = false;
#endif
    int result = 1;

    *out_measurement = (struct load_measurement){0};
    if (begin_transaction(database) != 0) {
        return 1;
    }
    transaction_started = true;
#ifdef MYLITE_ENABLE_PROFILING
    if (collect_profile) {
        if (mylite_profile_start(database) != MYLITE_OK) {
            fprintf(stderr, "load-data-benchmark: failed to start profile\n");
            goto cleanup;
        }
        profile_started = true;
    }
#else
    (void)collect_profile;
#endif
    started = monotonic_now_ns();
    if (execute_load(database, fixture_path, options->row_count, &out_measurement->affected_rows) !=
        0) {
        goto cleanup;
    }
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
#ifdef MYLITE_ENABLE_PROFILING
    if (profile_started) {
        if (mylite_profile_stop(database, &profile) != MYLITE_OK) {
            profile_started = false;
            fprintf(stderr, "load-data-benchmark: failed to stop profile\n");
            goto cleanup;
        }
        profile_started = false;
        out_measurement->allocation_count = profile.allocation_count;
        out_measurement->allocation_bytes = profile.allocation_bytes;
        out_measurement->sqlite_steps = profile.sqlite_step_count;
        out_measurement->metadata_steps = profile.metadata_step_count;
    }
#endif
    out_measurement->peak_rss_kib = process_peak_rss_kib();
    if (validate_results(database, options->shape, fixture, &out_measurement->result_checksum) !=
            0 ||
        rollback_transaction(database) != 0) {
        transaction_started = false;
        goto cleanup;
    }
    transaction_started = false;
    if (validate_empty_table(database) != 0) {
        goto cleanup;
    }
    result = 0;

cleanup:
#ifdef MYLITE_ENABLE_PROFILING
    if (profile_started) {
        (void)mylite_profile_stop(database, &profile);
    }
#endif
    if (transaction_started) {
        (void)rollback_transaction(database);
    }
    return result;
}

static int begin_transaction(mylite_db *database) {
    mylite_result *result = NULL;
    int rc =
        mylite_execute_transaction_control(database, MYLITE_TRANSACTION_CONTROL_START, &result);

    mylite_result_free(result);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "load-data-benchmark: failed to start transaction\n");
        return 1;
    }
    return 0;
}

static int execute_load(
    mylite_db *database,
    const char *fixture_path,
    size_t row_count,
    uint64_t *out_affected_rows
) {
    char escaped_path[sql_capacity];
    char sql[sql_capacity];
    mylite_result *result = NULL;
    int written = 0;
    int rc = MYLITE_OK;

    if (escape_sql_string(fixture_path, escaped_path, sizeof(escaped_path)) != 0) {
        return 1;
    }
    written = snprintf(sql, sizeof(sql), "LOAD DATA INFILE '%s' INTO TABLE imported", escaped_path);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        return 1;
    }
    rc = mylite_execute(database, sql, (size_t)written, &result);
    if (rc != MYLITE_OK || result == NULL ||
        mylite_result_affected_rows(result) != (int64_t)row_count) {
        fprintf(stderr, "load-data-benchmark: LOAD DATA failed: %s\n", mylite_errmsg(database));
        mylite_result_free(result);
        return 1;
    }
    *out_affected_rows = (uint64_t)mylite_result_affected_rows(result);
    mylite_result_free(result);
    return 0;
}

static int escape_sql_string(const char *input, char *output, size_t output_size) {
    size_t output_index = 0U;

    for (size_t input_index = 0U; input[input_index] != '\0'; ++input_index) {
        if (output_index + 2U >= output_size) {
            return 1;
        }
        output[output_index++] = input[input_index];
        if (input[input_index] == '\'') {
            output[output_index++] = '\'';
        }
    }
    output[output_index] = '\0';
    return 0;
}

static int validate_results(
    mylite_db *database,
    enum load_shape shape,
    const struct fixture_summary *fixture,
    uint64_t *out_checksum
) {
    static const char *const narrow_queries[] = {
        "SELECT COUNT(*) FROM imported",
        "SELECT SUM(id) FROM imported",
        "SELECT SUM(value_a) FROM imported",
        "SELECT SUM(value_b) FROM imported",
    };
    static const char *const text_queries[] = {
        "SELECT COUNT(*) FROM imported",
        "SELECT SUM(id) FROM imported",
        "SELECT SUM(LENGTH(value_text)) FROM imported",
        "SELECT MIN(LENGTH(value_text)) FROM imported",
    };
    static const char *const many_queries[] = {
        "SELECT COUNT(*) FROM imported",
        "SELECT SUM(id) FROM imported",
        "SELECT SUM(c01) FROM imported",
        "SELECT SUM(c15) FROM imported",
    };
    const char *const *queries = NULL;
    uint64_t actual[4] = {0};
    int validation_result = 0;

    switch (shape) {
    case load_shape_narrow:
        queries = narrow_queries;
        break;
    case load_shape_wide:
    case load_shape_escaped:
        queries = text_queries;
        break;
    case load_shape_many:
        queries = many_queries;
        break;
    }
    for (size_t value_index = 0U; value_index < 4U; ++value_index) {
        mylite_result *result = NULL;
        const char *sql = queries[value_index];
        int rc = mylite_execute(database, sql, strlen(sql), &result);

        if (rc != MYLITE_OK || result == NULL || mylite_result_row_count(result) != 1U ||
            mylite_result_column_count(result) != 1U ||
            parse_result_uint64(result, 0U, &actual[value_index]) != 0) {
            fprintf(
                stderr,
                "load-data-benchmark: validation query %zu failed: rc=%d message=%s\n",
                value_index,
                rc,
                mylite_errmsg(database)
            );
            mylite_result_free(result);
            return 1;
        }
        mylite_result_free(result);
        if (actual[value_index] != fixture->values[value_index]) {
            fprintf(
                stderr,
                "load-data-benchmark: result mismatch at value %zu: "
                "actual=%" PRIu64 " expected=%" PRIu64 "\n",
                value_index,
                actual[value_index],
                fixture->values[value_index]
            );
            validation_result = 1;
        }
    }
    *out_checksum = hash_values(actual);
    return validation_result;
}

static int parse_result_uint64(const mylite_result *result, size_t column, uint64_t *out_value) {
    const char *text = mylite_result_value_text(result, 0U, column);
    char *end = NULL;
    unsigned long long value = 0ULL;

    if (text == NULL) {
        return 1;
    }
    errno = 0;
    value = strtoull(text, &end, decimal_base);
    if (errno != 0 || end == text || *end != '\0') {
        return 1;
    }
    *out_value = (uint64_t)value;
    return 0;
}

static uint64_t hash_values(const uint64_t values[4]) {
    uint64_t hash = fnv_offset_basis;

    for (size_t index = 0U; index < 4U; ++index) {
        hash_uint64(&hash, values[index]);
    }
    return hash;
}

static void hash_uint64(uint64_t *hash, uint64_t value) {
    for (size_t byte = 0U; byte < sizeof(value); ++byte) {
        *hash ^= value & UINT64_C(0xff);
        *hash *= fnv_prime;
        value >>= bits_per_byte;
    }
}

static int rollback_transaction(mylite_db *database) {
    mylite_result *result = NULL;
    int rc =
        mylite_execute_transaction_control(database, MYLITE_TRANSACTION_CONTROL_ROLLBACK, &result);

    mylite_result_free(result);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "load-data-benchmark: failed to roll back transaction\n");
        return 1;
    }
    return 0;
}

static int validate_empty_table(mylite_db *database) {
    mylite_result *result = NULL;
    uint64_t row_count = 0U;
    static const char sql[] = "SELECT COUNT(*) FROM imported";
    int rc = mylite_execute(database, sql, sizeof(sql) - 1U, &result);
    int validation_result = 1;

    if (rc == MYLITE_OK && result != NULL && mylite_result_row_count(result) == 1U &&
        mylite_result_column_count(result) == 1U &&
        parse_result_uint64(result, 0U, &row_count) == 0 && row_count == 0U) {
        validation_result = 0;
    } else {
        fprintf(stderr, "load-data-benchmark: rollback left imported rows\n");
    }
    mylite_result_free(result);
    return validation_result;
}

static uint64_t monotonic_now_ns(void) {
    struct timespec now = {0};

#if defined(_WIN32)
    if (timespec_get(&now, TIME_UTC) != TIME_UTC) {
        return 0U;
    }
#else
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0U;
    }
#endif
    return ((uint64_t)now.tv_sec * nanoseconds_per_second) + (uint64_t)now.tv_nsec;
}

static uint64_t process_peak_rss_kib(void) {
#if defined(_WIN32)
    return 0U;
#else
    struct rusage usage = {0};

    if (getrusage(RUSAGE_SELF, &usage) != 0 || usage.ru_maxrss < 0) {
        return 0U;
    }
#  if defined(__APPLE__)
    return (uint64_t)usage.ru_maxrss / 1024U;
#  else
    return (uint64_t)usage.ru_maxrss;
#  endif
#endif
}

static void print_header(FILE *output) {
    fprintf(
        output,
        "record,shape,index_count,rows,sample,input_bytes,total_ms,"
        "rows_per_second,bytes_per_second,process_peak_rss_kib,affected_rows,"
        "result_checksum,allocations,allocation_bytes,sqlite_steps,metadata_steps\n"
    );
}

static void print_measurement(
    FILE *output,
    const struct benchmark_options *options,
    const struct fixture_summary *fixture,
    size_t sample,
    const struct load_measurement *measurement
) {
    double seconds = (double)measurement->elapsed_ns / (double)nanoseconds_per_second;
    double total_ms = (double)measurement->elapsed_ns / (double)nanoseconds_per_millisecond;
    double rows_per_second = seconds == 0.0 ? 0.0 : (double)options->row_count / seconds;
    double bytes_per_second = seconds == 0.0 ? 0.0 : (double)fixture->input_bytes / seconds;

    fprintf(
        output,
        "measurement,%s,%zu,%zu,%zu,%" PRIu64 ",%.6f,%.3f,%.3f,%" PRIu64 ",%" PRIu64 ",%" PRIu64
        ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
        shape_name(options->shape),
        options->index_count,
        options->row_count,
        sample,
        fixture->input_bytes,
        total_ms,
        rows_per_second,
        bytes_per_second,
        measurement->peak_rss_kib,
        measurement->affected_rows,
        measurement->result_checksum,
        measurement->allocation_count,
        measurement->allocation_bytes,
        measurement->sqlite_steps,
        measurement->metadata_steps
    );
}

static const char *shape_name(enum load_shape shape) {
    switch (shape) {
    case load_shape_narrow:
        return "narrow";
    case load_shape_wide:
        return "wide";
    case load_shape_many:
        return "many";
    case load_shape_escaped:
        return "escaped";
    }
    return "unknown";
}
