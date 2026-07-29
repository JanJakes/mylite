#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "sqlite3.h"

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
#include <sys/stat.h>
#include <time.h>

#if defined(_WIN32)
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    default_row_count = 100000,
    default_sample_count = 5,
    default_warmup_iterations = 1,
    path_capacity = 1024,
    generated_text_capacity = 160,
    import_line_capacity = 192,
    import_sql_capacity = path_capacity * 2,
    decimal_base = 10,
    sqlite_busy_timeout_ms = 5000,
    nanoseconds_per_second = 1000000000ULL,
    nanoseconds_per_millisecond = 1000000,
    nanoseconds_per_microsecond = 1000,
    microseconds_per_second = 1000000ULL,
    milliseconds_per_second = 1000,
    minimum_account_count = 100,
    minimum_tag_count = 100,
    maximum_tag_count = 2000,
    rows_per_account = 100,
    account_region_count = 32,
    category_count = 1000,
    score_multiplier = 48271,
    score_modulus = 100000,
    score_range_lower = 10000,
    score_range_upper = 90000,
    correlated_score_threshold = 95000,
    pseudo_random_increment = 17,
    bridge_tag_span = 9,
    write_log_id_factor = 10,
    seed_progress_interval = 100000,
    seed_timestamp_base = 1700000000,
    seconds_per_year = 31536000,
    tag_weight_modulus = 100,
    item_created_at_parameter = 5,
    item_title_parameter = 6,
    item_payload_parameter = 7,
    item_tenant_parameter = 8,
    item_optional_parameter = 9,
    skew_tenant_count = 10000,
    null_value_interval = 10,
    selectivity_basis_points = 10000,
    selectivity_one_basis_point = 1,
    selectivity_one_percent_basis_points = 100,
    selectivity_ten_percent_basis_points = 1000,
    result_small_limit = 100,
    result_medium_limit = 10000,
    unindexed_sort_limit = 1000,
    batch_insert_count = 10,
    fanout_parent_count = 100,
    bits_per_byte = 8,
    byte_mask = 255,
    median_pair_count = 2,
    or_lookup_third_offset_multiplier = 2,
    deep_offset_limit = 20,
    deep_offset_numerator = 9,
    deep_offset_denominator = 10,
    window_partition_account_limit = 10,
    attribution_layer_count = 4,
    attribution_insert_prefix_size = 12,
    attribution_identifier_sql_extra = 32,
    attribution_query_sql_extra = 64,
    attribution_parameter_sql_capacity = 24,
};

static const uint64_t fnv_offset_basis = 1469598103934665603ULL;

enum benchmark_engine_kind {
    benchmark_engine_mylite,
    benchmark_engine_sqlite,
    benchmark_engine_mylite_physical,
    benchmark_engine_mylite_guarded,
};

enum benchmark_execution_mode {
    benchmark_execution_prepare_each,
    benchmark_execution_prepared,
    benchmark_execution_write_rollback,
    benchmark_execution_expected_error_rollback,
    benchmark_execution_bulk_import_rollback,
};

enum benchmark_scenario_id {
    benchmark_scenario_point_lookup_prepare_each,
    benchmark_scenario_point_lookup_prepared,
    benchmark_scenario_secondary_lookup,
    benchmark_scenario_range_aggregate,
    benchmark_scenario_full_scan_expression,
    benchmark_scenario_text_expression,
    benchmark_scenario_group_aggregate,
    benchmark_scenario_indexed_order_limit,
    benchmark_scenario_parent_join,
    benchmark_scenario_bridge_join,
    benchmark_scenario_correlated_exists,
    benchmark_scenario_indexed_update,
    benchmark_scenario_foreign_key_insert,
    benchmark_scenario_foreign_key_cascade,
    benchmark_scenario_selectivity_zero,
    benchmark_scenario_selectivity_one,
    benchmark_scenario_selectivity_one_basis_point,
    benchmark_scenario_selectivity_one_percent,
    benchmark_scenario_selectivity_ten_percent,
    benchmark_scenario_selectivity_full,
    benchmark_scenario_covering_range,
    benchmark_scenario_noncovering_range,
    benchmark_scenario_null_lookup,
    benchmark_scenario_or_lookup,
    benchmark_scenario_deep_offset,
    benchmark_scenario_unindexed_sort_limit,
    benchmark_scenario_high_cardinality_group,
    benchmark_scenario_high_cardinality_distinct,
    benchmark_scenario_window_partition,
    benchmark_scenario_three_table_join,
    benchmark_scenario_left_join,
    benchmark_scenario_anti_join,
    benchmark_scenario_large_large_join,
    benchmark_scenario_skew_hot,
    benchmark_scenario_skew_cold,
    benchmark_scenario_update_one_basis_point,
    benchmark_scenario_update_one_percent,
    benchmark_scenario_update_ten_percent,
    benchmark_scenario_delete_one_basis_point,
    benchmark_scenario_delete_one_percent,
    benchmark_scenario_upsert_hit,
    benchmark_scenario_upsert_miss,
    benchmark_scenario_insert_index_zero,
    benchmark_scenario_insert_index_one,
    benchmark_scenario_insert_index_five,
    benchmark_scenario_insert_index_ten,
    benchmark_scenario_insert_batch_ten,
    benchmark_scenario_load_data_index_zero,
    benchmark_scenario_load_data_index_five,
    benchmark_scenario_composite_foreign_key_insert,
    benchmark_scenario_composite_foreign_key_invalid,
    benchmark_scenario_foreign_key_cascade_fanout,
    benchmark_scenario_foreign_key_restrict,
    benchmark_scenario_foreign_key_set_null,
    benchmark_scenario_result_narrow_small,
    benchmark_scenario_result_narrow_medium,
    benchmark_scenario_result_narrow_full,
    benchmark_scenario_result_wide_small,
    benchmark_scenario_result_wide_medium,
};

struct benchmark_options {
    size_t row_count;
    size_t sample_count;
    size_t warmup_iterations;
    size_t iteration_override;
    const char *scenario_name;
    const char *database_directory;
    const char *database_base;
    const char *output_path;
    const char *attribution_layer_name;
    bool keep_databases;
    bool reuse_databases;
    bool seed_only;
    bool attribution_seed;
    bool attribution_timing;
    bool analyze;
    bool list_scenarios;
    bool show_help;
};

struct benchmark_scenario {
    const char *name;
    enum benchmark_scenario_id id;
    enum benchmark_execution_mode mode;
    const char *sql;
    size_t default_iterations;
};

enum attribution_table_id {
    attribution_table_accounts,
    attribution_table_items,
    attribution_table_item_tags,
    attribution_table_write_log,
    attribution_table_upsert_targets,
    attribution_table_composite_parents,
    attribution_table_composite_children,
    attribution_table_fanout_parents,
    attribution_table_fanout_children,
    attribution_table_restrict_parents,
    attribution_table_restrict_children,
    attribution_table_set_null_parents,
    attribution_table_set_null_children,
    attribution_table_count,
};

struct attribution_program {
    const char *logical_table;
    char physical_table[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *guarded_sql;
    char *plain_sql;
    size_t parameter_count;
    uint64_t sql_hash;
    bool has_guard;
    bool is_seeded;
};

struct attribution_program_set {
    struct attribution_program programs[attribution_table_count];
    size_t captured_program_count;
    bool capture_failed;
};

struct benchmark_database {
    enum benchmark_engine_kind kind;
    mylite_db *mylite;
    sqlite3 *sqlite;
    const char *path;
    const struct attribution_program_set *attribution_programs;
    bool profile_seed;
#ifdef MYLITE_ENABLE_PROFILING
    struct mylite_profile_snapshot *manual_profile;
#endif
};

struct benchmark_statement {
    enum benchmark_engine_kind kind;
    mylite_stmt *mylite;
    sqlite3_stmt *sqlite;
#ifdef MYLITE_ENABLE_PROFILING
    struct mylite_profile_snapshot *manual_profile;
#endif
};

struct benchmark_measurement {
    uint64_t elapsed_ns;
    uint64_t checksum;
    size_t operation_count;
    size_t result_row_count;
    size_t result_value_bytes;
    uint64_t affected_rows;
#ifdef MYLITE_ENABLE_PROFILING
    struct mylite_profile_snapshot profile;
#endif
};

struct benchmark_load_measurement {
    uint64_t total_ns;
    uint64_t total_cpu_ns;
    uint64_t accounts_ns;
    uint64_t accounts_cpu_ns;
    uint64_t items_ns;
    uint64_t items_cpu_ns;
    uint64_t tags_ns;
    uint64_t tags_cpu_ns;
    uint64_t support_ns;
    uint64_t support_cpu_ns;
    uint64_t support_upsert_ns;
    uint64_t support_upsert_cpu_ns;
    uint64_t support_fanout_ns;
    uint64_t support_fanout_cpu_ns;
    uint64_t support_restrict_ns;
    uint64_t support_restrict_cpu_ns;
    uint64_t support_set_null_ns;
    uint64_t support_set_null_cpu_ns;
#ifdef MYLITE_ENABLE_PROFILING
    struct mylite_profile_snapshot profile;
    struct mylite_profile_snapshot accounts_profile;
    struct mylite_profile_snapshot items_profile;
    struct mylite_profile_snapshot tags_profile;
    struct mylite_profile_snapshot support_profile;
    struct mylite_profile_snapshot support_upsert_profile;
    struct mylite_profile_snapshot support_fanout_profile;
    struct mylite_profile_snapshot support_restrict_profile;
    struct mylite_profile_snapshot support_set_null_profile;
#endif
};

struct benchmark_dataset {
    size_t row_count;
    size_t account_count;
    size_t tag_count;
};

struct benchmark_bulk_import_options {
    size_t row_count;
    size_t warmup_iterations;
};

struct benchmark_statement_run_options {
    size_t iterations;
    size_t warmup_iterations;
};

struct benchmark_paths {
    char mylite[path_capacity];
    char sqlite[path_capacity];
};

struct benchmark_sample_pair {
    struct benchmark_measurement mylite;
    struct benchmark_measurement sqlite;
};

static int parse_options(int argc, char **argv, struct benchmark_options *out_options);
static bool parse_flag_option(const char *argument, struct benchmark_options *out_options);
static bool option_takes_value(const char *argument);
static int parse_named_option(
    const char *argument,
    struct benchmark_options *out_options,
    const char *value
);
static void print_usage(const char *program_name, FILE *stream);
static void print_scenarios(FILE *stream);
static int run_benchmark(const struct benchmark_options *options);
static int run_attribution(const struct benchmark_options *options);
static bool attribution_options_are_valid(const struct benchmark_options *options);
static FILE *open_attribution_output(const struct benchmark_options *options);
static void print_attribution_header(
    FILE *output,
    const struct benchmark_dataset *dataset,
    const struct attribution_program_set *programs
);
static int run_attribution_samples(
    const struct benchmark_options *options,
    const struct benchmark_dataset *dataset,
    const struct attribution_program_set *programs,
    FILE *output
);
static int run_attribution_sample(
    const struct benchmark_options *options,
    const struct benchmark_dataset *dataset,
    const struct attribution_program_set *programs,
    FILE *output,
    size_t sample
);
static bool attribution_layer_from_name(const char *name, enum benchmark_engine_kind *out_kind);
static int discover_attribution_programs(
    const struct benchmark_options *options,
    const struct benchmark_dataset *dataset,
    struct attribution_program_set *out_programs
);
static int run_attribution_layer(
    const struct benchmark_options *options,
    const struct benchmark_dataset *dataset,
    const struct attribution_program_set *programs,
    enum benchmark_engine_kind kind,
    const char *path,
    FILE *output,
    size_t sample,
    uint64_t out_checksums[attribution_table_count]
);
static int attribution_trace_callback(
    unsigned int event,
    void *context,
    void *statement_pointer,
    void *sql_pointer
);
static int load_attribution_physical_names(
    sqlite3 *sqlite,
    struct attribution_program_set *programs
);
static int validate_attribution_programs(struct attribution_program_set *programs);
static int validate_attribution_layout(
    sqlite3 *sqlite,
    const struct attribution_program_set *programs
);
static int build_plain_attribution_sql(struct attribution_program *program);
static void deinit_attribution_programs(struct attribution_program_set *programs);
static const struct attribution_program *find_attribution_program(
    const struct attribution_program_set *programs,
    const char *logical_sql
);
static int make_attribution_path(
    const struct benchmark_options *options,
    const char *suffix,
    char *out_path,
    size_t out_path_capacity
);
static int verify_attribution_database(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset,
    uint64_t out_checksums[attribution_table_count]
);
static int read_attribution_checksums(
    struct benchmark_database *database,
    uint64_t out_checksums[attribution_table_count]
);
static int verify_attribution_checksums(
    size_t sample,
    uint64_t checksums[attribution_layer_count][attribution_table_count]
);
static void print_attribution_measurement(
    FILE *output,
    const struct benchmark_dataset *dataset,
    enum benchmark_engine_kind kind,
    size_t sample,
    const struct benchmark_load_measurement *measurement,
    const uint64_t checksums[attribution_table_count]
);
static void print_attribution_phase(
#ifdef MYLITE_ENABLE_PROFILING
    FILE *output,
    const struct benchmark_dataset *dataset,
    enum benchmark_engine_kind kind,
    size_t sample,
    const char *phase,
    uint64_t dataset_hash,
    uint64_t elapsed_ns,
    uint64_t cpu_ns,
    const struct mylite_profile_snapshot *profile,
    const struct mylite_profile_snapshot *previous
);
#else
    FILE *output,
    const struct benchmark_dataset *dataset,
    enum benchmark_engine_kind kind,
    size_t sample,
    const char *phase,
    uint64_t dataset_hash,
    uint64_t elapsed_ns,
    uint64_t cpu_ns
);
#endif
static bool engine_uses_mylite_api(enum benchmark_engine_kind kind);
static bool engine_uses_mylite_storage(enum benchmark_engine_kind kind);
static bool engine_uses_direct_sqlite(enum benchmark_engine_kind kind);
static int prepare_benchmark_databases(
    const struct benchmark_options *options,
    struct benchmark_paths *paths,
    const struct benchmark_dataset *dataset,
    struct benchmark_database *mylite,
    struct benchmark_database *sqlite,
    struct benchmark_load_measurement *mylite_load,
    struct benchmark_load_measurement *sqlite_load
);
static int make_database_paths(
    const struct benchmark_options *options,
    struct benchmark_paths *out_paths
);
static int open_benchmark_database(
    enum benchmark_engine_kind kind,
    const char *path,
    struct benchmark_database *out_database
);
static int create_benchmark_schema(struct benchmark_database *database);
static int seed_benchmark_database(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset,
    struct benchmark_load_measurement *out_measurement
);
#ifdef MYLITE_ENABLE_PROFILING
static int finish_load_profile(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset,
    struct benchmark_load_measurement *measurement,
    bool *profile_started
);
static void capture_load_profile(
    const struct benchmark_database *database,
    struct mylite_profile_snapshot *out_profile
);
static int step_profiled_sqlite_statement(struct benchmark_statement *statement);
static void record_manual_statement_status(
    struct mylite_profile_snapshot *profile,
    sqlite3_stmt *statement,
    uint64_t elapsed_ns
);
static void add_manual_profile_counter(uint64_t *counter, uint64_t value);
#endif
static int verify_benchmark_database(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset
);
static void print_csv_header(FILE *output);
static void print_load_measurements(
    FILE *output,
    const struct benchmark_dataset *dataset,
    const struct benchmark_load_measurement *mylite,
    const struct benchmark_load_measurement *sqlite
);
static int run_scenarios(
    FILE *output,
    struct benchmark_database *mylite,
    struct benchmark_database *sqlite,
    const struct benchmark_dataset *dataset,
    const struct benchmark_options *options
);
static int run_scenario_samples(
    FILE *output,
    const struct benchmark_scenario *scenario,
    struct benchmark_database *mylite,
    struct benchmark_database *sqlite,
    const struct benchmark_dataset *dataset,
    const struct benchmark_options *options
);
static int run_scenario_engine(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iterations,
    size_t warmup_iterations,
    struct benchmark_measurement *out_measurement
);
static int run_statement_scenario_engine(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    struct benchmark_statement_run_options options,
    struct benchmark_measurement *out_measurement
);
static int prepare_statement_scenario(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t warmup_iterations,
    struct benchmark_statement *statement,
    struct benchmark_statement **out_prepared_statement
);
#ifdef MYLITE_ENABLE_PROFILING
static int start_statement_scenario_profile(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    bool *profile_started
);
static int finish_statement_scenario_profile(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    size_t iterations,
    struct benchmark_measurement *measurement,
    bool *profile_started
);
#endif
static int run_bulk_import_engine(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const struct benchmark_bulk_import_options *options,
    struct benchmark_measurement *out_measurement
);
static int create_bulk_import_file(
    const struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    size_t row_count,
    char *out_path,
    size_t out_path_size
);
static int run_bulk_import_once(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const char *path,
    size_t row_count,
    struct benchmark_measurement *measurement
);
static int run_mylite_bulk_import(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const char *path,
    size_t row_count,
    struct benchmark_measurement *measurement
);
static int run_sqlite_bulk_import(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const char *path,
    size_t row_count,
    struct benchmark_measurement *measurement
);
static const char *bulk_import_table(const struct benchmark_scenario *scenario);
static const char *scenario_sql(
    const struct benchmark_database *database,
    const struct benchmark_scenario *scenario
);
static int run_scenario_phase(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    struct benchmark_statement *prepared_statement,
    size_t iterations,
    struct benchmark_measurement *out_measurement
);
static int execute_scenario_iteration(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    struct benchmark_statement *prepared_statement,
    size_t iteration,
    struct benchmark_measurement *measurement
);
static int bind_scenario_parameters(
    struct benchmark_statement *statement,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iteration
);
static int bind_core_scenario_parameters(
    struct benchmark_statement *statement,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iteration
);
static int bind_read_scenario_parameters(
    struct benchmark_statement *statement,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iteration
);
static int bind_write_scenario_parameters(
    struct benchmark_statement *statement,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iteration
);
static int bind_foreign_key_scenario_parameters(
    struct benchmark_statement *statement,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iteration
);
static int bind_id_range_parameters(
    struct benchmark_statement *statement,
    size_t iteration,
    const struct benchmark_dataset *dataset,
    unsigned int basis_points
);
static int consume_statement(
    struct benchmark_database *database,
    struct benchmark_statement *statement,
    bool include_affected_rows,
    struct benchmark_measurement *measurement
);
static int consume_expected_error(
    struct benchmark_database *database,
    struct benchmark_statement *statement,
    struct benchmark_measurement *measurement
);
static int consume_mylite_statement(
    struct benchmark_database *database,
    struct benchmark_statement *statement,
    bool include_affected_rows,
    struct benchmark_measurement *measurement
);
static int consume_sqlite_statement(
    struct benchmark_statement *statement,
    bool include_affected_rows,
    struct benchmark_measurement *measurement
);
static void print_sample(
    FILE *output,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    enum benchmark_engine_kind engine,
    size_t sample,
    size_t iterations,
    const struct benchmark_measurement *measurement
);
static void print_summary(
    FILE *output,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iterations,
    const struct benchmark_sample_pair *samples,
    size_t sample_count
);
static int verify_sample_pair(
    const struct benchmark_scenario *scenario,
    size_t sample,
    const struct benchmark_sample_pair *pair
);
static int begin_transaction(struct benchmark_database *database);
static int commit_transaction(struct benchmark_database *database);
static int rollback_transaction(struct benchmark_database *database);
static int prepare_statement(
    struct benchmark_database *database,
    const char *sql,
    struct benchmark_statement *out_statement
);
static int reset_statement(struct benchmark_statement *statement);
static int finalize_statement(struct benchmark_statement *statement);
static int bind_int64(struct benchmark_statement *statement, size_t index, int64_t value);
static int bind_null(struct benchmark_statement *statement, size_t index);
static int bind_text(struct benchmark_statement *statement, size_t index, const char *value);
static int execute_sql(struct benchmark_database *database, const char *sql);
static int fetch_scalar_count(
    struct benchmark_database *database,
    const char *sql,
    uint64_t *out_value
);
static int seed_accounts(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset
);
static int seed_items(struct benchmark_database *database, const struct benchmark_dataset *dataset);
static int seed_tags(struct benchmark_database *database, const struct benchmark_dataset *dataset);
static int seed_support_tables(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset,
    struct benchmark_load_measurement *measurement
);
static int seed_upsert_and_composite_parents(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset
);
static int seed_fanout_tables(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset
);
static int seed_restrict_tables(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset
);
static int seed_set_null_tables(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset
);
static int insert_account(struct benchmark_statement *statement, size_t account_id, size_t region);
static int insert_item(
    struct benchmark_statement *statement,
    size_t item_id,
    const struct benchmark_dataset *dataset
);
static int insert_tag(
    struct benchmark_statement *statement,
    size_t item_id,
    const struct benchmark_dataset *dataset
);
static int step_write_statement(struct benchmark_statement *statement);
static void close_benchmark_database(struct benchmark_database *database);
static void remove_database_files(const char *path);
static uint64_t file_size(const char *path);
static uint64_t monotonic_now_ns(void);
static uint64_t process_cpu_now_ns(void);
static uint64_t hash_bytes(uint64_t hash, const void *bytes, size_t size);
static void hash_uint64(uint64_t *hash, uint64_t value);
static double median_elapsed_us(
    enum benchmark_engine_kind engine,
    const struct benchmark_sample_pair *samples,
    size_t sample_count
);
static int compare_double(const void *left, const void *right);
static const char *engine_name(enum benchmark_engine_kind kind);
static const char *mode_name(enum benchmark_execution_mode mode);
static size_t account_count_for_rows(size_t row_count);
static size_t tag_count_for_rows(size_t row_count);
static size_t fanout_parent_count_for_rows(size_t row_count);
static size_t range_count_for_basis_points(size_t row_count, size_t basis_points);
static bool scenario_includes_affected_rows(const struct benchmark_scenario *scenario);
static bool scenario_uses_rollback(const struct benchmark_scenario *scenario);
static size_t scenario_iterations(
    const struct benchmark_scenario *scenario,
    const struct benchmark_options *options
);
static int parse_size(const char *text, bool allow_zero, size_t *out_value);
static const char *option_value(int argc, char **argv, int *index);
static const char *default_database_directory(void);
static long benchmark_process_id(void);

static const struct benchmark_scenario benchmark_scenarios[] = {
    {
        "point_lookup_prepare_each",
        benchmark_scenario_point_lookup_prepare_each,
        benchmark_execution_prepare_each,
        "SELECT id, account_id, score FROM items WHERE id = ?",
        1000,
    },
    {
        "point_lookup_prepared",
        benchmark_scenario_point_lookup_prepared,
        benchmark_execution_prepared,
        "SELECT id, account_id, score FROM items WHERE id = ?",
        5000,
    },
    {
        "secondary_lookup",
        benchmark_scenario_secondary_lookup,
        benchmark_execution_prepared,
        "SELECT id, created_at FROM items WHERE account_id = ? "
        "ORDER BY created_at DESC, id DESC LIMIT 20",
        500,
    },
    {
        "range_aggregate",
        benchmark_scenario_range_aggregate,
        benchmark_execution_prepared,
        "SELECT COUNT(*), SUM(score) FROM items "
        "WHERE category_id = ? AND score BETWEEN ? AND ?",
        100,
    },
    {
        "full_scan_expression",
        benchmark_scenario_full_scan_expression,
        benchmark_execution_prepared,
        "SELECT COUNT(*), SUM(score) FROM items "
        "WHERE (score * 115 + category_id) > 10000000",
        3,
    },
    {
        "text_expression",
        benchmark_scenario_text_expression,
        benchmark_execution_prepared,
        "SELECT id, LENGTH(title) FROM items "
        "WHERE status = ? AND title LIKE 'item-00000%' ORDER BY id",
        3,
    },
    {
        "group_aggregate",
        benchmark_scenario_group_aggregate,
        benchmark_execution_prepared,
        "SELECT category_id, COUNT(*), SUM(score) FROM items "
        "GROUP BY category_id ORDER BY category_id",
        3,
    },
    {
        "indexed_order_limit",
        benchmark_scenario_indexed_order_limit,
        benchmark_execution_prepared,
        "SELECT id, created_at FROM items WHERE status = ? "
        "ORDER BY created_at DESC, id DESC LIMIT 100",
        100,
    },
    {
        "parent_join",
        benchmark_scenario_parent_join,
        benchmark_execution_prepared,
        "SELECT a.region, COUNT(*), SUM(i.score) FROM accounts AS a "
        "JOIN items AS i ON i.account_id = a.id "
        "WHERE a.region = ? GROUP BY a.region",
        20,
    },
    {
        "bridge_join",
        benchmark_scenario_bridge_join,
        benchmark_execution_prepared,
        "SELECT t.item_id, i.score + t.weight FROM item_tags AS t "
        "JOIN items AS i ON i.id = t.item_id WHERE t.tag_id BETWEEN ? AND ? "
        "ORDER BY t.tag_id, t.item_id",
        20,
    },
    {
        "correlated_exists",
        benchmark_scenario_correlated_exists,
        benchmark_execution_prepared,
        "SELECT a.id FROM accounts AS a WHERE EXISTS "
        "(SELECT 1 FROM items AS i WHERE i.account_id = a.id AND i.score > ?) ORDER BY a.id",
        10,
    },
    {
        "indexed_update_rollback",
        benchmark_scenario_indexed_update,
        benchmark_execution_write_rollback,
        "UPDATE items SET score = score + 1 WHERE id = ?",
        500,
    },
    {
        "foreign_key_insert_rollback",
        benchmark_scenario_foreign_key_insert,
        benchmark_execution_write_rollback,
        "INSERT INTO write_log (id, item_id, note) VALUES (?, ?, ?)",
        500,
    },
    {
        "foreign_key_cascade_rollback",
        benchmark_scenario_foreign_key_cascade,
        benchmark_execution_write_rollback,
        "DELETE FROM items WHERE id = ?",
        5,
    },
    {
        "selectivity_zero",
        benchmark_scenario_selectivity_zero,
        benchmark_execution_prepared,
        "SELECT COUNT(*), SUM(score) FROM items WHERE id BETWEEN ? AND ?",
        100,
    },
    {
        "selectivity_one",
        benchmark_scenario_selectivity_one,
        benchmark_execution_prepared,
        "SELECT COUNT(*), SUM(score) FROM items WHERE id BETWEEN ? AND ?",
        100,
    },
    {
        "selectivity_001pct",
        benchmark_scenario_selectivity_one_basis_point,
        benchmark_execution_prepared,
        "SELECT COUNT(*), SUM(score) FROM items WHERE id BETWEEN ? AND ?",
        50,
    },
    {
        "selectivity_1pct",
        benchmark_scenario_selectivity_one_percent,
        benchmark_execution_prepared,
        "SELECT COUNT(*), SUM(score) FROM items WHERE id BETWEEN ? AND ?",
        20,
    },
    {
        "selectivity_10pct",
        benchmark_scenario_selectivity_ten_percent,
        benchmark_execution_prepared,
        "SELECT COUNT(*), SUM(score) FROM items WHERE id BETWEEN ? AND ?",
        5,
    },
    {
        "selectivity_full",
        benchmark_scenario_selectivity_full,
        benchmark_execution_prepared,
        "SELECT COUNT(*), SUM(score) FROM items WHERE id BETWEEN ? AND ?",
        3,
    },
    {
        "covering_composite_range",
        benchmark_scenario_covering_range,
        benchmark_execution_prepared,
        "SELECT id, score FROM items WHERE category_id = ? AND score BETWEEN ? AND ? "
        "ORDER BY score, id LIMIT 100",
        50,
    },
    {
        "noncovering_composite_range",
        benchmark_scenario_noncovering_range,
        benchmark_execution_prepared,
        "SELECT id, title FROM items WHERE category_id = ? AND score BETWEEN ? AND ? "
        "ORDER BY score, id LIMIT 100",
        50,
    },
    {
        "indexed_null_lookup",
        benchmark_scenario_null_lookup,
        benchmark_execution_prepared,
        "SELECT COUNT(*), SUM(score) FROM items WHERE optional_value IS NULL",
        5,
    },
    {
        "primary_key_or_lookup",
        benchmark_scenario_or_lookup,
        benchmark_execution_prepared,
        "SELECT id, score FROM items WHERE id = ? OR id = ? OR id = ? ORDER BY id",
        200,
    },
    {
        "deep_offset_90pct",
        benchmark_scenario_deep_offset,
        benchmark_execution_prepared,
        "SELECT id, score FROM items ORDER BY id LIMIT ? OFFSET ?",
        10,
    },
    {
        "unindexed_sort_top_1000",
        benchmark_scenario_unindexed_sort_limit,
        benchmark_execution_prepared,
        "SELECT id, payload FROM items ORDER BY payload, id LIMIT 1000",
        3,
    },
    {
        "high_cardinality_group",
        benchmark_scenario_high_cardinality_group,
        benchmark_execution_prepared,
        "SELECT score, COUNT(*) FROM items GROUP BY score ORDER BY score",
        1,
    },
    {
        "high_cardinality_distinct",
        benchmark_scenario_high_cardinality_distinct,
        benchmark_execution_prepared,
        "SELECT DISTINCT score FROM items ORDER BY score",
        1,
    },
    {
        "window_partition_rank",
        benchmark_scenario_window_partition,
        benchmark_execution_prepared,
        "SELECT id, ROW_NUMBER() OVER (PARTITION BY account_id ORDER BY score DESC) "
        "FROM items WHERE account_id BETWEEN ? AND ? ORDER BY account_id, score DESC, id",
        3,
    },
    {
        "three_table_join",
        benchmark_scenario_three_table_join,
        benchmark_execution_prepared,
        "SELECT a.region, COUNT(*), SUM(i.score + t.weight) FROM accounts AS a "
        "JOIN items AS i ON i.account_id = a.id "
        "JOIN item_tags AS t ON t.item_id = i.id "
        "WHERE a.region = ? GROUP BY a.region",
        10,
    },
    {
        "left_join_range",
        benchmark_scenario_left_join,
        benchmark_execution_prepared,
        "SELECT i.id, t.item_id FROM items AS i "
        "LEFT JOIN item_tags AS t ON t.item_id = i.id WHERE i.id BETWEEN ? AND ? "
        "ORDER BY i.id, t.item_id",
        10,
    },
    {
        "anti_join",
        benchmark_scenario_anti_join,
        benchmark_execution_prepared,
        "SELECT i.id FROM items AS i WHERE NOT EXISTS "
        "(SELECT 1 FROM item_tags AS t WHERE t.item_id = i.id) ORDER BY i.id",
        3,
    },
    {
        "large_large_bounded_join",
        benchmark_scenario_large_large_join,
        benchmark_execution_prepared,
        "SELECT i.id, i.score + t.weight FROM items AS i "
        "JOIN item_tags AS t ON t.item_id = i.id WHERE i.id BETWEEN ? AND ? "
        "ORDER BY i.id, t.tag_id",
        10,
    },
    {
        "skew_hot_tenant",
        benchmark_scenario_skew_hot,
        benchmark_execution_prepared,
        "SELECT COUNT(*), SUM(score) FROM items WHERE tenant_id = ?",
        5,
    },
    {
        "skew_cold_tenant",
        benchmark_scenario_skew_cold,
        benchmark_execution_prepared,
        "SELECT COUNT(*), SUM(score) FROM items WHERE tenant_id = ?",
        100,
    },
    {
        "update_001pct_rollback",
        benchmark_scenario_update_one_basis_point,
        benchmark_execution_write_rollback,
        "UPDATE items SET score = score + 1 WHERE id BETWEEN ? AND ?",
        3,
    },
    {
        "update_1pct_rollback",
        benchmark_scenario_update_one_percent,
        benchmark_execution_write_rollback,
        "UPDATE items SET score = score + 1 WHERE id BETWEEN ? AND ?",
        1,
    },
    {
        "update_10pct_rollback",
        benchmark_scenario_update_ten_percent,
        benchmark_execution_write_rollback,
        "UPDATE items SET score = score + 1 WHERE id BETWEEN ? AND ?",
        1,
    },
    {
        "delete_001pct_rollback",
        benchmark_scenario_delete_one_basis_point,
        benchmark_execution_write_rollback,
        "DELETE FROM items WHERE id BETWEEN ? AND ?",
        1,
    },
    {
        "delete_1pct_rollback",
        benchmark_scenario_delete_one_percent,
        benchmark_execution_write_rollback,
        "DELETE FROM items WHERE id BETWEEN ? AND ?",
        1,
    },
    {
        "upsert_hit_rollback",
        benchmark_scenario_upsert_hit,
        benchmark_execution_write_rollback,
        "INSERT INTO upsert_targets (id, value_text) VALUES (?, ?) "
        "ON DUPLICATE KEY UPDATE value_text = VALUES(value_text)",
        200,
    },
    {
        "upsert_miss_rollback",
        benchmark_scenario_upsert_miss,
        benchmark_execution_write_rollback,
        "INSERT INTO upsert_targets (id, value_text) VALUES (?, ?) "
        "ON DUPLICATE KEY UPDATE value_text = VALUES(value_text)",
        200,
    },
    {
        "insert_zero_indexes_rollback",
        benchmark_scenario_insert_index_zero,
        benchmark_execution_write_rollback,
        "INSERT INTO insert_index_0 (id, value_a, value_b) VALUES (?, ?, ?)",
        500,
    },
    {
        "insert_one_index_rollback",
        benchmark_scenario_insert_index_one,
        benchmark_execution_write_rollback,
        "INSERT INTO insert_index_1 (id, value_a, value_b) VALUES (?, ?, ?)",
        500,
    },
    {
        "insert_five_indexes_rollback",
        benchmark_scenario_insert_index_five,
        benchmark_execution_write_rollback,
        "INSERT INTO insert_index_5 (id, value_a, value_b) VALUES (?, ?, ?)",
        500,
    },
    {
        "insert_ten_indexes_rollback",
        benchmark_scenario_insert_index_ten,
        benchmark_execution_write_rollback,
        "INSERT INTO insert_index_10 (id, value_a, value_b) VALUES (?, ?, ?)",
        500,
    },
    {
        "insert_batch_10_rollback",
        benchmark_scenario_insert_batch_ten,
        benchmark_execution_write_rollback,
        "INSERT INTO insert_index_1 (id, value_a, value_b) VALUES "
        "(?,?,?),(?,?,?),(?,?,?),(?,?,?),(?,?,?),"
        "(?,?,?),(?,?,?),(?,?,?),(?,?,?),(?,?,?)",
        100,
    },
    {
        "load_data_zero_indexes_rollback",
        benchmark_scenario_load_data_index_zero,
        benchmark_execution_bulk_import_rollback,
        NULL,
        10000,
    },
    {
        "load_data_five_indexes_rollback",
        benchmark_scenario_load_data_index_five,
        benchmark_execution_bulk_import_rollback,
        NULL,
        10000,
    },
    {
        "composite_foreign_key_insert_rollback",
        benchmark_scenario_composite_foreign_key_insert,
        benchmark_execution_write_rollback,
        "INSERT INTO composite_children (id, parent_id, parent_shard, payload) VALUES (?, ?, ?, ?)",
        200,
    },
    {
        "composite_foreign_key_invalid_rollback",
        benchmark_scenario_composite_foreign_key_invalid,
        benchmark_execution_expected_error_rollback,
        "INSERT INTO composite_children (id, parent_id, parent_shard, payload) VALUES (?, ?, ?, ?)",
        50,
    },
    {
        "foreign_key_cascade_fanout_rollback",
        benchmark_scenario_foreign_key_cascade_fanout,
        benchmark_execution_write_rollback,
        "DELETE FROM fanout_parents WHERE id = ?",
        1,
    },
    {
        "foreign_key_restrict_miss_rollback",
        benchmark_scenario_foreign_key_restrict,
        benchmark_execution_write_rollback,
        "DELETE FROM restrict_parents WHERE id = ?",
        100,
    },
    {
        "foreign_key_set_null_fanout_rollback",
        benchmark_scenario_foreign_key_set_null,
        benchmark_execution_write_rollback,
        "DELETE FROM set_null_parents WHERE id = ?",
        1,
    },
    {
        "result_narrow_100",
        benchmark_scenario_result_narrow_small,
        benchmark_execution_prepared,
        "SELECT id FROM items ORDER BY id LIMIT 100",
        20,
    },
    {
        "result_narrow_10000",
        benchmark_scenario_result_narrow_medium,
        benchmark_execution_prepared,
        "SELECT id FROM items ORDER BY id LIMIT 10000",
        3,
    },
    {
        "result_narrow_full",
        benchmark_scenario_result_narrow_full,
        benchmark_execution_prepared,
        "SELECT id FROM items ORDER BY id",
        1,
    },
    {
        "result_wide_100",
        benchmark_scenario_result_wide_small,
        benchmark_execution_prepared,
        "SELECT id, account_id, category_id, status, score, created_at, title, payload "
        "FROM items ORDER BY id LIMIT 100",
        20,
    },
    {
        "result_wide_10000",
        benchmark_scenario_result_wide_medium,
        benchmark_execution_prepared,
        "SELECT id, account_id, category_id, status, score, created_at, title, payload "
        "FROM items ORDER BY id LIMIT 10000",
        3,
    },
};

int main(int argc, char **argv) {
    struct benchmark_options options = {
        .row_count = default_row_count,
        .sample_count = default_sample_count,
        .warmup_iterations = default_warmup_iterations,
        .iteration_override = 0U,
        .scenario_name = NULL,
        .database_directory = NULL,
        .database_base = NULL,
        .output_path = NULL,
        .attribution_layer_name = NULL,
        .keep_databases = false,
        .reuse_databases = false,
        .seed_only = false,
        .attribution_seed = false,
        .attribution_timing = false,
        .analyze = false,
        .list_scenarios = false,
        .show_help = false,
    };

    if (parse_options(argc, argv, &options) != 0) {
        print_usage(argv[0], stderr);
        return 1;
    }
    if (options.show_help) {
        print_usage(argv[0], stdout);
        return 0;
    }
    if (options.list_scenarios) {
        print_scenarios(stdout);
        return 0;
    }
    if (options.reuse_databases && options.database_base == NULL) {
        fprintf(stderr, "--reuse-databases requires --database-base\n");
        return 1;
    }
    if (options.attribution_seed && options.attribution_timing) {
        fprintf(stderr, "--attribution-seed and --attribution-timing are mutually exclusive\n");
        return 1;
    }
    if (options.attribution_layer_name != NULL && !options.attribution_seed &&
        !options.attribution_timing) {
        fprintf(stderr, "--attribution-layer requires an attribution mode\n");
        return 1;
    }
    if (options.attribution_seed) {
#ifndef MYLITE_ENABLE_PROFILING
        fprintf(stderr, "--attribution-seed requires MYLITE_ENABLE_PROFILING=ON\n");
        return 1;
#endif
        return run_attribution(&options);
    }
    if (options.attribution_timing) {
#ifdef MYLITE_ENABLE_PROFILING
        fprintf(stderr, "--attribution-timing requires MYLITE_ENABLE_PROFILING=OFF\n");
        return 1;
#endif
        return run_attribution(&options);
    }
    return run_benchmark(&options);
}

static int parse_options(int argc, char **argv, struct benchmark_options *out_options) {
    for (int index = 1; index < argc; ++index) {
        const char *argument = argv[index];
        const char *value = NULL;

        if (parse_flag_option(argument, out_options)) {
            continue;
        }
        if (!option_takes_value(argument)) {
            fprintf(stderr, "unknown argument: %s\n", argument);
            return 1;
        }
        value = option_value(argc, argv, &index);
        if (value == NULL || parse_named_option(argument, out_options, value) != 0) {
            fprintf(stderr, "invalid or missing %s value\n", argument);
            return 1;
        }
    }
    return 0;
}

static bool parse_flag_option(const char *argument, struct benchmark_options *out_options) {
    if (strcmp(argument, "--help") == 0) {
        out_options->show_help = true;
        return true;
    }
    if (strcmp(argument, "--list") == 0) {
        out_options->list_scenarios = true;
        return true;
    }
    if (strcmp(argument, "--keep-databases") == 0) {
        out_options->keep_databases = true;
        return true;
    }
    if (strcmp(argument, "--reuse-databases") == 0) {
        out_options->reuse_databases = true;
        out_options->keep_databases = true;
        return true;
    }
    if (strcmp(argument, "--seed-only") == 0) {
        out_options->seed_only = true;
        out_options->keep_databases = true;
        return true;
    }
    if (strcmp(argument, "--attribution-seed") == 0) {
        out_options->attribution_seed = true;
        return true;
    }
    if (strcmp(argument, "--attribution-timing") == 0) {
        out_options->attribution_timing = true;
        return true;
    }
    if (strcmp(argument, "--analyze") == 0) {
        out_options->analyze = true;
        return true;
    }
    return false;
}

static bool option_takes_value(const char *argument) {
    return strcmp(argument, "--rows") == 0 || strcmp(argument, "--samples") == 0 ||
           strcmp(argument, "--warmup") == 0 || strcmp(argument, "--iterations") == 0 ||
           strcmp(argument, "--scenario") == 0 || strcmp(argument, "--database-dir") == 0 ||
           strcmp(argument, "--database-base") == 0 || strcmp(argument, "--output") == 0 ||
           strcmp(argument, "--attribution-layer") == 0;
}

static int parse_named_option(
    const char *argument,
    struct benchmark_options *out_options,
    const char *value
) {
    if (strcmp(argument, "--rows") == 0) {
        return parse_size(value, false, &out_options->row_count);
    }
    if (strcmp(argument, "--samples") == 0) {
        return parse_size(value, false, &out_options->sample_count);
    }
    if (strcmp(argument, "--warmup") == 0) {
        return parse_size(value, true, &out_options->warmup_iterations);
    }
    if (strcmp(argument, "--iterations") == 0) {
        return parse_size(value, false, &out_options->iteration_override);
    }
    if (strcmp(argument, "--scenario") == 0) {
        out_options->scenario_name = value;
    } else if (strcmp(argument, "--database-dir") == 0) {
        out_options->database_directory = value;
    } else if (strcmp(argument, "--database-base") == 0) {
        out_options->database_base = value;
    } else if (strcmp(argument, "--output") == 0) {
        out_options->output_path = value;
    } else if (strcmp(argument, "--attribution-layer") == 0) {
        out_options->attribution_layer_name = value;
    } else {
        return 1;
    }
    return 0;
}

static void print_usage(const char *program_name, FILE *stream) {
    fprintf(
        stream,
        "usage: %s [--rows N] [--samples N] [--warmup N] [--iterations N]\n"
        "          [--scenario NAME] [--database-dir PATH] [--database-base PATH]\n"
        "          [--output PATH] [--attribution-layer LAYER]\n"
        "          [--keep-databases] [--reuse-databases]\n"
        "          [--seed-only] [--attribution-seed] [--attribution-timing]\n"
        "          [--analyze] [--list] [--help]\n",
        program_name
    );
}

static void print_scenarios(FILE *stream) {
    for (size_t index = 0U; index < sizeof(benchmark_scenarios) / sizeof(benchmark_scenarios[0]);
         ++index) {
        fprintf(
            stream,
            "%s\t%s\t%zu\n",
            benchmark_scenarios[index].name,
            mode_name(benchmark_scenarios[index].mode),
            benchmark_scenarios[index].default_iterations
        );
    }
}

static int run_benchmark(const struct benchmark_options *options) {
    struct benchmark_paths paths = {{0}, {0}};
    struct benchmark_database mylite = {0};
    struct benchmark_database sqlite = {0};
    struct benchmark_load_measurement mylite_load = {0};
    struct benchmark_load_measurement sqlite_load = {0};
    const struct benchmark_dataset dataset = {
        .row_count = options->row_count,
        .account_count = account_count_for_rows(options->row_count),
        .tag_count = tag_count_for_rows(options->row_count),
    };
    FILE *output = stdout;
    int result = 1;

    if (options->output_path != NULL) {
        output = fopen(options->output_path, "wb");
        if (output == NULL) {
            fprintf(
                stderr,
                "failed to open output %s: %s\n",
                options->output_path,
                strerror(errno)
            );
            return 1;
        }
    }
    if (prepare_benchmark_databases(
            options,
            &paths,
            &dataset,
            &mylite,
            &sqlite,
            &mylite_load,
            &sqlite_load
        ) != 0) {
        goto cleanup;
    }
    print_csv_header(output);
    if (!options->reuse_databases) {
        print_load_measurements(output, &dataset, &mylite_load, &sqlite_load);
    }
    fprintf(
        output,
        "size,database_file,%zu,mylite,setup,0,1,0,%" PRIu64 ",0,0.000,0.000,0.000,0.000\n",
        dataset.row_count,
        file_size(paths.mylite)
    );
    fprintf(
        output,
        "size,database_file,%zu,sqlite,setup,0,1,0,%" PRIu64 ",0,0.000,0.000,0.000,0.000\n",
        dataset.row_count,
        file_size(paths.sqlite)
    );
    if (!options->seed_only && run_scenarios(output, &mylite, &sqlite, &dataset, options) != 0) {
        goto cleanup;
    }
    result = 0;

cleanup:
    close_benchmark_database(&sqlite);
    close_benchmark_database(&mylite);
    if (!options->keep_databases && !options->reuse_databases && !options->seed_only) {
        remove_database_files(paths.sqlite);
        remove_database_files(paths.mylite);
    } else if (paths.mylite[0] != '\0') {
        fprintf(stderr, "large-dataset: kept %s and %s\n", paths.mylite, paths.sqlite);
    }
    if (output != stdout) {
        if (fclose(output) != 0) {
            result = 1;
        }
    }
    return result;
}

static int run_attribution(const struct benchmark_options *options) {
    struct attribution_program_set programs = {0};
    const struct benchmark_dataset dataset = {
        .row_count = options->row_count,
        .account_count = account_count_for_rows(options->row_count),
        .tag_count = tag_count_for_rows(options->row_count),
    };
    FILE *output = NULL;
    int result = 1;

    if (!attribution_options_are_valid(options)) {
        return 1;
    }
    output = open_attribution_output(options);
    if (output == NULL) {
        return 1;
    }
    if (discover_attribution_programs(options, &dataset, &programs) != 0) {
        goto cleanup;
    }
    print_attribution_header(output, &dataset, &programs);
    if (run_attribution_samples(options, &dataset, &programs, output) != 0) {
        goto cleanup;
    }
    result = 0;

cleanup:
    deinit_attribution_programs(&programs);
    if (output != stdout && fclose(output) != 0) {
        result = 1;
    }
    return result;
}

static bool attribution_options_are_valid(const struct benchmark_options *options) {
    enum benchmark_engine_kind selected_layer = benchmark_engine_sqlite;

    if (options->reuse_databases || options->seed_only || options->analyze ||
        options->scenario_name != NULL || options->iteration_override != 0U ||
        options->warmup_iterations != 0U) {
        fprintf(
            stderr,
            "attribution requires --warmup 0 and cannot use scenario/database reuse flags\n"
        );
        return false;
    }
    if (options->attribution_layer_name != NULL &&
        !attribution_layer_from_name(options->attribution_layer_name, &selected_layer)) {
        fprintf(stderr, "unknown attribution layer: %s\n", options->attribution_layer_name);
        return false;
    }
    return true;
}

static FILE *open_attribution_output(const struct benchmark_options *options) {
    FILE *output = stdout;

    if (options->output_path == NULL) {
        return output;
    }
    output = fopen(options->output_path, "wb");
    if (output == NULL) {
        fprintf(stderr, "failed to open output %s: %s\n", options->output_path, strerror(errno));
    }
    return output;
}

static void print_attribution_header(
    FILE *output,
    const struct benchmark_dataset *dataset,
    const struct attribution_program_set *programs
) {
    fprintf(
        output,
        "record,rows,sample,layer,phase,logical_table,physical_table,parameters,guarded,"
        "hash,total_ms,process_cpu_ms,sqlite_steps,vm_steps,fullscan_steps,sorts,autoindexes,"
        "reprepares,"
        "runs,filter_hits,filter_misses,metadata_vm_steps,scalar_callbacks,scalar_ms,"
        "collation_callbacks,collation_ms,mylite_allocations,mylite_allocation_bytes,"
        "dml_plans,dml_plan_hits,statement_cache_hits,statement_cache_misses\n"
    );
    for (size_t index = 0U; index < attribution_table_count; ++index) {
        const struct attribution_program *program = &programs->programs[index];

        if (!program->is_seeded) {
            continue;
        }
        fprintf(
            output,
            "program,%zu,0,discovery,program,%s,%s,%zu,%d,%" PRIu64
            ",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\n",
            dataset->row_count,
            program->logical_table,
            program->physical_table,
            program->parameter_count,
            program->has_guard ? 1 : 0,
            program->sql_hash
        );
    }
}

static int run_attribution_samples(
    const struct benchmark_options *options,
    const struct benchmark_dataset *dataset,
    const struct attribution_program_set *programs,
    FILE *output
) {
    for (size_t sample = 0U; sample < options->sample_count; ++sample) {
        if (run_attribution_sample(options, dataset, programs, output, sample) != 0) {
            return 1;
        }
    }
    return 0;
}

static int run_attribution_sample(
    const struct benchmark_options *options,
    const struct benchmark_dataset *dataset,
    const struct attribution_program_set *programs,
    FILE *output,
    size_t sample
) {
    static const enum benchmark_engine_kind layers[attribution_layer_count] = {
        benchmark_engine_sqlite,
        benchmark_engine_mylite_physical,
        benchmark_engine_mylite_guarded,
        benchmark_engine_mylite,
    };

    uint64_t checksums[attribution_layer_count][attribution_table_count] = {{0}};
    size_t rotation = (sample / 2U) % attribution_layer_count;
    bool reverse = sample % 2U != 0U;

    if (options->attribution_layer_name != NULL) {
        enum benchmark_engine_kind kind = benchmark_engine_sqlite;
        char suffix[generated_text_capacity];
        char path[path_capacity];
        int written = 0;

        if (!attribution_layer_from_name(options->attribution_layer_name, &kind)) {
            return 1;
        }
        written =
            snprintf(suffix, sizeof(suffix), "attribution-%zu-%s", sample + 1U, engine_name(kind));
        return written < 0 || (size_t)written >= sizeof(suffix) ||
                       make_attribution_path(options, suffix, path, sizeof(path)) != 0 ||
                       run_attribution_layer(
                           options,
                           dataset,
                           programs,
                           kind,
                           path,
                           output,
                           sample + 1U,
                           checksums[0]
                       ) != 0
                   ? 1
                   : 0;
    }
    for (size_t position = 0U; position < attribution_layer_count; ++position) {
        size_t layer_index = reverse ? ((attribution_layer_count - 1U) + rotation - position) %
                                           attribution_layer_count
                                     : (position + rotation) % attribution_layer_count;
        enum benchmark_engine_kind kind = layers[layer_index];
        char suffix[generated_text_capacity];
        char path[path_capacity];
        int written =
            snprintf(suffix, sizeof(suffix), "attribution-%zu-%s", sample + 1U, engine_name(kind));

        if (written < 0 || (size_t)written >= sizeof(suffix) ||
            make_attribution_path(options, suffix, path, sizeof(path)) != 0 ||
            run_attribution_layer(
                options,
                dataset,
                programs,
                kind,
                path,
                output,
                sample + 1U,
                checksums[layer_index]
            ) != 0) {
            return 1;
        }
    }
    return verify_attribution_checksums(sample + 1U, checksums);
}

static bool attribution_layer_from_name(const char *name, enum benchmark_engine_kind *out_kind) {
    static const enum benchmark_engine_kind layers[attribution_layer_count] = {
        benchmark_engine_sqlite,
        benchmark_engine_mylite_physical,
        benchmark_engine_mylite_guarded,
        benchmark_engine_mylite,
    };

    if (name == NULL || out_kind == NULL) {
        return false;
    }
    for (size_t index = 0U; index < attribution_layer_count; ++index) {
        if (strcmp(name, engine_name(layers[index])) == 0) {
            *out_kind = layers[index];
            return true;
        }
    }
    return false;
}

static int discover_attribution_programs(
    const struct benchmark_options *options,
    const struct benchmark_dataset *dataset,
    struct attribution_program_set *out_programs
) {
    const struct benchmark_dataset discovery_dataset = {
        .row_count = minimum_account_count,
        .account_count = account_count_for_rows(minimum_account_count),
        .tag_count = tag_count_for_rows(minimum_account_count),
    };
    const struct attribution_program initial_programs[attribution_table_count] = {
        [attribution_table_accounts] = {.logical_table = "accounts", .is_seeded = true},
        [attribution_table_items] = {.logical_table = "items", .is_seeded = true},
        [attribution_table_item_tags] = {.logical_table = "item_tags", .is_seeded = true},
        [attribution_table_write_log] = {.logical_table = "write_log"},
        [attribution_table_upsert_targets] =
            {
                .logical_table = "upsert_targets",
                .is_seeded = true,
            },
        [attribution_table_composite_parents] =
            {
                .logical_table = "composite_parents",
                .is_seeded = true,
            },
        [attribution_table_composite_children] = {.logical_table = "composite_children"},
        [attribution_table_fanout_parents] =
            {
                .logical_table = "fanout_parents",
                .is_seeded = true,
            },
        [attribution_table_fanout_children] =
            {
                .logical_table = "fanout_children",
                .is_seeded = true,
            },
        [attribution_table_restrict_parents] =
            {
                .logical_table = "restrict_parents",
                .is_seeded = true,
            },
        [attribution_table_restrict_children] =
            {
                .logical_table = "restrict_children",
                .is_seeded = true,
            },
        [attribution_table_set_null_parents] =
            {
                .logical_table = "set_null_parents",
                .is_seeded = true,
            },
        [attribution_table_set_null_children] =
            {
                .logical_table = "set_null_children",
                .is_seeded = true,
            },
    };
    struct benchmark_database discovery = {0};
    struct benchmark_load_measurement measurement = {0};
    char path[path_capacity];
    int result = 1;

    (void)dataset;
    memset(out_programs, 0, sizeof(*out_programs));
    memcpy(out_programs->programs, initial_programs, sizeof(initial_programs));
    if (make_attribution_path(options, "attribution-discovery", path, sizeof(path)) != 0) {
        return 1;
    }
    remove_database_files(path);
    if (open_benchmark_database(benchmark_engine_mylite, path, &discovery) != 0) {
        goto cleanup;
    }
    discovery.profile_seed = false;
    if (create_benchmark_schema(&discovery) != 0 ||
        load_attribution_physical_names(discovery.sqlite, out_programs) != 0 ||
        sqlite3_trace_v2(
            discovery.sqlite,
            SQLITE_TRACE_STMT,
            attribution_trace_callback,
            out_programs
        ) != SQLITE_OK ||
        seed_benchmark_database(&discovery, &discovery_dataset, &measurement) != 0) {
        goto cleanup;
    }
    (void)sqlite3_trace_v2(discovery.sqlite, 0U, NULL, NULL);
    if (validate_attribution_programs(out_programs) != 0) {
        goto cleanup;
    }
    result = 0;

cleanup:
    if (discovery.sqlite != NULL) {
        (void)sqlite3_trace_v2(discovery.sqlite, 0U, NULL, NULL);
    }
    close_benchmark_database(&discovery);
    remove_database_files(path);
    return result;
}

static int run_attribution_layer(
    const struct benchmark_options *options,
    const struct benchmark_dataset *dataset,
    const struct attribution_program_set *programs,
    enum benchmark_engine_kind kind,
    const char *path,
    FILE *output,
    size_t sample,
    uint64_t out_checksums[attribution_table_count]
) {
    struct benchmark_database database = {0};
    struct benchmark_load_measurement measurement = {0};
    int result = 1;

    remove_database_files(path);
    if (open_benchmark_database(kind, path, &database) != 0) {
        goto cleanup;
    }
    database.attribution_programs = programs;
    database.profile_seed = options->attribution_seed;
    if (create_benchmark_schema(&database) != 0 ||
        (engine_uses_mylite_storage(kind) &&
         validate_attribution_layout(database.sqlite, programs) != 0) ||
        seed_benchmark_database(&database, dataset, &measurement) != 0 ||
        verify_attribution_database(&database, dataset, out_checksums) != 0) {
        goto cleanup;
    }
    print_attribution_measurement(output, dataset, kind, sample, &measurement, out_checksums);
    result = 0;

cleanup:
    close_benchmark_database(&database);
    if (!options->keep_databases) {
        remove_database_files(path);
    } else if (result == 0) {
        fprintf(stderr, "large-dataset: kept %s\n", path);
    }
    return result;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters): SQLite fixes this callback signature.
static int attribution_trace_callback(
    unsigned int event,
    void *context,
    void *statement_pointer,
    void *sql_pointer
)
// NOLINTEND(bugprone-easily-swappable-parameters)
{
    struct attribution_program_set *programs = context;
    sqlite3_stmt *statement = statement_pointer;
    const char *sql = sqlite3_sql(statement);

    (void)sql_pointer;
    if (event != SQLITE_TRACE_STMT || sql == NULL ||
        strncmp(sql, "INSERT INTO ", attribution_insert_prefix_size) != 0) {
        return 0;
    }
    for (size_t index = 0U; index < attribution_table_count; ++index) {
        struct attribution_program *program = &programs->programs[index];
        char prefix[MYLITE_CATALOG_IDENTIFIER_CAPACITY + attribution_identifier_sql_extra];
        int written =
            snprintf(prefix, sizeof(prefix), "INSERT INTO \"%s\" ", program->physical_table);

        if (written < 0 || (size_t)written >= sizeof(prefix) ||
            strncmp(sql, prefix, (size_t)written) != 0) {
            continue;
        }
        if (!program->is_seeded) {
            programs->capture_failed = true;
            return 0;
        }
        if (program->guarded_sql != NULL) {
            if (strcmp(program->guarded_sql, sql) != 0 ||
                program->parameter_count != (size_t)sqlite3_bind_parameter_count(statement)) {
                programs->capture_failed = true;
            }
            return 0;
        }
        program->guarded_sql = malloc(strlen(sql) + 1U);
        if (program->guarded_sql == NULL) {
            programs->capture_failed = true;
            return 0;
        }
        memcpy(program->guarded_sql, sql, strlen(sql) + 1U);
        program->parameter_count = (size_t)sqlite3_bind_parameter_count(statement);
        program->has_guard = strstr(sql, " WHERE ") != NULL;
        program->sql_hash = hash_bytes(fnv_offset_basis, sql, strlen(sql));
        ++programs->captured_program_count;
        return 0;
    }
    return 0;
}

static int load_attribution_physical_names(
    sqlite3 *sqlite,
    struct attribution_program_set *programs
) {
    static const char sql[] =
        "SELECT tables.name, tables.physical_name "
        "FROM _mylite_catalog_tables AS tables "
        "JOIN _mylite_catalog_schemas AS schemas ON schemas.schema_id = tables.schema_id "
        "WHERE schemas.name = 'perf' AND tables.kind = 1";
    sqlite3_stmt *statement = NULL;
    size_t matched_count = 0U;
    int rc = sqlite3_prepare_v2(sqlite, sql, -1, &statement, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "large-dataset: prepare physical-name discovery failed\n");
        return 1;
    }
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        const char *logical = (const char *)sqlite3_column_text(statement, 0);
        const char *physical = (const char *)sqlite3_column_text(statement, 1);

        for (size_t index = 0U; index < attribution_table_count; ++index) {
            struct attribution_program *program = &programs->programs[index];

            if (logical == NULL || physical == NULL ||
                strcmp(program->logical_table, logical) != 0) {
                continue;
            }
            if (program->physical_table[0] != '\0' ||
                strlen(physical) >= sizeof(program->physical_table)) {
                (void)sqlite3_finalize(statement);
                return 1;
            }
            memcpy(program->physical_table, physical, strlen(physical) + 1U);
            ++matched_count;
            break;
        }
    }
    if (sqlite3_finalize(statement) != SQLITE_OK || rc != SQLITE_DONE ||
        matched_count != attribution_table_count) {
        fprintf(stderr, "large-dataset: incomplete physical-name discovery\n");
        return 1;
    }
    return 0;
}

static int validate_attribution_programs(struct attribution_program_set *programs) {
    size_t expected_count = 0U;

    if (programs->capture_failed) {
        fprintf(stderr, "large-dataset: generated-program capture drifted\n");
        return 1;
    }
    for (size_t index = 0U; index < attribution_table_count; ++index) {
        struct attribution_program *program = &programs->programs[index];

        if (!program->is_seeded) {
            continue;
        }
        ++expected_count;
        if (program->guarded_sql == NULL || program->parameter_count == 0U ||
            build_plain_attribution_sql(program) != 0) {
            fprintf(
                stderr,
                "large-dataset: missing generated program for %s\n",
                program->logical_table
            );
            return 1;
        }
    }
    if (programs->captured_program_count != expected_count) {
        fprintf(
            stderr,
            "large-dataset: expected %zu generated programs, captured %zu\n",
            expected_count,
            programs->captured_program_count
        );
        return 1;
    }
    return 0;
}

static int validate_attribution_layout(
    sqlite3 *sqlite,
    const struct attribution_program_set *programs
) {
    static const char sql[] =
        "SELECT tables.physical_name "
        "FROM _mylite_catalog_tables AS tables "
        "JOIN _mylite_catalog_schemas AS schemas ON schemas.schema_id = tables.schema_id "
        "WHERE schemas.name = 'perf' AND tables.kind = 1 AND tables.name = ?1";
    sqlite3_stmt *statement = NULL;
    int rc = sqlite3_prepare_v2(sqlite, sql, -1, &statement, NULL);

    if (rc != SQLITE_OK) {
        return 1;
    }
    for (size_t index = 0U; index < attribution_table_count; ++index) {
        const struct attribution_program *program = &programs->programs[index];
        const char *physical = NULL;

        (void)sqlite3_reset(statement);
        if (sqlite3_clear_bindings(statement) != SQLITE_OK ||
            sqlite3_bind_text(statement, 1, program->logical_table, -1, SQLITE_STATIC) !=
                SQLITE_OK ||
            sqlite3_step(statement) != SQLITE_ROW) {
            (void)sqlite3_finalize(statement);
            return 1;
        }
        physical = (const char *)sqlite3_column_text(statement, 0);
        if (physical == NULL || strcmp(physical, program->physical_table) != 0 ||
            sqlite3_step(statement) != SQLITE_DONE) {
            fprintf(stderr, "large-dataset: physical-name drift for %s\n", program->logical_table);
            (void)sqlite3_finalize(statement);
            return 1;
        }
    }
    return sqlite3_finalize(statement) == SQLITE_OK ? 0 : 1;
}

static int build_plain_attribution_sql(struct attribution_program *program) {
    const char *select_marker = strstr(program->guarded_sql, ") SELECT ");
    size_t capacity = 0U;
    size_t used = 0U;

    if (!program->has_guard) {
        program->plain_sql = malloc(strlen(program->guarded_sql) + 1U);
        if (program->plain_sql == NULL) {
            return 1;
        }
        memcpy(program->plain_sql, program->guarded_sql, strlen(program->guarded_sql) + 1U);
        return 0;
    }
    if (select_marker == NULL) {
        return 1;
    }
    capacity = (size_t)(select_marker - program->guarded_sql) +
               (program->parameter_count * attribution_parameter_sql_capacity) +
               attribution_identifier_sql_extra;
    program->plain_sql = malloc(capacity);
    if (program->plain_sql == NULL) {
        return 1;
    }
    used = (size_t)(select_marker - program->guarded_sql) + 1U;
    memcpy(program->plain_sql, program->guarded_sql, used);
    {
        int written = snprintf(program->plain_sql + used, capacity - used, " VALUES (");

        if (written < 0 || (size_t)written >= capacity - used) {
            return 1;
        }
        used += (size_t)written;
    }
    for (size_t parameter = 1U; parameter <= program->parameter_count; ++parameter) {
        int written = snprintf(
            program->plain_sql + used,
            capacity - used,
            parameter == 1U ? "?%zu" : ", ?%zu",
            parameter
        );

        if (written < 0 || (size_t)written >= capacity - used) {
            return 1;
        }
        used += (size_t)written;
    }
    if (used + 2U > capacity) {
        return 1;
    }
    program->plain_sql[used] = ')';
    program->plain_sql[used + 1U] = '\0';
    return 0;
}

static void deinit_attribution_programs(struct attribution_program_set *programs) {
    for (size_t index = 0U; index < attribution_table_count; ++index) {
        free(programs->programs[index].guarded_sql);
        free(programs->programs[index].plain_sql);
        programs->programs[index].guarded_sql = NULL;
        programs->programs[index].plain_sql = NULL;
    }
}

static const struct attribution_program *find_attribution_program(
    const struct attribution_program_set *programs,
    const char *logical_sql
) {
    if (programs == NULL || logical_sql == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < attribution_table_count; ++index) {
        const struct attribution_program *program = &programs->programs[index];
        char prefix[MYLITE_CATALOG_IDENTIFIER_CAPACITY + attribution_identifier_sql_extra];
        int written = snprintf(prefix, sizeof(prefix), "INSERT INTO %s ", program->logical_table);

        if (written > 0 && (size_t)written < sizeof(prefix) &&
            strncmp(logical_sql, prefix, (size_t)written) == 0) {
            return program->is_seeded ? program : NULL;
        }
    }
    return NULL;
}

static int make_attribution_path(
    const struct benchmark_options *options,
    const char *suffix,
    char *out_path,
    size_t out_path_capacity
) {
    const char *directory = options->database_directory == NULL ? default_database_directory()
                                                                : options->database_directory;
#if defined(_WIN32)
    const char separator = '\\';
#else
    const char separator = '/';
#endif
    int written =
        options->database_base != NULL
            ? snprintf(out_path, out_path_capacity, "%s.%s", options->database_base, suffix)
            : snprintf(
                  out_path,
                  out_path_capacity,
                  "%s%cmylite-large-%ld-%zu-%s.db",
                  directory,
                  separator,
                  benchmark_process_id(),
                  options->row_count,
                  suffix
              );

    if (written < 0 || (size_t)written >= out_path_capacity) {
        fprintf(stderr, "large-dataset: attribution database path is too long\n");
        return 1;
    }
    return 0;
}

static int verify_attribution_database(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset,
    uint64_t out_checksums[attribution_table_count]
) {
    const uint64_t expected[attribution_table_count] = {
        [attribution_table_accounts] = dataset->account_count,
        [attribution_table_items] = dataset->row_count,
        [attribution_table_item_tags] = dataset->row_count,
        [attribution_table_write_log] = 0U,
        [attribution_table_upsert_targets] = dataset->account_count,
        [attribution_table_composite_parents] = dataset->account_count,
        [attribution_table_composite_children] = 0U,
        [attribution_table_fanout_parents] = fanout_parent_count_for_rows(dataset->row_count),
        [attribution_table_fanout_children] = dataset->row_count,
        [attribution_table_restrict_parents] = fanout_parent_count_for_rows(dataset->row_count),
        [attribution_table_restrict_children] =
            fanout_parent_count_for_rows(dataset->row_count) - 1U,
        [attribution_table_set_null_parents] = fanout_parent_count_for_rows(dataset->row_count),
        [attribution_table_set_null_children] = dataset->row_count,
    };

    if (!engine_uses_direct_sqlite(database->kind)) {
        if (verify_benchmark_database(database, dataset) != 0) {
            return 1;
        }
    } else {
        for (size_t index = 0U; index < attribution_table_count; ++index) {
            const struct attribution_program *program =
                &database->attribution_programs->programs[index];
            char sql[MYLITE_CATALOG_IDENTIFIER_CAPACITY + attribution_query_sql_extra];
            sqlite3_stmt *statement = NULL;
            uint64_t actual = 0U;
            int written =
                snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM \"%s\"", program->physical_table);

            if (written < 0 || (size_t)written >= sizeof(sql) ||
                sqlite3_prepare_v2(database->sqlite, sql, -1, &statement, NULL) != SQLITE_OK ||
                sqlite3_step(statement) != SQLITE_ROW) {
                (void)sqlite3_finalize(statement);
                return 1;
            }
            actual = (uint64_t)sqlite3_column_int64(statement, 0);
            if (sqlite3_step(statement) != SQLITE_DONE ||
                sqlite3_finalize(statement) != SQLITE_OK || actual != expected[index]) {
                fprintf(
                    stderr,
                    "large-dataset: %s physical verification for %s expected %" PRIu64
                    ", got %" PRIu64 "\n",
                    engine_name(database->kind),
                    program->logical_table,
                    expected[index],
                    actual
                );
                return 1;
            }
        }
    }
    return read_attribution_checksums(database, out_checksums);
}

static int read_attribution_checksums(
    struct benchmark_database *database,
    uint64_t out_checksums[attribution_table_count]
) {
    for (size_t index = 0U; index < attribution_table_count; ++index) {
        const struct attribution_program *program =
            &database->attribution_programs->programs[index];
        const char *table = database->kind == benchmark_engine_sqlite ? program->logical_table
                                                                      : program->physical_table;
        char sql[MYLITE_CATALOG_IDENTIFIER_CAPACITY + attribution_query_sql_extra];
        sqlite3_stmt *statement = NULL;
        uint64_t checksum = fnv_offset_basis;
        int written = snprintf(sql, sizeof(sql), "SELECT * FROM \"%s\" ORDER BY 1", table);
        int rc = SQLITE_OK;

        if (written < 0 || (size_t)written >= sizeof(sql) ||
            sqlite3_prepare_v2(database->sqlite, sql, -1, &statement, NULL) != SQLITE_OK) {
            (void)sqlite3_finalize(statement);
            return 1;
        }
        while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
            int column_count = sqlite3_column_count(statement);

            hash_uint64(&checksum, (uint64_t)column_count);
            for (int column = 0; column < column_count; ++column) {
                int type = sqlite3_column_type(statement, column);

                hash_uint64(&checksum, (uint64_t)type);
                if (type != SQLITE_NULL) {
                    int size = sqlite3_column_bytes(statement, column);
                    const void *value = sqlite3_column_blob(statement, column);

                    if (size < 0 || (value == NULL && size != 0)) {
                        (void)sqlite3_finalize(statement);
                        return 1;
                    }
                    hash_uint64(&checksum, (uint64_t)size);
                    checksum = hash_bytes(checksum, value, (size_t)size);
                }
            }
        }
        if (rc != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK) {
            return 1;
        }
        out_checksums[index] = checksum;
    }
    return 0;
}

static int verify_attribution_checksums(
    size_t sample,
    uint64_t checksums[attribution_layer_count][attribution_table_count]
) {
    static const enum benchmark_engine_kind layers[] = {
        benchmark_engine_sqlite,
        benchmark_engine_mylite_physical,
        benchmark_engine_mylite_guarded,
        benchmark_engine_mylite,
    };

    for (size_t layer = 1U; layer < sizeof(layers) / sizeof(layers[0]); ++layer) {
        for (size_t table = 0U; table < attribution_table_count; ++table) {
            if (checksums[layer][table] != checksums[0][table]) {
                fprintf(
                    stderr,
                    "large-dataset: sample %zu %s checksum mismatch for %s\n",
                    sample,
                    engine_name(layers[layer]),
                    (const char *const[attribution_table_count]){"accounts",
                                                                 "items",
                                                                 "item_tags",
                                                                 "write_log",
                                                                 "upsert_targets",
                                                                 "composite_parents",
                                                                 "composite_children",
                                                                 "fanout_parents",
                                                                 "fanout_children",
                                                                 "restrict_parents",
                                                                 "restrict_children",
                                                                 "set_null_parents",
                                                                 "set_null_children"}[table]
                );
                return 1;
            }
        }
    }
    return 0;
}

static void print_attribution_measurement(
    FILE *output,
    const struct benchmark_dataset *dataset,
    enum benchmark_engine_kind kind,
    size_t sample,
    const struct benchmark_load_measurement *measurement,
    const uint64_t checksums[attribution_table_count]
) {
#ifdef MYLITE_ENABLE_PROFILING
    const struct mylite_profile_snapshot zero = {0};
#  define PRINT_ATTRIBUTION_PHASE(                                                                 \
      phase_name,                                                                                  \
      elapsed_value,                                                                               \
      cpu_value,                                                                                   \
      profile_value,                                                                               \
      previous_value                                                                               \
  )                                                                                                \
      print_attribution_phase(                                                                     \
          output,                                                                                  \
          dataset,                                                                                 \
          kind,                                                                                    \
          sample,                                                                                  \
          phase_name,                                                                              \
          dataset_hash,                                                                            \
          elapsed_value,                                                                           \
          cpu_value,                                                                               \
          profile_value,                                                                           \
          previous_value                                                                           \
      )
#else
#  define PRINT_ATTRIBUTION_PHASE(                                                                 \
      phase_name,                                                                                  \
      elapsed_value,                                                                               \
      cpu_value,                                                                                   \
      profile_value,                                                                               \
      previous_value                                                                               \
  )                                                                                                \
      print_attribution_phase(                                                                     \
          output,                                                                                  \
          dataset,                                                                                 \
          kind,                                                                                    \
          sample,                                                                                  \
          phase_name,                                                                              \
          dataset_hash,                                                                            \
          elapsed_value,                                                                           \
          cpu_value                                                                                \
      )
#endif
    uint64_t dataset_hash = fnv_offset_basis;

    for (size_t index = 0U; index < attribution_table_count; ++index) {
        hash_uint64(&dataset_hash, checksums[index]);
    }

    PRINT_ATTRIBUTION_PHASE(
        "total",
        measurement->total_ns,
        measurement->total_cpu_ns,
        &measurement->profile,
        &zero
    );
    PRINT_ATTRIBUTION_PHASE(
        "accounts",
        measurement->accounts_ns,
        measurement->accounts_cpu_ns,
        &measurement->accounts_profile,
        &zero
    );
    PRINT_ATTRIBUTION_PHASE(
        "items",
        measurement->items_ns,
        measurement->items_cpu_ns,
        &measurement->items_profile,
        &measurement->accounts_profile
    );
    PRINT_ATTRIBUTION_PHASE(
        "tags",
        measurement->tags_ns,
        measurement->tags_cpu_ns,
        &measurement->tags_profile,
        &measurement->items_profile
    );
    PRINT_ATTRIBUTION_PHASE(
        "support",
        measurement->support_ns,
        measurement->support_cpu_ns,
        &measurement->support_profile,
        &measurement->tags_profile
    );
    PRINT_ATTRIBUTION_PHASE(
        "support.upsert_composite",
        measurement->support_upsert_ns,
        measurement->support_upsert_cpu_ns,
        &measurement->support_upsert_profile,
        &measurement->tags_profile
    );
    PRINT_ATTRIBUTION_PHASE(
        "support.fanout",
        measurement->support_fanout_ns,
        measurement->support_fanout_cpu_ns,
        &measurement->support_fanout_profile,
        &measurement->support_upsert_profile
    );
    PRINT_ATTRIBUTION_PHASE(
        "support.restrict",
        measurement->support_restrict_ns,
        measurement->support_restrict_cpu_ns,
        &measurement->support_restrict_profile,
        &measurement->support_fanout_profile
    );
    PRINT_ATTRIBUTION_PHASE(
        "support.set_null",
        measurement->support_set_null_ns,
        measurement->support_set_null_cpu_ns,
        &measurement->support_set_null_profile,
        &measurement->support_restrict_profile
    );
#undef PRINT_ATTRIBUTION_PHASE
}

static void print_attribution_phase(
#ifdef MYLITE_ENABLE_PROFILING
    FILE *output,
    const struct benchmark_dataset *dataset,
    enum benchmark_engine_kind kind,
    size_t sample,
    const char *phase,
    uint64_t dataset_hash,
    uint64_t elapsed_ns,
    uint64_t cpu_ns,
    const struct mylite_profile_snapshot *profile,
    const struct mylite_profile_snapshot *previous
) {
#  define PROFILE_DELTA(field)                                                                     \
      (profile->field >= previous->field ? profile->field - previous->field : 0U)
    fprintf(
        output,
        "measurement,%zu,%zu,%s,%s,,,,,%" PRIu64 ",%.3f,%.3f,%" PRIu64 ",%" PRIu64 ",%" PRIu64
        ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
        ",%" PRIu64 ",%.3f,%" PRIu64 ",%.3f,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
        ",%" PRIu64 ",%" PRIu64 "\n",
        dataset->row_count,
        sample,
        engine_name(kind),
        phase,
        dataset_hash,
        (double)elapsed_ns / (double)nanoseconds_per_millisecond,
        (double)cpu_ns / (double)nanoseconds_per_millisecond,
        PROFILE_DELTA(sqlite_step_count),
        PROFILE_DELTA(sqlite_vm_step_count),
        PROFILE_DELTA(sqlite_fullscan_step_count),
        PROFILE_DELTA(sqlite_sort_count),
        PROFILE_DELTA(sqlite_autoindex_count),
        PROFILE_DELTA(sqlite_reprepare_count),
        PROFILE_DELTA(sqlite_run_count),
        PROFILE_DELTA(sqlite_filter_hit_count),
        PROFILE_DELTA(sqlite_filter_miss_count),
        PROFILE_DELTA(metadata_vm_step_count),
        PROFILE_DELTA(scalar_callback_count),
        (double)PROFILE_DELTA(scalar_callback_ns) / (double)nanoseconds_per_millisecond,
        PROFILE_DELTA(collation_callback_count),
        (double)PROFILE_DELTA(collation_callback_ns) / (double)nanoseconds_per_millisecond,
        PROFILE_DELTA(allocation_count),
        PROFILE_DELTA(allocation_bytes),
        PROFILE_DELTA(dml_plan_count),
        PROFILE_DELTA(dml_plan_cache_hit_count),
        PROFILE_DELTA(execution_statement_cache_hit_count),
        PROFILE_DELTA(execution_statement_cache_miss_count)
    );
#  undef PROFILE_DELTA
}
#else
    FILE *output,
    const struct benchmark_dataset *dataset,
    enum benchmark_engine_kind kind,
    size_t sample,
    const char *phase,
    uint64_t dataset_hash,
    uint64_t elapsed_ns,
    uint64_t cpu_ns
) {
    fprintf(
        output,
        "measurement,%zu,%zu,%s,%s,,,,,%" PRIu64 ",%.3f,%.3f,"
        "0,0,0,0,0,0,0,0,0,0,0,0.000,0,0.000,0,0,0,0,0,0\n",
        dataset->row_count,
        sample,
        engine_name(kind),
        phase,
        dataset_hash,
        (double)elapsed_ns / (double)nanoseconds_per_millisecond,
        (double)cpu_ns / (double)nanoseconds_per_millisecond
    );
}
#endif

static int prepare_benchmark_databases(
    const struct benchmark_options *options,
    struct benchmark_paths *paths,
    const struct benchmark_dataset *dataset,
    struct benchmark_database *mylite,
    struct benchmark_database *sqlite,
    struct benchmark_load_measurement *mylite_load,
    struct benchmark_load_measurement *sqlite_load
) {
    if (make_database_paths(options, paths) != 0) {
        return 1;
    }
    if (!options->reuse_databases) {
        remove_database_files(paths->mylite);
        remove_database_files(paths->sqlite);
    }
    fprintf(
        stderr,
        "large-dataset: rows=%zu accounts=%zu tags=%zu\n",
        dataset->row_count,
        dataset->account_count,
        dataset->tag_count
    );
    if (open_benchmark_database(benchmark_engine_mylite, paths->mylite, mylite) != 0 ||
        open_benchmark_database(benchmark_engine_sqlite, paths->sqlite, sqlite) != 0) {
        return 1;
    }
    if (options->reuse_databases) {
        if (execute_sql(mylite, "USE perf") != 0) {
            return 1;
        }
    } else {
        if (create_benchmark_schema(mylite) != 0 || create_benchmark_schema(sqlite) != 0) {
            return 1;
        }
        fprintf(stderr, "large-dataset: seeding MyLite\n");
        if (seed_benchmark_database(mylite, dataset, mylite_load) != 0) {
            return 1;
        }
        fprintf(stderr, "large-dataset: seeding SQLite\n");
        if (seed_benchmark_database(sqlite, dataset, sqlite_load) != 0) {
            return 1;
        }
    }
    if (verify_benchmark_database(mylite, dataset) != 0 ||
        verify_benchmark_database(sqlite, dataset) != 0) {
        return 1;
    }
    if (options->analyze && (execute_sql(mylite, "ANALYZE TABLE accounts, items, item_tags") != 0 ||
                             execute_sql(sqlite, "ANALYZE") != 0)) {
        return 1;
    }
    return 0;
}

static int make_database_paths(
    const struct benchmark_options *options,
    struct benchmark_paths *out_paths
) {
    if (options->database_base != NULL) {
        int mylite_written = snprintf(
            out_paths->mylite,
            sizeof(out_paths->mylite),
            "%s.mylite",
            options->database_base
        );
        int sqlite_written = snprintf(
            out_paths->sqlite,
            sizeof(out_paths->sqlite),
            "%s.sqlite",
            options->database_base
        );

        if (mylite_written < 0 || (size_t)mylite_written >= sizeof(out_paths->mylite) ||
            sqlite_written < 0 || (size_t)sqlite_written >= sizeof(out_paths->sqlite)) {
            fprintf(stderr, "large-dataset: database base path is too long\n");
            return 1;
        }
        return 0;
    }
    const char *directory = options->database_directory == NULL ? default_database_directory()
                                                                : options->database_directory;
#if defined(_WIN32)
    const char separator = '\\';
#else
    const char separator = '/';
#endif
    int mylite_written = snprintf(
        out_paths->mylite,
        sizeof(out_paths->mylite),
        "%s%cmylite_large_%ld_%zu.mylite",
        directory,
        separator,
        benchmark_process_id(),
        options->row_count
    );
    int sqlite_written = snprintf(
        out_paths->sqlite,
        sizeof(out_paths->sqlite),
        "%s%cmylite_large_%ld_%zu.sqlite",
        directory,
        separator,
        benchmark_process_id(),
        options->row_count
    );

    if (mylite_written < 0 || (size_t)mylite_written >= sizeof(out_paths->mylite) ||
        sqlite_written < 0 || (size_t)sqlite_written >= sizeof(out_paths->sqlite)) {
        fprintf(stderr, "large-dataset: database path is too long\n");
        return 1;
    }
    return 0;
}

static int open_benchmark_database(
    enum benchmark_engine_kind kind,
    const char *path,
    struct benchmark_database *out_database
) {
    int rc = 0;

    *out_database = (struct benchmark_database){
        .kind = kind,
        .mylite = NULL,
        .sqlite = NULL,
        .path = path,
        .attribution_programs = NULL,
        .profile_seed = true,
#ifdef MYLITE_ENABLE_PROFILING
        .manual_profile = NULL,
#endif
    };
    if (engine_uses_mylite_storage(kind)) {
        rc = mylite_open(path, &out_database->mylite);
        if (rc != MYLITE_OK) {
            fprintf(stderr, "large-dataset: MyLite open failed: %d\n", rc);
            return 1;
        }
        out_database->sqlite = mylite_connection_sqlite_for_test(out_database->mylite);
        return 0;
    }
    rc = sqlite3_open(path, &out_database->sqlite);
    if (rc != SQLITE_OK) {
        fprintf(
            stderr,
            "large-dataset: SQLite open failed: %s\n",
            sqlite3_errmsg(out_database->sqlite)
        );
        return 1;
    }
    if (sqlite3_busy_timeout(out_database->sqlite, sqlite_busy_timeout_ms) != SQLITE_OK ||
        sqlite3_exec(
            out_database->sqlite,
            "PRAGMA journal_mode=DELETE; PRAGMA foreign_keys=ON; PRAGMA trusted_schema=OFF",
            NULL,
            NULL,
            NULL
        ) != SQLITE_OK) {
        fprintf(
            stderr,
            "large-dataset: SQLite configuration failed: %s\n",
            sqlite3_errmsg(out_database->sqlite)
        );
        return 1;
    }
    return 0;
}

static int create_benchmark_schema(struct benchmark_database *database) {
    static const char *const mylite_schema[] = {
        "CREATE DATABASE perf",
        "USE perf",
        "CREATE TABLE accounts ("
        "id BIGINT NOT NULL, region INT NOT NULL, name VARCHAR(64) NOT NULL,"
        "PRIMARY KEY (id), KEY idx_accounts_region (region, id))",
        "CREATE TABLE items ("
        "id BIGINT NOT NULL, account_id BIGINT NOT NULL, category_id INT NOT NULL,"
        "status VARCHAR(16) NOT NULL, score BIGINT NOT NULL, created_at BIGINT NOT NULL,"
        "title VARCHAR(64) NOT NULL, payload VARCHAR(160) NOT NULL,"
        "tenant_id INT NOT NULL, optional_value BIGINT NULL,"
        "PRIMARY KEY (id),"
        "KEY idx_items_account_created (account_id, created_at, id),"
        "KEY idx_items_account_score (account_id, score),"
        "KEY idx_items_category_score (category_id, score),"
        "KEY idx_items_status_created (status, created_at, id),"
        "KEY idx_items_tenant_status_score (tenant_id, status, score),"
        "KEY idx_items_optional (optional_value, id),"
        "CONSTRAINT fk_items_account FOREIGN KEY (account_id) "
        "REFERENCES accounts (id) ON DELETE CASCADE)",
        "CREATE TABLE item_tags ("
        "item_id BIGINT NOT NULL, tag_id INT NOT NULL, weight INT NOT NULL,"
        "PRIMARY KEY (item_id, tag_id),"
        "KEY idx_item_tags_tag_weight (tag_id, weight, item_id),"
        "CONSTRAINT fk_item_tags_item FOREIGN KEY (item_id) "
        "REFERENCES items (id) ON DELETE CASCADE)",
        "CREATE TABLE write_log ("
        "id BIGINT NOT NULL, item_id BIGINT NOT NULL, note VARCHAR(64) NOT NULL,"
        "PRIMARY KEY (id), KEY idx_write_log_item (item_id),"
        "CONSTRAINT fk_write_log_item FOREIGN KEY (item_id) REFERENCES items (id))",
        "CREATE TABLE upsert_targets ("
        "id BIGINT NOT NULL, value_text VARCHAR(64) NOT NULL, PRIMARY KEY (id))",
        "CREATE TABLE insert_index_0 (id BIGINT NOT NULL, value_a BIGINT NOT NULL,"
        "value_b BIGINT NOT NULL)",
        "CREATE TABLE insert_index_1 (id BIGINT NOT NULL, value_a BIGINT NOT NULL,"
        "value_b BIGINT NOT NULL, PRIMARY KEY (id))",
        "CREATE TABLE insert_index_5 (id BIGINT NOT NULL, value_a BIGINT NOT NULL,"
        "value_b BIGINT NOT NULL, PRIMARY KEY (id), KEY i5_a (value_a), KEY i5_b (value_b),"
        "KEY i5_ab (value_a, value_b), KEY i5_ba (value_b, value_a))",
        "CREATE TABLE insert_index_10 (id BIGINT NOT NULL, value_a BIGINT NOT NULL,"
        "value_b BIGINT NOT NULL, PRIMARY KEY (id), KEY i10_a (value_a), KEY i10_b (value_b),"
        "KEY i10_ab (value_a, value_b), KEY i10_ba (value_b, value_a),"
        "KEY i10_ai (value_a, id), KEY i10_bi (value_b, id),"
        "KEY i10_abi (value_a, value_b, id), KEY i10_bai (value_b, value_a, id),"
        "KEY i10_ia (id, value_a))",
        "CREATE TABLE composite_parents ("
        "id BIGINT NOT NULL, shard_id INT NOT NULL, payload BIGINT NOT NULL,"
        "PRIMARY KEY (id, shard_id))",
        "CREATE TABLE composite_children ("
        "id BIGINT NOT NULL, parent_id BIGINT NOT NULL, parent_shard INT NOT NULL,"
        "payload BIGINT NOT NULL, PRIMARY KEY (id),"
        "KEY idx_composite_parent (parent_id, parent_shard),"
        "CONSTRAINT fk_composite_parent FOREIGN KEY (parent_id, parent_shard) "
        "REFERENCES composite_parents (id, shard_id))",
        "CREATE TABLE fanout_parents (id BIGINT NOT NULL, payload BIGINT NOT NULL,"
        "PRIMARY KEY (id))",
        "CREATE TABLE fanout_children ("
        "id BIGINT NOT NULL, parent_id BIGINT NOT NULL, payload BIGINT NOT NULL,"
        "PRIMARY KEY (id), KEY idx_fanout_parent (parent_id),"
        "CONSTRAINT fk_fanout_parent FOREIGN KEY (parent_id) "
        "REFERENCES fanout_parents (id) ON DELETE CASCADE)",
        "CREATE TABLE restrict_parents (id BIGINT NOT NULL, PRIMARY KEY (id))",
        "CREATE TABLE restrict_children ("
        "id BIGINT NOT NULL, parent_id BIGINT NOT NULL, PRIMARY KEY (id),"
        "KEY idx_restrict_parent (parent_id),"
        "CONSTRAINT fk_restrict_parent FOREIGN KEY (parent_id) "
        "REFERENCES restrict_parents (id) ON DELETE RESTRICT)",
        "CREATE TABLE set_null_parents (id BIGINT NOT NULL, PRIMARY KEY (id))",
        "CREATE TABLE set_null_children ("
        "id BIGINT NOT NULL, parent_id BIGINT NULL, payload BIGINT NOT NULL,"
        "PRIMARY KEY (id), KEY idx_set_null_parent (parent_id),"
        "CONSTRAINT fk_set_null_parent FOREIGN KEY (parent_id) "
        "REFERENCES set_null_parents (id) ON DELETE SET NULL)",
    };
    static const char *const sqlite_schema[] = {
        "CREATE TABLE accounts ("
        "id BIGINT NOT NULL PRIMARY KEY, region INTEGER NOT NULL, name TEXT NOT NULL)",
        "CREATE INDEX idx_accounts_region ON accounts(region, id)",
        "CREATE TABLE items ("
        "id BIGINT NOT NULL PRIMARY KEY, account_id BIGINT NOT NULL, category_id INTEGER NOT NULL,"
        "status TEXT NOT NULL, score BIGINT NOT NULL, created_at BIGINT NOT NULL,"
        "title TEXT NOT NULL, payload TEXT NOT NULL, tenant_id INTEGER NOT NULL,"
        "optional_value BIGINT NULL,"
        "FOREIGN KEY (account_id) REFERENCES accounts(id) ON DELETE CASCADE)",
        "CREATE INDEX idx_items_account_created ON items(account_id, created_at, id)",
        "CREATE INDEX idx_items_account_score ON items(account_id, score)",
        "CREATE INDEX idx_items_category_score ON items(category_id, score)",
        "CREATE INDEX idx_items_status_created ON items(status, created_at, id)",
        "CREATE INDEX idx_items_tenant_status_score ON items(tenant_id, status, score)",
        "CREATE INDEX idx_items_optional ON items(optional_value, id)",
        "CREATE TABLE item_tags ("
        "item_id BIGINT NOT NULL, tag_id INTEGER NOT NULL, weight INTEGER NOT NULL,"
        "PRIMARY KEY (item_id, tag_id),"
        "FOREIGN KEY (item_id) REFERENCES items(id) ON DELETE CASCADE)",
        "CREATE INDEX idx_item_tags_tag_weight ON item_tags(tag_id, weight, item_id)",
        "CREATE TABLE write_log ("
        "id BIGINT NOT NULL PRIMARY KEY, item_id BIGINT NOT NULL, note TEXT NOT NULL,"
        "FOREIGN KEY (item_id) REFERENCES items(id))",
        "CREATE INDEX idx_write_log_item ON write_log(item_id)",
        "CREATE TABLE upsert_targets ("
        "id BIGINT NOT NULL PRIMARY KEY, value_text TEXT NOT NULL)",
        "CREATE TABLE insert_index_0 ("
        "id BIGINT NOT NULL, value_a BIGINT NOT NULL, value_b BIGINT NOT NULL)",
        "CREATE TABLE insert_index_1 ("
        "id BIGINT NOT NULL PRIMARY KEY, value_a BIGINT NOT NULL, value_b BIGINT NOT NULL)",
        "CREATE TABLE insert_index_5 ("
        "id BIGINT NOT NULL PRIMARY KEY, value_a BIGINT NOT NULL, value_b BIGINT NOT NULL)",
        "CREATE INDEX i5_a ON insert_index_5(value_a)",
        "CREATE INDEX i5_b ON insert_index_5(value_b)",
        "CREATE INDEX i5_ab ON insert_index_5(value_a, value_b)",
        "CREATE INDEX i5_ba ON insert_index_5(value_b, value_a)",
        "CREATE TABLE insert_index_10 ("
        "id BIGINT NOT NULL PRIMARY KEY, value_a BIGINT NOT NULL, value_b BIGINT NOT NULL)",
        "CREATE INDEX i10_a ON insert_index_10(value_a)",
        "CREATE INDEX i10_b ON insert_index_10(value_b)",
        "CREATE INDEX i10_ab ON insert_index_10(value_a, value_b)",
        "CREATE INDEX i10_ba ON insert_index_10(value_b, value_a)",
        "CREATE INDEX i10_ai ON insert_index_10(value_a, id)",
        "CREATE INDEX i10_bi ON insert_index_10(value_b, id)",
        "CREATE INDEX i10_abi ON insert_index_10(value_a, value_b, id)",
        "CREATE INDEX i10_bai ON insert_index_10(value_b, value_a, id)",
        "CREATE INDEX i10_ia ON insert_index_10(id, value_a)",
        "CREATE TABLE composite_parents ("
        "id BIGINT NOT NULL, shard_id INTEGER NOT NULL, payload BIGINT NOT NULL,"
        "PRIMARY KEY (id, shard_id))",
        "CREATE TABLE composite_children ("
        "id BIGINT NOT NULL PRIMARY KEY, parent_id BIGINT NOT NULL,"
        "parent_shard INTEGER NOT NULL, payload BIGINT NOT NULL,"
        "FOREIGN KEY (parent_id, parent_shard) REFERENCES composite_parents(id, shard_id))",
        "CREATE INDEX idx_composite_parent ON composite_children(parent_id, parent_shard)",
        "CREATE TABLE fanout_parents ("
        "id BIGINT NOT NULL PRIMARY KEY, payload BIGINT NOT NULL)",
        "CREATE TABLE fanout_children ("
        "id BIGINT NOT NULL PRIMARY KEY, parent_id BIGINT NOT NULL, payload BIGINT NOT NULL,"
        "FOREIGN KEY (parent_id) REFERENCES fanout_parents(id) ON DELETE CASCADE)",
        "CREATE INDEX idx_fanout_parent ON fanout_children(parent_id)",
        "CREATE TABLE restrict_parents (id BIGINT NOT NULL PRIMARY KEY)",
        "CREATE TABLE restrict_children ("
        "id BIGINT NOT NULL PRIMARY KEY, parent_id BIGINT NOT NULL,"
        "FOREIGN KEY (parent_id) REFERENCES restrict_parents(id) ON DELETE RESTRICT)",
        "CREATE INDEX idx_restrict_parent ON restrict_children(parent_id)",
        "CREATE TABLE set_null_parents (id BIGINT NOT NULL PRIMARY KEY)",
        "CREATE TABLE set_null_children ("
        "id BIGINT NOT NULL PRIMARY KEY, parent_id BIGINT NULL, payload BIGINT NOT NULL,"
        "FOREIGN KEY (parent_id) REFERENCES set_null_parents(id) ON DELETE SET NULL)",
        "CREATE INDEX idx_set_null_parent ON set_null_children(parent_id)",
    };
    const char *const *queries =
        engine_uses_mylite_storage(database->kind) ? mylite_schema : sqlite_schema;
    size_t query_count = engine_uses_mylite_storage(database->kind)
                             ? sizeof(mylite_schema) / sizeof(mylite_schema[0])
                             : sizeof(sqlite_schema) / sizeof(sqlite_schema[0]);

    for (size_t index = 0U; index < query_count; ++index) {
        int statement_result = 0;

        if (engine_uses_mylite_storage(database->kind)) {
            mylite_result *result = NULL;
            int rc =
                mylite_execute(database->mylite, queries[index], strlen(queries[index]), &result);

            mylite_result_free(result);
            statement_result = rc == MYLITE_OK ? 0 : 1;
        } else {
            statement_result = execute_sql(database, queries[index]);
        }
        if (statement_result != 0) {
            fprintf(
                stderr,
                "large-dataset: %s schema statement %zu failed\n",
                engine_name(database->kind),
                index + 1U
            );
            return 1;
        }
    }
    return 0;
}

static int seed_benchmark_database(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset,
    struct benchmark_load_measurement *out_measurement
) {
    uint64_t total_started = monotonic_now_ns();
    uint64_t total_cpu_started = process_cpu_now_ns();
    uint64_t phase_started = 0U;
    uint64_t phase_cpu_started = 0U;
#ifdef MYLITE_ENABLE_PROFILING
    bool profile_started = false;

    if (database->profile_seed && engine_uses_mylite_storage(database->kind)) {
        if (mylite_profile_start(database->mylite) != MYLITE_OK) {
            fprintf(stderr, "large-dataset: failed to start load profile\n");
            return 1;
        }
        profile_started = true;
        if (engine_uses_direct_sqlite(database->kind)) {
            mylite_profile_enter_api(database->mylite);
        }
    } else if (database->profile_seed) {
        memset(&out_measurement->profile, 0, sizeof(out_measurement->profile));
        database->manual_profile = &out_measurement->profile;
        profile_started = true;
    }
#endif

    if (begin_transaction(database) != 0) {
#ifdef MYLITE_ENABLE_PROFILING
        (void)finish_load_profile(database, dataset, out_measurement, &profile_started);
#endif
        return 1;
    }
    phase_started = monotonic_now_ns();
    phase_cpu_started = process_cpu_now_ns();
    if (seed_accounts(database, dataset) != 0) {
        (void)rollback_transaction(database);
#ifdef MYLITE_ENABLE_PROFILING
        (void)finish_load_profile(database, dataset, out_measurement, &profile_started);
#endif
        return 1;
    }
    out_measurement->accounts_ns = monotonic_now_ns() - phase_started;
    out_measurement->accounts_cpu_ns = process_cpu_now_ns() - phase_cpu_started;
#ifdef MYLITE_ENABLE_PROFILING
    capture_load_profile(database, &out_measurement->accounts_profile);
#endif
    phase_started = monotonic_now_ns();
    phase_cpu_started = process_cpu_now_ns();
    if (seed_items(database, dataset) != 0) {
        (void)rollback_transaction(database);
#ifdef MYLITE_ENABLE_PROFILING
        (void)finish_load_profile(database, dataset, out_measurement, &profile_started);
#endif
        return 1;
    }
    out_measurement->items_ns = monotonic_now_ns() - phase_started;
    out_measurement->items_cpu_ns = process_cpu_now_ns() - phase_cpu_started;
#ifdef MYLITE_ENABLE_PROFILING
    capture_load_profile(database, &out_measurement->items_profile);
#endif
    phase_started = monotonic_now_ns();
    phase_cpu_started = process_cpu_now_ns();
    if (seed_tags(database, dataset) != 0) {
        (void)rollback_transaction(database);
#ifdef MYLITE_ENABLE_PROFILING
        (void)finish_load_profile(database, dataset, out_measurement, &profile_started);
#endif
        return 1;
    }
    out_measurement->tags_ns = monotonic_now_ns() - phase_started;
    out_measurement->tags_cpu_ns = process_cpu_now_ns() - phase_cpu_started;
#ifdef MYLITE_ENABLE_PROFILING
    capture_load_profile(database, &out_measurement->tags_profile);
#endif
    phase_started = monotonic_now_ns();
    phase_cpu_started = process_cpu_now_ns();
    if (seed_support_tables(database, dataset, out_measurement) != 0) {
        (void)rollback_transaction(database);
#ifdef MYLITE_ENABLE_PROFILING
        (void)finish_load_profile(database, dataset, out_measurement, &profile_started);
#endif
        return 1;
    }
    out_measurement->support_ns = monotonic_now_ns() - phase_started;
    out_measurement->support_cpu_ns = process_cpu_now_ns() - phase_cpu_started;
#ifdef MYLITE_ENABLE_PROFILING
    capture_load_profile(database, &out_measurement->support_profile);
#endif
    if (commit_transaction(database) != 0) {
#ifdef MYLITE_ENABLE_PROFILING
        (void)finish_load_profile(database, dataset, out_measurement, &profile_started);
#endif
        return 1;
    }
    out_measurement->total_ns = monotonic_now_ns() - total_started;
    out_measurement->total_cpu_ns = process_cpu_now_ns() - total_cpu_started;
#ifdef MYLITE_ENABLE_PROFILING
    if (finish_load_profile(database, dataset, out_measurement, &profile_started) != 0) {
        return 1;
    }
#endif
    return 0;
}

#ifdef MYLITE_ENABLE_PROFILING
static int finish_load_profile(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset,
    struct benchmark_load_measurement *measurement,
    bool *profile_started
) {
    if (!*profile_started) {
        return 0;
    }
    *profile_started = false;
    if (engine_uses_mylite_storage(database->kind)) {
        if (engine_uses_direct_sqlite(database->kind)) {
            mylite_profile_leave_api(database->mylite);
        }
        if (mylite_profile_stop(database->mylite, &measurement->profile) != MYLITE_OK) {
            fprintf(stderr, "large-dataset: failed to stop load profile\n");
            return 1;
        }
    } else {
        database->manual_profile = NULL;
    }
    fprintf(
        stderr,
        "large-dataset-load-profile: rows=%zu sqlite_step_ms=%.3f metadata_step_ms=%.3f "
        "cursor_step_ms=%.3f statements=%" PRIu64 " sqlite_steps=%" PRIu64
        " metadata_steps=%" PRIu64 " dml_plans=%" PRIu64 " dml_plan_hits=%" PRIu64
        " allocations=%" PRIu64 " allocation_bytes=%" PRIu64 " statement_cache_hits=%" PRIu64
        " statement_cache_misses=%" PRIu64 "\n",
        dataset->row_count,
        (double)measurement->profile.sqlite_step_ns / (double)nanoseconds_per_millisecond,
        (double)measurement->profile.metadata_step_ns / (double)nanoseconds_per_millisecond,
        (double)measurement->profile.cursor_step_ns / (double)nanoseconds_per_millisecond,
        measurement->profile.statement_count,
        measurement->profile.sqlite_step_count,
        measurement->profile.metadata_step_count,
        measurement->profile.dml_plan_count,
        measurement->profile.dml_plan_cache_hit_count,
        measurement->profile.allocation_count,
        measurement->profile.allocation_bytes,
        measurement->profile.execution_statement_cache_hit_count,
        measurement->profile.execution_statement_cache_miss_count
    );
    return 0;
}

static void capture_load_profile(
    const struct benchmark_database *database,
    struct mylite_profile_snapshot *out_profile
) {
    if (engine_uses_mylite_storage(database->kind) && database->mylite->profile_active) {
        *out_profile = database->mylite->profile;
        return;
    }
    if (database->manual_profile != NULL) {
        *out_profile = *database->manual_profile;
    }
}
#endif

static int verify_benchmark_database(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset
) {
    static const char *const queries[] = {
        "SELECT COUNT(*) FROM accounts",
        "SELECT COUNT(*) FROM items",
        "SELECT COUNT(*) FROM item_tags",
        "SELECT COUNT(*) FROM write_log",
        "SELECT COUNT(*) FROM upsert_targets",
        "SELECT COUNT(*) FROM composite_parents",
        "SELECT COUNT(*) FROM composite_children",
        "SELECT COUNT(*) FROM fanout_parents",
        "SELECT COUNT(*) FROM fanout_children",
        "SELECT COUNT(*) FROM restrict_parents",
        "SELECT COUNT(*) FROM restrict_children",
        "SELECT COUNT(*) FROM set_null_parents",
        "SELECT COUNT(*) FROM set_null_children",
    };
    size_t fanout_parents = fanout_parent_count_for_rows(dataset->row_count);
    const uint64_t expected[] = {
        dataset->account_count,
        dataset->row_count,
        dataset->row_count,
        0U,
        dataset->account_count,
        dataset->account_count,
        0U,
        fanout_parents,
        dataset->row_count,
        fanout_parents,
        fanout_parents - 1U,
        fanout_parents,
        dataset->row_count,
    };

    for (size_t index = 0U; index < sizeof(queries) / sizeof(queries[0]); ++index) {
        uint64_t actual = 0U;

        if (fetch_scalar_count(database, queries[index], &actual) != 0 ||
            actual != expected[index]) {
            fprintf(
                stderr,
                "large-dataset: %s verification %zu expected %" PRIu64 ", got %" PRIu64 "\n",
                engine_name(database->kind),
                index,
                expected[index],
                actual
            );
            return 1;
        }
    }
    return 0;
}

static void print_csv_header(FILE *output) {
    fprintf(
        output,
        "record,scenario,rows,engine,mode,sample,iterations,result_rows,value_bytes,"
        "checksum,total_ms,avg_us,ops_per_sec,ratio\n"
    );
}

static void print_load_measurements(
    FILE *output,
    const struct benchmark_dataset *dataset,
    const struct benchmark_load_measurement *mylite,
    const struct benchmark_load_measurement *sqlite
) {
    static const char *const names[] = {
        "load.total",
        "load.accounts",
        "load.items",
        "load.tags",
        "load.support",
    };
    size_t fanout_parents = fanout_parent_count_for_rows(dataset->row_count);
    size_t support_operations =
        (dataset->account_count * 2U) + (dataset->row_count * 2U) + (fanout_parents * 4U) - 1U;
    const uint64_t mylite_values[] = {
        mylite->total_ns,
        mylite->accounts_ns,
        mylite->items_ns,
        mylite->tags_ns,
        mylite->support_ns,
    };
    const uint64_t sqlite_values[] = {
        sqlite->total_ns,
        sqlite->accounts_ns,
        sqlite->items_ns,
        sqlite->tags_ns,
        sqlite->support_ns,
    };
    const size_t operations[] = {
        dataset->account_count + (dataset->row_count * 2U) + support_operations,
        dataset->account_count,
        dataset->row_count,
        dataset->row_count,
        support_operations,
    };

    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        double mylite_ms = (double)mylite_values[index] / (double)nanoseconds_per_millisecond;
        double sqlite_ms = (double)sqlite_values[index] / (double)nanoseconds_per_millisecond;
        double mylite_us = (double)mylite_values[index] / (double)nanoseconds_per_microsecond /
                           (double)operations[index];
        double sqlite_us = (double)sqlite_values[index] / (double)nanoseconds_per_microsecond /
                           (double)operations[index];
        double ratio = sqlite_values[index] == 0U
                           ? 0.0
                           : (double)mylite_values[index] / (double)sqlite_values[index];

        fprintf(
            output,
            "summary,%s,%zu,mylite,load,0,%zu,0,0,0,%.3f,%.3f,%.3f,%.3f\n",
            names[index],
            dataset->row_count,
            operations[index],
            mylite_ms,
            mylite_us,
            mylite_values[index] == 0U
                ? 0.0
                : (double)operations[index] * (double)nanoseconds_per_second /
                      (double)mylite_values[index],
            ratio
        );
        fprintf(
            output,
            "summary,%s,%zu,sqlite,load,0,%zu,0,0,0,%.3f,%.3f,%.3f,1.000\n",
            names[index],
            dataset->row_count,
            operations[index],
            sqlite_ms,
            sqlite_us,
            sqlite_values[index] == 0U
                ? 0.0
                : (double)operations[index] * (double)nanoseconds_per_second /
                      (double)sqlite_values[index]
        );
    }
}

static int run_scenarios(
    FILE *output,
    struct benchmark_database *mylite,
    struct benchmark_database *sqlite,
    const struct benchmark_dataset *dataset,
    const struct benchmark_options *options
) {
    bool matched = options->scenario_name == NULL;

    for (size_t index = 0U; index < sizeof(benchmark_scenarios) / sizeof(benchmark_scenarios[0]);
         ++index) {
        const struct benchmark_scenario *scenario = &benchmark_scenarios[index];

        if (options->scenario_name != NULL && strcmp(options->scenario_name, scenario->name) != 0) {
            continue;
        }
        matched = true;
        fprintf(
            stderr,
            "large-dataset: scenario=%s iterations=%zu samples=%zu\n",
            scenario->name,
            scenario_iterations(scenario, options),
            options->sample_count
        );
        if (run_scenario_samples(output, scenario, mylite, sqlite, dataset, options) != 0) {
            return 1;
        }
        fflush(output);
    }
    if (!matched) {
        fprintf(stderr, "large-dataset: unknown scenario %s\n", options->scenario_name);
        return 1;
    }
    return 0;
}

static int run_scenario_samples(
    FILE *output,
    const struct benchmark_scenario *scenario,
    struct benchmark_database *mylite,
    struct benchmark_database *sqlite,
    const struct benchmark_dataset *dataset,
    const struct benchmark_options *options
) {
    struct benchmark_sample_pair *samples = calloc(options->sample_count, sizeof(*samples));
    size_t iterations = scenario_iterations(scenario, options);
    int result = 1;

    if (samples == NULL) {
        fprintf(stderr, "large-dataset: failed to allocate samples\n");
        return 1;
    }
    for (size_t sample = 0U; sample < options->sample_count; ++sample) {
        struct benchmark_database *first = sample % 2U == 0U ? mylite : sqlite;
        struct benchmark_database *second = sample % 2U == 0U ? sqlite : mylite;
        struct benchmark_measurement *first_measurement = first->kind == benchmark_engine_mylite
                                                              ? &samples[sample].mylite
                                                              : &samples[sample].sqlite;
        struct benchmark_measurement *second_measurement = second->kind == benchmark_engine_mylite
                                                               ? &samples[sample].mylite
                                                               : &samples[sample].sqlite;

        if (run_scenario_engine(
                first,
                scenario,
                dataset,
                iterations,
                options->warmup_iterations,
                first_measurement
            ) != 0 ||
            run_scenario_engine(
                second,
                scenario,
                dataset,
                iterations,
                options->warmup_iterations,
                second_measurement
            ) != 0 ||
            verify_sample_pair(scenario, sample, &samples[sample]) != 0) {
            goto cleanup;
        }
        print_sample(
            output,
            scenario,
            dataset,
            benchmark_engine_mylite,
            sample + 1U,
            iterations,
            &samples[sample].mylite
        );
        print_sample(
            output,
            scenario,
            dataset,
            benchmark_engine_sqlite,
            sample + 1U,
            iterations,
            &samples[sample].sqlite
        );
    }
    print_summary(output, scenario, dataset, iterations, samples, options->sample_count);
    result = 0;

cleanup:
    free(samples);
    return result;
}

static int run_scenario_engine(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iterations,
    size_t warmup_iterations,
    struct benchmark_measurement *out_measurement
) {
    if (scenario->mode == benchmark_execution_bulk_import_rollback) {
        const struct benchmark_bulk_import_options options = {
            .row_count = iterations,
            .warmup_iterations = warmup_iterations,
        };

        return run_bulk_import_engine(database, scenario, &options, out_measurement);
    }
    return run_statement_scenario_engine(
        database,
        scenario,
        dataset,
        (struct benchmark_statement_run_options){
            .iterations = iterations,
            .warmup_iterations = warmup_iterations,
        },
        out_measurement
    );
}

static int run_statement_scenario_engine(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    struct benchmark_statement_run_options options,
    struct benchmark_measurement *out_measurement
) {
    struct benchmark_statement statement = {0};
    struct benchmark_statement *prepared_statement = NULL;
    uint64_t started = 0U;
#ifdef MYLITE_ENABLE_PROFILING
    bool profile_started = false;
#endif
    int result = 1;

    if (prepare_statement_scenario(
            database,
            scenario,
            dataset,
            options.warmup_iterations,
            &statement,
            &prepared_statement
        ) != 0) {
        goto cleanup;
    }
    *out_measurement = (struct benchmark_measurement){0};
#ifdef MYLITE_ENABLE_PROFILING
    if (start_statement_scenario_profile(database, scenario, &profile_started) != 0) {
        goto cleanup;
    }
#endif
    started = monotonic_now_ns();
    if (run_scenario_phase(
            database,
            scenario,
            dataset,
            prepared_statement,
            options.iterations,
            out_measurement
        ) != 0) {
        if (scenario_uses_rollback(scenario)) {
            (void)rollback_transaction(database);
        }
#ifdef MYLITE_ENABLE_PROFILING
        if (profile_started) {
            (void)mylite_profile_stop(database->mylite, &out_measurement->profile);
            profile_started = false;
        }
#endif
        goto cleanup;
    }
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
#ifdef MYLITE_ENABLE_PROFILING
    if (finish_statement_scenario_profile(
            database,
            scenario,
            options.iterations,
            out_measurement,
            &profile_started
        ) != 0) {
        goto cleanup;
    }
#endif
    if (scenario_uses_rollback(scenario) && rollback_transaction(database) != 0) {
        goto cleanup;
    }
    result = 0;

cleanup:
#ifdef MYLITE_ENABLE_PROFILING
    if (profile_started) {
        (void)mylite_profile_stop(database->mylite, &out_measurement->profile);
    }
#endif
    if (prepared_statement != NULL && finalize_statement(prepared_statement) != 0) {
        result = 1;
    }
    return result;
}

static int prepare_statement_scenario(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t warmup_iterations,
    struct benchmark_statement *statement,
    struct benchmark_statement **out_prepared_statement
) {
    struct benchmark_measurement warmup = {0};

    *out_prepared_statement = NULL;
    if (scenario->mode != benchmark_execution_prepare_each) {
        if (prepare_statement(database, scenario_sql(database, scenario), statement) != 0) {
            return 1;
        }
        *out_prepared_statement = statement;
    }
    if (scenario_uses_rollback(scenario) && begin_transaction(database) != 0) {
        return 1;
    }
    if (warmup_iterations > 0U && run_scenario_phase(
                                      database,
                                      scenario,
                                      dataset,
                                      *out_prepared_statement,
                                      warmup_iterations,
                                      &warmup
                                  ) != 0) {
        if (scenario_uses_rollback(scenario)) {
            (void)rollback_transaction(database);
        }
        return 1;
    }
    if (scenario_uses_rollback(scenario) &&
        (rollback_transaction(database) != 0 || begin_transaction(database) != 0)) {
        return 1;
    }
    return 0;
}

#ifdef MYLITE_ENABLE_PROFILING
static int start_statement_scenario_profile(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    bool *profile_started
) {
    if (database->kind != benchmark_engine_mylite) {
        return 0;
    }
    if (mylite_profile_start(database->mylite) != MYLITE_OK) {
        fprintf(stderr, "large-dataset: failed to start profile for %s\n", scenario->name);
        return 1;
    }
    *profile_started = true;
    return 0;
}

static int finish_statement_scenario_profile(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    size_t iterations,
    struct benchmark_measurement *measurement,
    bool *profile_started
) {
    if (!*profile_started) {
        return 0;
    }
    if (mylite_profile_stop(database->mylite, &measurement->profile) != MYLITE_OK) {
        fprintf(stderr, "large-dataset: failed to stop profile for %s\n", scenario->name);
        *profile_started = false;
        return 1;
    }
    *profile_started = false;
    fprintf(
        stderr,
        "large-dataset-profile: scenario=%s iterations=%zu total_ms=%.3f "
        "api_ms=%.3f sqlite_step_ms=%.3f metadata_step_ms=%.3f cursor_step_ms=%.3f "
        "cursor_finalize_ms=%.3f statements=%" PRIu64 " sqlite_steps=%" PRIu64
        " metadata_steps=%" PRIu64 " dml_plans=%" PRIu64 " dml_plan_hits=%" PRIu64
        " allocations=%" PRIu64 " allocation_bytes=%" PRIu64 " statement_cache_hits=%" PRIu64
        " statement_cache_misses=%" PRIu64 "\n",
        scenario->name,
        iterations,
        (double)measurement->elapsed_ns / (double)nanoseconds_per_millisecond,
        (double)measurement->profile.statement_api_ns / (double)nanoseconds_per_millisecond,
        (double)measurement->profile.sqlite_step_ns / (double)nanoseconds_per_millisecond,
        (double)measurement->profile.metadata_step_ns / (double)nanoseconds_per_millisecond,
        (double)measurement->profile.cursor_step_ns / (double)nanoseconds_per_millisecond,
        (double)measurement->profile.cursor_finalize_ns / (double)nanoseconds_per_millisecond,
        measurement->profile.statement_count,
        measurement->profile.sqlite_step_count,
        measurement->profile.metadata_step_count,
        measurement->profile.dml_plan_count,
        measurement->profile.dml_plan_cache_hit_count,
        measurement->profile.allocation_count,
        measurement->profile.allocation_bytes,
        measurement->profile.execution_statement_cache_hit_count,
        measurement->profile.execution_statement_cache_miss_count
    );
    return 0;
}
#endif

static int run_bulk_import_engine(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const struct benchmark_bulk_import_options *options,
    struct benchmark_measurement *out_measurement
) {
    char path[path_capacity];
    struct benchmark_measurement warmup = {0};
    uint64_t started = 0U;
#ifdef MYLITE_ENABLE_PROFILING
    bool profile_started = false;
#endif
    int result = 1;

    if (create_bulk_import_file(database, scenario, options->row_count, path, sizeof(path)) != 0) {
        return 1;
    }
    for (size_t index = 0U; index < options->warmup_iterations; ++index) {
        if (begin_transaction(database) != 0 ||
            run_bulk_import_once(database, scenario, path, options->row_count, &warmup) != 0 ||
            rollback_transaction(database) != 0) {
            goto cleanup;
        }
    }
    if (begin_transaction(database) != 0) {
        goto cleanup;
    }
    *out_measurement = (struct benchmark_measurement){0};
#ifdef MYLITE_ENABLE_PROFILING
    if (database->kind == benchmark_engine_mylite) {
        if (mylite_profile_start(database->mylite) != MYLITE_OK) {
            goto rollback;
        }
        profile_started = true;
    }
#endif
    started = monotonic_now_ns();
    if (run_bulk_import_once(database, scenario, path, options->row_count, out_measurement) != 0) {
        goto rollback;
    }
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
#ifdef MYLITE_ENABLE_PROFILING
    if (profile_started) {
        if (mylite_profile_stop(database->mylite, &out_measurement->profile) != MYLITE_OK) {
            profile_started = false;
            goto rollback;
        }
        fprintf(
            stderr,
            "large-dataset-profile: scenario=%s iterations=%zu total_ms=%.3f "
            "sqlite_step_ms=%.3f metadata_step_ms=%.3f sqlite_steps=%" PRIu64
            " metadata_steps=%" PRIu64 " allocations=%" PRIu64 " allocation_bytes=%" PRIu64 "\n",
            scenario->name,
            options->row_count,
            (double)out_measurement->elapsed_ns / (double)nanoseconds_per_millisecond,
            (double)out_measurement->profile.sqlite_step_ns / (double)nanoseconds_per_millisecond,
            (double)out_measurement->profile.metadata_step_ns / (double)nanoseconds_per_millisecond,
            out_measurement->profile.sqlite_step_count,
            out_measurement->profile.metadata_step_count,
            out_measurement->profile.allocation_count,
            out_measurement->profile.allocation_bytes
        );
    }
#endif
    if (rollback_transaction(database) != 0) {
        goto cleanup;
    }
    result = 0;
    goto cleanup;

rollback:
#ifdef MYLITE_ENABLE_PROFILING
    if (profile_started) {
        (void)mylite_profile_stop(database->mylite, &out_measurement->profile);
    }
#endif
    (void)rollback_transaction(database);

cleanup:
    if (remove(path) != 0 && errno != ENOENT) {
        fprintf(stderr, "large-dataset: failed to remove import file %s\n", path);
        result = 1;
    }
    return result;
}

static int create_bulk_import_file(
    const struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    size_t row_count,
    char *out_path,
    size_t out_path_size
) {
    FILE *file = NULL;
    int written = snprintf(
        out_path,
        out_path_size,
        "%s.%s.tsv",
        database->path,
        scenario->id == benchmark_scenario_load_data_index_zero ? "load0" : "load5"
    );

    if (written < 0 || (size_t)written >= out_path_size) {
        return 1;
    }
    file = fopen(out_path, "wb");
    if (file == NULL) {
        return 1;
    }
    for (size_t row = 0U; row < row_count; ++row) {
        size_t id = row + 1U;

        if (fprintf(
                file,
                "%zu\t%zu\t%zu\n",
                id,
                (id * score_multiplier) % score_modulus,
                id % category_count
            ) < 0) {
            (void)fclose(file);
            return 1;
        }
    }
    return fclose(file) == 0 ? 0 : 1;
}

static int run_bulk_import_once(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const char *path,
    size_t row_count,
    struct benchmark_measurement *measurement
) {
    int rc = database->kind == benchmark_engine_mylite
                 ? run_mylite_bulk_import(database, scenario, path, row_count, measurement)
                 : run_sqlite_bulk_import(database, scenario, path, row_count, measurement);

    if (rc == 0) {
        measurement->operation_count = row_count;
        measurement->result_row_count = 0U;
        measurement->result_value_bytes = 0U;
        measurement->checksum = fnv_offset_basis;
        hash_uint64(&measurement->checksum, measurement->affected_rows);
    }
    return rc;
}

static int run_mylite_bulk_import(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const char *path,
    size_t row_count,
    struct benchmark_measurement *measurement
) {
    char sql[import_sql_capacity];
    char escaped_path[import_sql_capacity];
    const char *table = bulk_import_table(scenario);
    mylite_result *result = NULL;
    size_t escaped_length = 0U;
    int written = 0;
    int rc = MYLITE_OK;

    for (size_t index = 0U; path[index] != '\0'; ++index) {
        if (escaped_length + 2U >= sizeof(escaped_path)) {
            return 1;
        }
        escaped_path[escaped_length++] = path[index];
        if (path[index] == '\'') {
            escaped_path[escaped_length++] = '\'';
        }
    }
    escaped_path[escaped_length] = '\0';
    written =
        snprintf(sql, sizeof(sql), "LOAD DATA INFILE '%s' INTO TABLE %s", escaped_path, table);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        return 1;
    }
    rc = mylite_execute(database->mylite, sql, (size_t)written, &result);
    if (rc != MYLITE_OK || mylite_result_affected_rows(result) != (int64_t)row_count) {
        fprintf(
            stderr,
            "large-dataset: MyLite bulk import failed: %s\n",
            mylite_errmsg(database->mylite)
        );
        mylite_result_free(result);
        return 1;
    }
    measurement->affected_rows = (uint64_t)mylite_result_affected_rows(result);
    mylite_result_free(result);
    return 0;
}

static int run_sqlite_bulk_import(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const char *path,
    size_t row_count,
    struct benchmark_measurement *measurement
) {
    char sql[generated_text_capacity];
    char line[import_line_capacity];
    sqlite3_stmt *statement = NULL;
    FILE *file = fopen(path, "rb");
    size_t imported = 0U;
    int written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO %s (id, value_a, value_b) VALUES (?, ?, ?)",
        bulk_import_table(scenario)
    );

    if (file == NULL || written < 0 || (size_t)written >= sizeof(sql) ||
        sqlite3_prepare_v2(database->sqlite, sql, written, &statement, NULL) != SQLITE_OK) {
        if (file != NULL) {
            (void)fclose(file);
        }
        sqlite3_finalize(statement);
        return 1;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        int64_t id = 0;
        int64_t value_a = 0;
        int64_t value_b = 0;
        int consumed = 0;

        if (sscanf(
                line,
                "%" SCNd64 "\t%" SCNd64 "\t%" SCNd64 "%n",
                &id,
                &value_a,
                &value_b,
                &consumed
            ) != 3 ||
            (line[consumed] != '\n' && line[consumed] != '\0') ||
            sqlite3_reset(statement) != SQLITE_OK ||
            sqlite3_clear_bindings(statement) != SQLITE_OK ||
            sqlite3_bind_int64(statement, 1, id) != SQLITE_OK ||
            sqlite3_bind_int64(statement, 2, value_a) != SQLITE_OK ||
            sqlite3_bind_int64(statement, 3, value_b) != SQLITE_OK ||
            sqlite3_step(statement) != SQLITE_DONE) {
            (void)fclose(file);
            sqlite3_finalize(statement);
            return 1;
        }
        ++imported;
    }
    int file_error = ferror(file);
    int close_rc = fclose(file);
    int finalize_rc = sqlite3_finalize(statement);

    if (file_error != 0 || close_rc != 0 || finalize_rc != SQLITE_OK || imported != row_count) {
        return 1;
    }
    measurement->affected_rows = imported;
    return 0;
}

static const char *bulk_import_table(const struct benchmark_scenario *scenario) {
    return scenario->id == benchmark_scenario_load_data_index_zero ? "insert_index_0"
                                                                   : "insert_index_5";
}

static const char *scenario_sql(
    const struct benchmark_database *database,
    const struct benchmark_scenario *scenario
) {
    static const char sqlite_upsert_sql[] =
        "INSERT INTO upsert_targets (id, value_text) VALUES (?, ?) "
        "ON CONFLICT(id) DO UPDATE SET value_text = excluded.value_text";

    if (database->kind == benchmark_engine_sqlite &&
        (scenario->id == benchmark_scenario_upsert_hit ||
         scenario->id == benchmark_scenario_upsert_miss)) {
        return sqlite_upsert_sql;
    }
    return scenario->sql;
}

static int run_scenario_phase(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    struct benchmark_statement *prepared_statement,
    size_t iterations,
    struct benchmark_measurement *out_measurement
) {
    out_measurement->checksum = fnv_offset_basis;

    for (size_t iteration = 0U; iteration < iterations; ++iteration) {
        if (execute_scenario_iteration(
                database,
                scenario,
                dataset,
                prepared_statement,
                iteration,
                out_measurement
            ) != 0) {
            return 1;
        }
        ++out_measurement->operation_count;
    }
    return 0;
}

static int execute_scenario_iteration(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    struct benchmark_statement *prepared_statement,
    size_t iteration,
    struct benchmark_measurement *measurement
) {
    struct benchmark_statement local_statement = {0};
    struct benchmark_statement *statement = prepared_statement;
    bool local = false;
    int result = 1;

    if (statement == NULL) {
        if (prepare_statement(database, scenario_sql(database, scenario), &local_statement) != 0) {
            return 1;
        }
        statement = &local_statement;
        local = true;
    }
    if (reset_statement(statement) != 0) {
        fprintf(
            stderr,
            "large-dataset: %s reset failed for %s\n",
            engine_name(database->kind),
            scenario->name
        );
        goto cleanup;
    }
    if (bind_scenario_parameters(statement, scenario, dataset, iteration) != 0) {
        fprintf(
            stderr,
            "large-dataset: %s bind failed for %s\n",
            engine_name(database->kind),
            scenario->name
        );
        goto cleanup;
    }
    if (scenario->mode == benchmark_execution_expected_error_rollback) {
        if (consume_expected_error(database, statement, measurement) != 0) {
            goto cleanup;
        }
    } else if (consume_statement(
                   database,
                   statement,
                   scenario_includes_affected_rows(scenario),
                   measurement
               ) != 0) {
        goto cleanup;
    }
    result = 0;

cleanup:
    if (local && finalize_statement(statement) != 0) {
        result = 1;
    }
    return result;
}

static int bind_scenario_parameters(
    struct benchmark_statement *statement,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iteration
) {
    if (scenario->id >= benchmark_scenario_result_narrow_small) {
        return 0;
    }
    if (scenario->id == benchmark_scenario_foreign_key_insert ||
        scenario->id == benchmark_scenario_foreign_key_cascade) {
        return bind_write_scenario_parameters(statement, scenario, dataset, iteration);
    }
    if (scenario->id < benchmark_scenario_selectivity_zero) {
        return bind_core_scenario_parameters(statement, scenario, dataset, iteration);
    }
    if (scenario->id <= benchmark_scenario_skew_cold) {
        return bind_read_scenario_parameters(statement, scenario, dataset, iteration);
    }
    return bind_write_scenario_parameters(statement, scenario, dataset, iteration);
}

static int bind_core_scenario_parameters(
    struct benchmark_statement *statement,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iteration
) {
    size_t pseudo_random = (iteration * score_multiplier) + pseudo_random_increment;
    int rc = 0;

    switch (scenario->id) {
    case benchmark_scenario_point_lookup_prepare_each:
    case benchmark_scenario_point_lookup_prepared:
    case benchmark_scenario_indexed_update:
        return bind_int64(statement, 0U, (int64_t)((pseudo_random % dataset->row_count) + 1U));
    case benchmark_scenario_secondary_lookup:
        return bind_int64(statement, 0U, (int64_t)((pseudo_random % dataset->account_count) + 1U));
    case benchmark_scenario_range_aggregate:
        rc = bind_int64(statement, 0U, (int64_t)(iteration % category_count));
        if (rc == 0) {
            rc = bind_int64(statement, 1U, score_range_lower);
        }
        if (rc == 0) {
            rc = bind_int64(statement, 2U, score_range_upper);
        }
        return rc;
    case benchmark_scenario_full_scan_expression:
    case benchmark_scenario_group_aggregate:
        return 0;
    case benchmark_scenario_text_expression:
    case benchmark_scenario_indexed_order_limit:
        return bind_text(statement, 0U, "published");
    case benchmark_scenario_parent_join:
        return bind_int64(statement, 0U, (int64_t)(iteration % account_region_count));
    case benchmark_scenario_bridge_join: {
        size_t lower = iteration % dataset->tag_count;
        size_t upper = lower + bridge_tag_span;

        if (upper >= dataset->tag_count) {
            upper = dataset->tag_count - 1U;
        }
        rc = bind_int64(statement, 0U, (int64_t)lower);
        if (rc == 0) {
            rc = bind_int64(statement, 1U, (int64_t)upper);
        }
        return rc;
    }
    case benchmark_scenario_correlated_exists:
        return bind_int64(statement, 0U, correlated_score_threshold);
    default:
        return 1;
    }
}

static int bind_read_scenario_parameters(
    struct benchmark_statement *statement,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iteration
) {
    size_t pseudo_random = (iteration * score_multiplier) + pseudo_random_increment;
    int rc = 0;

    switch (scenario->id) {
    case benchmark_scenario_selectivity_zero:
        rc = bind_int64(statement, 0U, (int64_t)dataset->row_count + 1);
        if (rc == 0) {
            rc = bind_int64(statement, 1U, (int64_t)dataset->row_count);
        }
        return rc;
    case benchmark_scenario_selectivity_one: {
        int64_t id = (int64_t)((pseudo_random % dataset->row_count) + 1U);

        rc = bind_int64(statement, 0U, id);
        if (rc == 0) {
            rc = bind_int64(statement, 1U, id);
        }
        return rc;
    }
    case benchmark_scenario_selectivity_one_basis_point:
        return bind_id_range_parameters(statement, iteration, dataset, selectivity_one_basis_point);
    case benchmark_scenario_selectivity_one_percent:
        return bind_id_range_parameters(
            statement,
            iteration,
            dataset,
            selectivity_one_percent_basis_points
        );
    case benchmark_scenario_selectivity_ten_percent:
        return bind_id_range_parameters(
            statement,
            iteration,
            dataset,
            selectivity_ten_percent_basis_points
        );
    case benchmark_scenario_selectivity_full:
        return bind_id_range_parameters(statement, iteration, dataset, selectivity_basis_points);
    case benchmark_scenario_covering_range:
    case benchmark_scenario_noncovering_range:
        rc = bind_int64(statement, 0U, (int64_t)(iteration % category_count));
        if (rc == 0) {
            rc = bind_int64(statement, 1U, score_range_lower);
        }
        if (rc == 0) {
            rc = bind_int64(statement, 2U, score_range_upper);
        }
        return rc;
    case benchmark_scenario_or_lookup:
        rc = bind_int64(statement, 0U, (int64_t)((pseudo_random % dataset->row_count) + 1U));
        if (rc == 0) {
            rc = bind_int64(
                statement,
                1U,
                (int64_t)(((pseudo_random + pseudo_random_increment) % dataset->row_count) + 1U)
            );
        }
        if (rc == 0) {
            rc = bind_int64(
                statement,
                2U,
                (int64_t)(((pseudo_random +
                            ((size_t)pseudo_random_increment * or_lookup_third_offset_multiplier)) %
                           dataset->row_count) +
                          1U)
            );
        }
        return rc;
    case benchmark_scenario_deep_offset:
        rc = bind_int64(statement, 0U, deep_offset_limit);
        if (rc == 0) {
            rc = bind_int64(
                statement,
                1U,
                (int64_t)((dataset->row_count * deep_offset_numerator) / deep_offset_denominator)
            );
        }
        return rc;
    case benchmark_scenario_null_lookup:
    case benchmark_scenario_unindexed_sort_limit:
    case benchmark_scenario_high_cardinality_group:
    case benchmark_scenario_high_cardinality_distinct:
    case benchmark_scenario_anti_join:
        return 0;
    case benchmark_scenario_window_partition:
        rc = bind_int64(statement, 0U, 1);
        if (rc == 0) {
            size_t upper = dataset->account_count < window_partition_account_limit
                               ? dataset->account_count
                               : window_partition_account_limit;

            rc = bind_int64(statement, 1U, (int64_t)upper);
        }
        return rc;
    case benchmark_scenario_three_table_join:
        return bind_int64(statement, 0U, (int64_t)(iteration % account_region_count));
    case benchmark_scenario_left_join:
    case benchmark_scenario_large_large_join:
        return bind_id_range_parameters(
            statement,
            iteration,
            dataset,
            selectivity_one_percent_basis_points
        );
    case benchmark_scenario_skew_hot:
        return bind_int64(statement, 0U, 0);
    case benchmark_scenario_skew_cold:
        return bind_int64(
            statement,
            0U,
            (int64_t)(((iteration * pseudo_random_increment) % skew_tenant_count) + 1U)
        );
    default:
        return 1;
    }
}

static int bind_write_scenario_parameters(
    struct benchmark_statement *statement,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iteration
) {
    if (scenario->id == benchmark_scenario_foreign_key_insert ||
        scenario->id == benchmark_scenario_foreign_key_cascade ||
        scenario->id >= benchmark_scenario_composite_foreign_key_insert) {
        return bind_foreign_key_scenario_parameters(statement, scenario, dataset, iteration);
    }
    size_t pseudo_random = (iteration * score_multiplier) + pseudo_random_increment;
    int rc = 0;

    switch (scenario->id) {
    case benchmark_scenario_update_one_basis_point:
    case benchmark_scenario_delete_one_basis_point:
        return bind_id_range_parameters(statement, iteration, dataset, selectivity_one_basis_point);
    case benchmark_scenario_update_one_percent:
    case benchmark_scenario_delete_one_percent:
        return bind_id_range_parameters(
            statement,
            iteration,
            dataset,
            selectivity_one_percent_basis_points
        );
    case benchmark_scenario_update_ten_percent:
        return bind_id_range_parameters(
            statement,
            iteration,
            dataset,
            selectivity_ten_percent_basis_points
        );
    case benchmark_scenario_upsert_hit:
    case benchmark_scenario_upsert_miss: {
        char value_text[generated_text_capacity];
        size_t id = scenario->id == benchmark_scenario_upsert_hit
                        ? ((pseudo_random % dataset->account_count) + 1U)
                        : ((dataset->row_count * write_log_id_factor) + iteration + 1U);
        int written = snprintf(value_text, sizeof(value_text), "updated-%010zu", iteration);

        if (written < 0 || (size_t)written >= sizeof(value_text)) {
            return 1;
        }
        rc = bind_int64(statement, 0U, (int64_t)id);
        if (rc == 0) {
            rc = bind_text(statement, 1U, value_text);
        }
        return rc;
    }
    case benchmark_scenario_insert_index_zero:
    case benchmark_scenario_insert_index_one:
    case benchmark_scenario_insert_index_five:
    case benchmark_scenario_insert_index_ten: {
        size_t id = (dataset->row_count * write_log_id_factor) + iteration + 1U;

        rc = bind_int64(statement, 0U, (int64_t)id);
        if (rc == 0) {
            rc = bind_int64(statement, 1U, (int64_t)(pseudo_random % score_modulus));
        }
        if (rc == 0) {
            rc = bind_int64(statement, 2U, (int64_t)(id % category_count));
        }
        return rc;
    }
    case benchmark_scenario_insert_batch_ten:
        for (size_t row = 0U; row < batch_insert_count && rc == 0; ++row) {
            size_t id = (dataset->row_count * write_log_id_factor) +
                        (iteration * batch_insert_count) + row + 1U;
            size_t parameter = row * 3U;

            rc = bind_int64(statement, parameter, (int64_t)id);
            if (rc == 0) {
                rc = bind_int64(
                    statement,
                    parameter + 1U,
                    (int64_t)((pseudo_random + row) % score_modulus)
                );
            }
            if (rc == 0) {
                rc = bind_int64(statement, parameter + 2U, (int64_t)(id % category_count));
            }
        }
        return rc;
    default:
        return 1;
    }
}

static int bind_foreign_key_scenario_parameters(
    struct benchmark_statement *statement,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iteration
) {
    size_t pseudo_random = (iteration * score_multiplier) + pseudo_random_increment;
    int rc = 0;

    switch (scenario->id) {
    case benchmark_scenario_foreign_key_insert: {
        char note[generated_text_capacity];
        int written = snprintf(note, sizeof(note), "write-%010zu", iteration);

        if (written < 0 || (size_t)written >= sizeof(note)) {
            return 1;
        }
        rc = bind_int64(
            statement,
            0U,
            (int64_t)((dataset->row_count * write_log_id_factor) + iteration + 1U)
        );
        if (rc == 0) {
            rc = bind_int64(statement, 1U, (int64_t)((pseudo_random % dataset->row_count) + 1U));
        }
        if (rc == 0) {
            rc = bind_text(statement, 2U, note);
        }
        return rc;
    }
    case benchmark_scenario_foreign_key_cascade:
        return bind_int64(statement, 0U, (int64_t)((pseudo_random % dataset->row_count) + 1U));
    case benchmark_scenario_composite_foreign_key_insert:
    case benchmark_scenario_composite_foreign_key_invalid: {
        size_t parent_id = (pseudo_random % dataset->account_count) + 1U;
        size_t id = (dataset->row_count * write_log_id_factor) + iteration + 1U;

        rc = bind_int64(statement, 0U, (int64_t)id);
        if (rc == 0) {
            rc = bind_int64(statement, 1U, (int64_t)parent_id);
        }
        if (rc == 0) {
            rc = bind_int64(
                statement,
                2U,
                scenario->id == benchmark_scenario_composite_foreign_key_invalid
                    ? (int64_t)account_region_count
                    : (int64_t)(parent_id % account_region_count)
            );
        }
        if (rc == 0) {
            rc = bind_int64(statement, 3U, (int64_t)(pseudo_random % score_modulus));
        }
        return rc;
    }
    case benchmark_scenario_foreign_key_cascade_fanout:
    case benchmark_scenario_foreign_key_set_null:
        return bind_int64(
            statement,
            0U,
            (int64_t)((iteration % fanout_parent_count_for_rows(dataset->row_count)) + 1U)
        );
    case benchmark_scenario_foreign_key_restrict:
        return bind_int64(statement, 0U, (int64_t)fanout_parent_count_for_rows(dataset->row_count));
    default:
        return 1;
    }
}

static int bind_id_range_parameters(
    struct benchmark_statement *statement,
    size_t iteration,
    const struct benchmark_dataset *dataset,
    unsigned int basis_points
) {
    size_t count = range_count_for_basis_points(dataset->row_count, basis_points);
    size_t available_starts = dataset->row_count - count + 1U;
    size_t lower = ((iteration * score_multiplier) % available_starts) + 1U;
    size_t upper = lower + count - 1U;
    int rc = bind_int64(statement, 0U, (int64_t)lower);

    if (rc == 0) {
        rc = bind_int64(statement, 1U, (int64_t)upper);
    }
    return rc;
}

static int consume_statement(
    struct benchmark_database *database,
    struct benchmark_statement *statement,
    bool include_affected_rows,
    struct benchmark_measurement *measurement
) {
    if (statement->kind == benchmark_engine_mylite) {
        return consume_mylite_statement(database, statement, include_affected_rows, measurement);
    }
    return consume_sqlite_statement(statement, include_affected_rows, measurement);
}

static int consume_expected_error(
    struct benchmark_database *database,
    struct benchmark_statement *statement,
    struct benchmark_measurement *measurement
) {
    if (statement->kind == benchmark_engine_mylite) {
        int rc = mylite_stmt_step(statement->mylite);

        if (rc == MYLITE_ROW || rc == MYLITE_DONE) {
            fprintf(stderr, "large-dataset: MyLite unexpectedly accepted invalid foreign key\n");
            return 1;
        }
        if (mylite_stmt_reset(statement->mylite) != MYLITE_OK) {
            return 1;
        }
    } else {
        int rc = sqlite3_step(statement->sqlite);

        if (rc == SQLITE_ROW || rc == SQLITE_DONE) {
            fprintf(
                stderr,
                "large-dataset: SQLite unexpectedly accepted invalid foreign key: %s\n",
                sqlite3_errmsg(database->sqlite)
            );
            return 1;
        }
        (void)sqlite3_reset(statement->sqlite);
    }
    hash_uint64(&measurement->checksum, 1U);
    return 0;
}

static int consume_mylite_statement(
    struct benchmark_database *database,
    struct benchmark_statement *statement,
    bool include_affected_rows,
    struct benchmark_measurement *measurement
) {
    for (;;) {
        int rc = mylite_stmt_step(statement->mylite);

        if (rc == MYLITE_ROW) {
            size_t columns = mylite_stmt_column_count(statement->mylite);

            ++measurement->result_row_count;
            hash_uint64(&measurement->checksum, columns);
            for (size_t column = 0U; column < columns; ++column) {
                if (mylite_stmt_value_is_null(statement->mylite, column) != 0) {
                    hash_uint64(&measurement->checksum, UINT64_MAX);
                } else {
                    const void *bytes = mylite_stmt_value_bytes(statement->mylite, column);
                    size_t size = mylite_stmt_value_size(statement->mylite, column);

                    hash_uint64(&measurement->checksum, size);
                    measurement->checksum = hash_bytes(measurement->checksum, bytes, size);
                    measurement->result_value_bytes += size;
                }
            }
            continue;
        }
        if (rc != MYLITE_DONE) {
            fprintf(
                stderr,
                "large-dataset: MyLite step failed: %d %s\n",
                rc,
                mylite_errmsg(database->mylite)
            );
            return 1;
        }
        if (include_affected_rows) {
            uint64_t affected_rows = (uint64_t)mylite_stmt_affected_rows(statement->mylite);

            measurement->affected_rows += affected_rows;
            hash_uint64(&measurement->checksum, affected_rows);
        }
        return 0;
    }
}

static int consume_sqlite_statement(
    struct benchmark_statement *statement,
    bool include_affected_rows,
    struct benchmark_measurement *measurement
) {
    for (;;) {
        int rc = sqlite3_step(statement->sqlite);

        if (rc == SQLITE_ROW) {
            int columns = sqlite3_column_count(statement->sqlite);

            ++measurement->result_row_count;
            hash_uint64(&measurement->checksum, (uint64_t)columns);
            for (int column = 0; column < columns; ++column) {
                if (sqlite3_column_type(statement->sqlite, column) == SQLITE_NULL) {
                    hash_uint64(&measurement->checksum, UINT64_MAX);
                } else {
                    const void *bytes = sqlite3_column_blob(statement->sqlite, column);
                    int byte_count = sqlite3_column_bytes(statement->sqlite, column);
                    size_t size = byte_count < 0 ? 0U : (size_t)byte_count;

                    hash_uint64(&measurement->checksum, size);
                    measurement->checksum = hash_bytes(measurement->checksum, bytes, size);
                    measurement->result_value_bytes += size;
                }
            }
            continue;
        }
        if (rc != SQLITE_DONE) {
            fprintf(
                stderr,
                "large-dataset: SQLite step failed: %s\n",
                sqlite3_errmsg(sqlite3_db_handle(statement->sqlite))
            );
            return 1;
        }
        if (include_affected_rows) {
            uint64_t affected_rows =
                (uint64_t)sqlite3_changes64(sqlite3_db_handle(statement->sqlite));

            measurement->affected_rows += affected_rows;
            hash_uint64(&measurement->checksum, affected_rows);
        }
        return 0;
    }
}

static void print_sample(
    FILE *output,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    enum benchmark_engine_kind engine,
    size_t sample,
    size_t iterations,
    const struct benchmark_measurement *measurement
) {
    double total_ms = (double)measurement->elapsed_ns / (double)nanoseconds_per_millisecond;
    double average_us =
        (double)measurement->elapsed_ns / (double)nanoseconds_per_microsecond / (double)iterations;
    double operations_per_second =
        measurement->elapsed_ns == 0U
            ? 0.0
            : (double)iterations * (double)nanoseconds_per_second / (double)measurement->elapsed_ns;

    fprintf(
        output,
        "sample,%s,%zu,%s,%s,%zu,%zu,%zu,%zu,%" PRIu64 ",%.3f,%.3f,%.3f,0.000\n",
        scenario->name,
        dataset->row_count,
        engine_name(engine),
        mode_name(scenario->mode),
        sample,
        iterations,
        measurement->result_row_count,
        measurement->result_value_bytes,
        measurement->checksum,
        total_ms,
        average_us,
        operations_per_second
    );
}

static void print_summary(
    FILE *output,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iterations,
    const struct benchmark_sample_pair *samples,
    size_t sample_count
) {
    double mylite_us = median_elapsed_us(benchmark_engine_mylite, samples, sample_count);
    double sqlite_us = median_elapsed_us(benchmark_engine_sqlite, samples, sample_count);
    double ratio = sqlite_us == 0.0 ? 0.0 : mylite_us / sqlite_us;

    fprintf(
        output,
        "summary,%s,%zu,mylite,%s,0,%zu,0,0,0,%.3f,%.3f,%.3f,%.3f\n",
        scenario->name,
        dataset->row_count,
        mode_name(scenario->mode),
        iterations,
        mylite_us * (double)iterations / (double)milliseconds_per_second,
        mylite_us,
        mylite_us == 0.0 ? 0.0 : (double)microseconds_per_second / mylite_us,
        ratio
    );
    fprintf(
        output,
        "summary,%s,%zu,sqlite,%s,0,%zu,0,0,0,%.3f,%.3f,%.3f,1.000\n",
        scenario->name,
        dataset->row_count,
        mode_name(scenario->mode),
        iterations,
        sqlite_us * (double)iterations / (double)milliseconds_per_second,
        sqlite_us,
        sqlite_us == 0.0 ? 0.0 : (double)microseconds_per_second / sqlite_us
    );
}

static int verify_sample_pair(
    const struct benchmark_scenario *scenario,
    size_t sample,
    const struct benchmark_sample_pair *pair
) {
    if (pair->mylite.operation_count != pair->sqlite.operation_count ||
        pair->mylite.result_row_count != pair->sqlite.result_row_count ||
        pair->mylite.result_value_bytes != pair->sqlite.result_value_bytes ||
        pair->mylite.affected_rows != pair->sqlite.affected_rows ||
        pair->mylite.checksum != pair->sqlite.checksum) {
        fprintf(
            stderr,
            "large-dataset: %s sample %zu result mismatch\n"
            "  MyLite operations=%zu rows=%zu bytes=%zu affected=%" PRIu64 " checksum=%" PRIu64 "\n"
            "  SQLite operations=%zu rows=%zu bytes=%zu affected=%" PRIu64 " checksum=%" PRIu64
            "\n",
            scenario->name,
            sample + 1U,
            pair->mylite.operation_count,
            pair->mylite.result_row_count,
            pair->mylite.result_value_bytes,
            pair->mylite.affected_rows,
            pair->mylite.checksum,
            pair->sqlite.operation_count,
            pair->sqlite.result_row_count,
            pair->sqlite.result_value_bytes,
            pair->sqlite.affected_rows,
            pair->sqlite.checksum
        );
        return 1;
    }
    return 0;
}

static int begin_transaction(struct benchmark_database *database) {
    if (engine_uses_mylite_api(database->kind)) {
        mylite_result *result = NULL;
        int rc = mylite_execute_transaction_control(
            database->mylite,
            MYLITE_TRANSACTION_CONTROL_START,
            &result
        );

        mylite_result_free(result);
        if (rc != MYLITE_OK) {
            fprintf(stderr, "large-dataset: MyLite transaction start failed\n");
            return 1;
        }
        return 0;
    }
    return execute_sql(database, "BEGIN");
}

static int commit_transaction(struct benchmark_database *database) {
    if (engine_uses_mylite_api(database->kind)) {
        mylite_result *result = NULL;
        int rc = mylite_execute_transaction_control(
            database->mylite,
            MYLITE_TRANSACTION_CONTROL_COMMIT,
            &result
        );

        mylite_result_free(result);
        if (rc != MYLITE_OK) {
            fprintf(stderr, "large-dataset: MyLite transaction commit failed\n");
            return 1;
        }
        return 0;
    }
    return execute_sql(database, "COMMIT");
}

static int rollback_transaction(struct benchmark_database *database) {
    if (engine_uses_mylite_api(database->kind)) {
        mylite_result *result = NULL;
        int rc = mylite_execute_transaction_control(
            database->mylite,
            MYLITE_TRANSACTION_CONTROL_ROLLBACK,
            &result
        );

        mylite_result_free(result);
        if (rc != MYLITE_OK) {
            fprintf(stderr, "large-dataset: MyLite transaction rollback failed\n");
            return 1;
        }
        return 0;
    }
    return execute_sql(database, "ROLLBACK");
}

static int prepare_statement(
    struct benchmark_database *database,
    const char *sql,
    struct benchmark_statement *out_statement
) {
    const struct attribution_program *attribution_program = NULL;

    *out_statement = (struct benchmark_statement){
        .kind = database->kind,
        .mylite = NULL,
        .sqlite = NULL,
#ifdef MYLITE_ENABLE_PROFILING
        .manual_profile = database->manual_profile,
#endif
    };
    if (engine_uses_mylite_api(database->kind)) {
        int rc = mylite_prepare(database->mylite, sql, strlen(sql), &out_statement->mylite);

        if (rc != MYLITE_OK) {
            fprintf(
                stderr,
                "large-dataset: MyLite prepare failed: %s\nSQL: %s\n",
                mylite_errmsg(database->mylite),
                sql
            );
            return 1;
        }
        return 0;
    }
    if (engine_uses_direct_sqlite(database->kind)) {
        attribution_program = find_attribution_program(database->attribution_programs, sql);

        if (attribution_program == NULL) {
            fprintf(stderr, "large-dataset: no attribution program for SQL: %s\n", sql);
            return 1;
        }
        sql = database->kind == benchmark_engine_mylite_physical ? attribution_program->plain_sql
                                                                 : attribution_program->guarded_sql;
    }
    if (sqlite3_prepare_v2(database->sqlite, sql, -1, &out_statement->sqlite, NULL) != SQLITE_OK) {
        fprintf(
            stderr,
            "large-dataset: SQLite prepare failed: %s\nSQL: %s\n",
            sqlite3_errmsg(database->sqlite),
            sql
        );
        return 1;
    }
    if (attribution_program != NULL && (size_t)sqlite3_bind_parameter_count(out_statement->sqlite
                                       ) != attribution_program->parameter_count) {
        fprintf(stderr, "large-dataset: attribution parameter count drift for %s\n", sql);
        (void)sqlite3_finalize(out_statement->sqlite);
        out_statement->sqlite = NULL;
        return 1;
    }
    return 0;
}

static int reset_statement(struct benchmark_statement *statement) {
    if (engine_uses_mylite_api(statement->kind)) {
        return mylite_stmt_reset(statement->mylite) == MYLITE_OK ? 0 : 1;
    }
    /*
     * sqlite3_reset() returns the prior step status. That status was already
     * checked by the caller and may intentionally be a constraint error.
     */
    (void)sqlite3_reset(statement->sqlite);
    if (sqlite3_clear_bindings(statement->sqlite) != SQLITE_OK) {
        return 1;
    }
    return 0;
}

static int finalize_statement(struct benchmark_statement *statement) {
    int rc = 0;

    if (engine_uses_mylite_api(statement->kind)) {
        rc = mylite_stmt_finalize(statement->mylite);
        statement->mylite = NULL;
        if (rc != MYLITE_OK) {
            fprintf(stderr, "large-dataset: MyLite finalize failed: %d\n", rc);
        }
        return rc == MYLITE_OK ? 0 : 1;
    }
    rc = sqlite3_finalize(statement->sqlite);
    statement->sqlite = NULL;
    if (rc != SQLITE_OK) {
        fprintf(stderr, "large-dataset: SQLite finalize failed: %d\n", rc);
    }
    return rc == SQLITE_OK ? 0 : 1;
}

static int bind_int64(struct benchmark_statement *statement, size_t index, int64_t value) {
    if (engine_uses_mylite_api(statement->kind)) {
        return mylite_stmt_bind_int64(statement->mylite, index, value) == MYLITE_OK ? 0 : 1;
    }
    return sqlite3_bind_int64(statement->sqlite, (int)index + 1, value) == SQLITE_OK ? 0 : 1;
}

static int bind_null(struct benchmark_statement *statement, size_t index) {
    if (engine_uses_mylite_api(statement->kind)) {
        return mylite_stmt_bind_null(statement->mylite, index) == MYLITE_OK ? 0 : 1;
    }
    return sqlite3_bind_null(statement->sqlite, (int)index + 1) == SQLITE_OK ? 0 : 1;
}

static int bind_text(struct benchmark_statement *statement, size_t index, const char *value) {
    if (engine_uses_mylite_api(statement->kind)) {
        return mylite_stmt_bind_text(statement->mylite, index, value, strlen(value)) == MYLITE_OK
                   ? 0
                   : 1;
    }
    return sqlite3_bind_text(statement->sqlite, (int)index + 1, value, -1, SQLITE_TRANSIENT) ==
                   SQLITE_OK
               ? 0
               : 1;
}

static int execute_sql(struct benchmark_database *database, const char *sql) {
    if (engine_uses_mylite_api(database->kind)) {
        mylite_result *result = NULL;
        int rc = mylite_execute(database->mylite, sql, strlen(sql), &result);

        mylite_result_free(result);
        if (rc != MYLITE_OK) {
            fprintf(
                stderr,
                "large-dataset: MyLite SQL failed: %s\nSQL: %s\n",
                mylite_errmsg(database->mylite),
                sql
            );
            return 1;
        }
        return 0;
    }
    if (sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(
            stderr,
            "large-dataset: SQLite SQL failed: %s\nSQL: %s\n",
            sqlite3_errmsg(database->sqlite),
            sql
        );
        return 1;
    }
    return 0;
}

static int fetch_scalar_count(
    struct benchmark_database *database,
    const char *sql,
    uint64_t *out_value
) {
    struct benchmark_statement statement = {0};
    int result = 1;

    *out_value = 0U;
    if (prepare_statement(database, sql, &statement) != 0 || reset_statement(&statement) != 0) {
        return 1;
    }
    if (engine_uses_mylite_api(database->kind)) {
        if (mylite_stmt_step(statement.mylite) != MYLITE_ROW) {
            goto cleanup;
        }
        *out_value = strtoull(mylite_stmt_value_text(statement.mylite, 0U), NULL, decimal_base);
        if (mylite_stmt_step(statement.mylite) != MYLITE_DONE) {
            goto cleanup;
        }
    } else {
        if (sqlite3_step(statement.sqlite) != SQLITE_ROW) {
            goto cleanup;
        }
        *out_value = (uint64_t)sqlite3_column_int64(statement.sqlite, 0);
        if (sqlite3_step(statement.sqlite) != SQLITE_DONE) {
            goto cleanup;
        }
    }
    result = 0;

cleanup:
    if (finalize_statement(&statement) != 0) {
        result = 1;
    }
    return result;
}

static int seed_accounts(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset
) {
    struct benchmark_statement statement = {0};
    int result = 1;

    if (prepare_statement(
            database,
            "INSERT INTO accounts (id, region, name) VALUES (?, ?, ?)",
            &statement
        ) != 0) {
        return 1;
    }
    for (size_t account_id = 1U; account_id <= dataset->account_count; ++account_id) {
        if (insert_account(&statement, account_id, account_id % account_region_count) != 0) {
            goto cleanup;
        }
    }
    result = 0;

cleanup:
    if (finalize_statement(&statement) != 0) {
        result = 1;
    }
    return result;
}

static int seed_items(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset
) {
    struct benchmark_statement statement = {0};
    int result = 1;

    if (prepare_statement(
            database,
            "INSERT INTO items "
            "(id, account_id, category_id, status, score, created_at, title, payload,"
            " tenant_id, optional_value) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            &statement
        ) != 0) {
        return 1;
    }
    for (size_t item_id = 1U; item_id <= dataset->row_count; ++item_id) {
        if (insert_item(&statement, item_id, dataset) != 0) {
            goto cleanup;
        }
        if (item_id % seed_progress_interval == 0U) {
            fprintf(
                stderr,
                "large-dataset: %s seeded %zu/%zu items\n",
                engine_name(database->kind),
                item_id,
                dataset->row_count
            );
        }
    }
    result = 0;

cleanup:
    if (finalize_statement(&statement) != 0) {
        result = 1;
    }
    return result;
}

static int seed_tags(struct benchmark_database *database, const struct benchmark_dataset *dataset) {
    struct benchmark_statement statement = {0};
    int result = 1;

    if (prepare_statement(
            database,
            "INSERT INTO item_tags (item_id, tag_id, weight) VALUES (?, ?, ?)",
            &statement
        ) != 0) {
        return 1;
    }
    for (size_t item_id = 1U; item_id <= dataset->row_count; ++item_id) {
        if (insert_tag(&statement, item_id, dataset) != 0) {
            goto cleanup;
        }
    }
    result = 0;

cleanup:
    if (finalize_statement(&statement) != 0) {
        result = 1;
    }
    return result;
}

static int seed_support_tables(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset,
    struct benchmark_load_measurement *measurement
) {
    uint64_t started_ns = monotonic_now_ns();
    uint64_t started_cpu_ns = process_cpu_now_ns();

    if (seed_upsert_and_composite_parents(database, dataset) != 0) {
        return 1;
    }
    measurement->support_upsert_ns = monotonic_now_ns() - started_ns;
    measurement->support_upsert_cpu_ns = process_cpu_now_ns() - started_cpu_ns;
#ifdef MYLITE_ENABLE_PROFILING
    capture_load_profile(database, &measurement->support_upsert_profile);
#endif
    started_ns = monotonic_now_ns();
    started_cpu_ns = process_cpu_now_ns();
    if (seed_fanout_tables(database, dataset) != 0) {
        return 1;
    }
    measurement->support_fanout_ns = monotonic_now_ns() - started_ns;
    measurement->support_fanout_cpu_ns = process_cpu_now_ns() - started_cpu_ns;
#ifdef MYLITE_ENABLE_PROFILING
    capture_load_profile(database, &measurement->support_fanout_profile);
#endif
    started_ns = monotonic_now_ns();
    started_cpu_ns = process_cpu_now_ns();
    if (seed_restrict_tables(database, dataset) != 0) {
        return 1;
    }
    measurement->support_restrict_ns = monotonic_now_ns() - started_ns;
    measurement->support_restrict_cpu_ns = process_cpu_now_ns() - started_cpu_ns;
#ifdef MYLITE_ENABLE_PROFILING
    capture_load_profile(database, &measurement->support_restrict_profile);
#endif
    started_ns = monotonic_now_ns();
    started_cpu_ns = process_cpu_now_ns();
    if (seed_set_null_tables(database, dataset) != 0) {
        return 1;
    }
    measurement->support_set_null_ns = monotonic_now_ns() - started_ns;
    measurement->support_set_null_cpu_ns = process_cpu_now_ns() - started_cpu_ns;
#ifdef MYLITE_ENABLE_PROFILING
    capture_load_profile(database, &measurement->support_set_null_profile);
#endif
    return 0;
}

static int seed_upsert_and_composite_parents(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset
) {
    struct benchmark_statement upsert_statement = {0};
    struct benchmark_statement composite_statement = {0};
    char value_text[generated_text_capacity];
    int result = 1;

    if (prepare_statement(
            database,
            "INSERT INTO upsert_targets (id, value_text) VALUES (?, ?)",
            &upsert_statement
        ) != 0 ||
        prepare_statement(
            database,
            "INSERT INTO composite_parents (id, shard_id, payload) VALUES (?, ?, ?)",
            &composite_statement
        ) != 0) {
        goto cleanup;
    }
    for (size_t id = 1U; id <= dataset->account_count; ++id) {
        int written = snprintf(value_text, sizeof(value_text), "value-%010zu", id);

        if (written < 0 || (size_t)written >= sizeof(value_text) ||
            reset_statement(&upsert_statement) != 0 ||
            bind_int64(&upsert_statement, 0U, (int64_t)id) != 0 ||
            bind_text(&upsert_statement, 1U, value_text) != 0 ||
            step_write_statement(&upsert_statement) != 0 ||
            reset_statement(&composite_statement) != 0 ||
            bind_int64(&composite_statement, 0U, (int64_t)id) != 0 ||
            bind_int64(&composite_statement, 1U, (int64_t)(id % account_region_count)) != 0 ||
            bind_int64(&composite_statement, 2U, (int64_t)(id * score_multiplier)) != 0 ||
            step_write_statement(&composite_statement) != 0) {
            goto cleanup;
        }
    }
    result = 0;

cleanup:
    if ((upsert_statement.mylite != NULL || upsert_statement.sqlite != NULL) &&
        finalize_statement(&upsert_statement) != 0) {
        result = 1;
    }
    if ((composite_statement.mylite != NULL || composite_statement.sqlite != NULL) &&
        finalize_statement(&composite_statement) != 0) {
        result = 1;
    }
    return result;
}

static int seed_fanout_tables(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset
) {
    struct benchmark_statement parent_statement = {0};
    struct benchmark_statement child_statement = {0};
    size_t parent_count = fanout_parent_count_for_rows(dataset->row_count);
    int result = 1;

    if (parent_count == 0U) {
        return 1;
    }
    if (prepare_statement(
            database,
            "INSERT INTO fanout_parents (id, payload) VALUES (?, ?)",
            &parent_statement
        ) != 0 ||
        prepare_statement(
            database,
            "INSERT INTO fanout_children (id, parent_id, payload) VALUES (?, ?, ?)",
            &child_statement
        ) != 0) {
        goto cleanup;
    }
    for (size_t id = 1U; id <= parent_count; ++id) {
        if (reset_statement(&parent_statement) != 0 ||
            bind_int64(&parent_statement, 0U, (int64_t)id) != 0 ||
            bind_int64(&parent_statement, 1U, (int64_t)(id * score_multiplier)) != 0 ||
            step_write_statement(&parent_statement) != 0) {
            goto cleanup;
        }
    }
    for (size_t id = 1U; id <= dataset->row_count; ++id) {
        size_t parent_id = ((id - 1U) % parent_count) + 1U;

        if (reset_statement(&child_statement) != 0 ||
            bind_int64(&child_statement, 0U, (int64_t)id) != 0 ||
            bind_int64(&child_statement, 1U, (int64_t)parent_id) != 0 ||
            bind_int64(&child_statement, 2U, (int64_t)(id % score_modulus)) != 0 ||
            step_write_statement(&child_statement) != 0) {
            goto cleanup;
        }
    }
    result = 0;

cleanup:
    if ((parent_statement.mylite != NULL || parent_statement.sqlite != NULL) &&
        finalize_statement(&parent_statement) != 0) {
        result = 1;
    }
    if ((child_statement.mylite != NULL || child_statement.sqlite != NULL) &&
        finalize_statement(&child_statement) != 0) {
        result = 1;
    }
    return result;
}

static int seed_restrict_tables(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset
) {
    struct benchmark_statement parent_statement = {0};
    struct benchmark_statement child_statement = {0};
    size_t parent_count = fanout_parent_count_for_rows(dataset->row_count);
    int result = 1;

    if (parent_count == 0U) {
        return 1;
    }
    if (prepare_statement(
            database,
            "INSERT INTO restrict_parents (id) VALUES (?)",
            &parent_statement
        ) != 0 ||
        prepare_statement(
            database,
            "INSERT INTO restrict_children (id, parent_id) VALUES (?, ?)",
            &child_statement
        ) != 0) {
        goto cleanup;
    }
    for (size_t id = 1U; id <= parent_count; ++id) {
        if (reset_statement(&parent_statement) != 0 ||
            bind_int64(&parent_statement, 0U, (int64_t)id) != 0 ||
            step_write_statement(&parent_statement) != 0) {
            goto cleanup;
        }
        if (id < parent_count && (reset_statement(&child_statement) != 0 ||
                                  bind_int64(&child_statement, 0U, (int64_t)id) != 0 ||
                                  bind_int64(&child_statement, 1U, (int64_t)id) != 0 ||
                                  step_write_statement(&child_statement) != 0)) {
            goto cleanup;
        }
    }
    result = 0;

cleanup:
    if ((parent_statement.mylite != NULL || parent_statement.sqlite != NULL) &&
        finalize_statement(&parent_statement) != 0) {
        result = 1;
    }
    if ((child_statement.mylite != NULL || child_statement.sqlite != NULL) &&
        finalize_statement(&child_statement) != 0) {
        result = 1;
    }
    return result;
}

static int seed_set_null_tables(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset
) {
    struct benchmark_statement parent_statement = {0};
    struct benchmark_statement child_statement = {0};
    size_t parent_count = fanout_parent_count_for_rows(dataset->row_count);
    int result = 1;

    if (parent_count == 0U) {
        return 1;
    }
    if (prepare_statement(
            database,
            "INSERT INTO set_null_parents (id) VALUES (?)",
            &parent_statement
        ) != 0 ||
        prepare_statement(
            database,
            "INSERT INTO set_null_children (id, parent_id, payload) VALUES (?, ?, ?)",
            &child_statement
        ) != 0) {
        goto cleanup;
    }
    for (size_t id = 1U; id <= parent_count; ++id) {
        if (reset_statement(&parent_statement) != 0 ||
            bind_int64(&parent_statement, 0U, (int64_t)id) != 0 ||
            step_write_statement(&parent_statement) != 0) {
            goto cleanup;
        }
    }
    for (size_t id = 1U; id <= dataset->row_count; ++id) {
        size_t parent_id = ((id - 1U) % parent_count) + 1U;

        if (reset_statement(&child_statement) != 0 ||
            bind_int64(&child_statement, 0U, (int64_t)id) != 0 ||
            bind_int64(&child_statement, 1U, (int64_t)parent_id) != 0 ||
            bind_int64(&child_statement, 2U, (int64_t)(id % score_modulus)) != 0 ||
            step_write_statement(&child_statement) != 0) {
            goto cleanup;
        }
    }
    result = 0;

cleanup:
    if ((parent_statement.mylite != NULL || parent_statement.sqlite != NULL) &&
        finalize_statement(&parent_statement) != 0) {
        result = 1;
    }
    if ((child_statement.mylite != NULL || child_statement.sqlite != NULL) &&
        finalize_statement(&child_statement) != 0) {
        result = 1;
    }
    return result;
}

static int insert_account(struct benchmark_statement *statement, size_t account_id, size_t region) {
    char name[generated_text_capacity];
    int written = snprintf(name, sizeof(name), "account-%010zu", account_id);

    if (written < 0 || (size_t)written >= sizeof(name) || reset_statement(statement) != 0 ||
        bind_int64(statement, 0U, (int64_t)account_id) != 0 ||
        bind_int64(statement, 1U, (int64_t)region) != 0 || bind_text(statement, 2U, name) != 0) {
        return 1;
    }
    return step_write_statement(statement);
}

static int insert_item(
    struct benchmark_statement *statement,
    size_t item_id,
    const struct benchmark_dataset *dataset
) {
    static const char *const statuses[] = {
        "published",
        "published",
        "published",
        "published",
        "published",
        "published",
        "published",
        "draft",
        "draft",
        "archived",
    };
    char title[generated_text_capacity];
    char payload[generated_text_capacity];
    size_t account_id = ((item_id - 1U) % dataset->account_count) + 1U;
    size_t category_id = item_id % category_count;
    size_t tenant_id =
        item_id <= dataset->row_count / 2U ? 0U : ((item_id % skew_tenant_count) + 1U);
    int64_t score = (int64_t)((item_id * score_multiplier) % score_modulus);
    int title_written = snprintf(title, sizeof(title), "item-%010zu", item_id);
    int payload_written = snprintf(
        payload,
        sizeof(payload),
        "payload-%010zu-abcdefghijklmnopqrstuvwxyz-0123456789",
        item_id
    );

    if (title_written < 0 || (size_t)title_written >= sizeof(title) || payload_written < 0 ||
        (size_t)payload_written >= sizeof(payload) || reset_statement(statement) != 0 ||
        bind_int64(statement, 0U, (int64_t)item_id) != 0 ||
        bind_int64(statement, 1U, (int64_t)account_id) != 0 ||
        bind_int64(statement, 2U, (int64_t)category_id) != 0 ||
        bind_text(statement, 3U, statuses[item_id % (sizeof(statuses) / sizeof(statuses[0]))]) !=
            0 ||
        bind_int64(statement, 4U, score) != 0 ||
        bind_int64(
            statement,
            item_created_at_parameter,
            (int64_t)(seed_timestamp_base + (item_id % seconds_per_year))
        ) != 0 ||
        bind_text(statement, item_title_parameter, title) != 0 ||
        bind_text(statement, item_payload_parameter, payload) != 0 ||
        bind_int64(statement, item_tenant_parameter, (int64_t)tenant_id) != 0 ||
        (item_id % null_value_interval == 0U
             ? bind_null(statement, item_optional_parameter)
             : bind_int64(statement, item_optional_parameter, score)) != 0) {
        return 1;
    }
    return step_write_statement(statement);
}

static int insert_tag(
    struct benchmark_statement *statement,
    size_t item_id,
    const struct benchmark_dataset *dataset
) {
    if (reset_statement(statement) != 0 || bind_int64(statement, 0U, (int64_t)item_id) != 0 ||
        bind_int64(statement, 1U, (int64_t)(item_id % dataset->tag_count)) != 0 ||
        bind_int64(statement, 2U, (int64_t)(item_id % tag_weight_modulus)) != 0) {
        return 1;
    }
    return step_write_statement(statement);
}

static int step_write_statement(struct benchmark_statement *statement) {
    if (engine_uses_mylite_api(statement->kind)) {
        return mylite_stmt_step(statement->mylite) == MYLITE_DONE ? 0 : 1;
    }
#ifdef MYLITE_ENABLE_PROFILING
    if (engine_uses_direct_sqlite(statement->kind) || statement->manual_profile != NULL) {
        return step_profiled_sqlite_statement(statement) == SQLITE_DONE ? 0 : 1;
    }
#endif
    return sqlite3_step(statement->sqlite) == SQLITE_DONE ? 0 : 1;
}

#ifdef MYLITE_ENABLE_PROFILING
static int step_profiled_sqlite_statement(struct benchmark_statement *statement) {
    uint64_t started_ns = monotonic_now_ns();
    int rc = SQLITE_OK;

    if (engine_uses_direct_sqlite(statement->kind)) {
        return mylite_profile_sqlite3_step(statement->sqlite);
    }
    rc = sqlite3_step(statement->sqlite);
    record_manual_statement_status(
        statement->manual_profile,
        statement->sqlite,
        monotonic_now_ns() - started_ns
    );
    return rc;
}

static void record_manual_statement_status(
    struct mylite_profile_snapshot *profile,
    sqlite3_stmt *statement,
    uint64_t elapsed_ns
) {
    struct statement_status_counter {
        uint64_t *counter;
        int operation;
    };
    const struct statement_status_counter counters[] = {
        {&profile->sqlite_vm_step_count, SQLITE_STMTSTATUS_VM_STEP},
        {&profile->sqlite_fullscan_step_count, SQLITE_STMTSTATUS_FULLSCAN_STEP},
        {&profile->sqlite_sort_count, SQLITE_STMTSTATUS_SORT},
        {&profile->sqlite_autoindex_count, SQLITE_STMTSTATUS_AUTOINDEX},
        {&profile->sqlite_reprepare_count, SQLITE_STMTSTATUS_REPREPARE},
        {&profile->sqlite_run_count, SQLITE_STMTSTATUS_RUN},
        {&profile->sqlite_filter_hit_count, SQLITE_STMTSTATUS_FILTER_HIT},
        {&profile->sqlite_filter_miss_count, SQLITE_STMTSTATUS_FILTER_MISS},
    };

    add_manual_profile_counter(&profile->sqlite_step_ns, elapsed_ns);
    add_manual_profile_counter(&profile->sqlite_step_count, 1U);
    for (size_t index = 0U; index < sizeof(counters) / sizeof(counters[0]); ++index) {
        int value = sqlite3_stmt_status(statement, counters[index].operation, 1);

        if (value > 0) {
            add_manual_profile_counter(counters[index].counter, (uint64_t)value);
        }
    }
}

static void add_manual_profile_counter(uint64_t *counter, uint64_t value) {
    if (value > UINT64_MAX - *counter) {
        *counter = UINT64_MAX;
        return;
    }
    *counter += value;
}
#endif

static void close_benchmark_database(struct benchmark_database *database) {
    if (database->mylite != NULL) {
        mylite_close(database->mylite);
        database->mylite = NULL;
        database->sqlite = NULL;
    } else if (database->sqlite != NULL) {
        (void)sqlite3_close(database->sqlite);
        database->sqlite = NULL;
    }
}

static void remove_database_files(const char *path) {
    static const char *const suffixes[] = {"", "-journal", "-wal", "-shm"};
    char related[path_capacity];

    if (path == NULL || path[0] == '\0') {
        return;
    }
    for (size_t index = 0U; index < sizeof(suffixes) / sizeof(suffixes[0]); ++index) {
        int written = snprintf(related, sizeof(related), "%s%s", path, suffixes[index]);

        if (written >= 0 && (size_t)written < sizeof(related)) {
            (void)remove(related);
        }
    }
}

static uint64_t file_size(const char *path) {
#if defined(_WIN32)
    struct _stat64 status = {0};

    return _stat64(path, &status) == 0 ? (uint64_t)status.st_size : 0U;
#else
    struct stat status = {0};

    return stat(path, &status) == 0 ? (uint64_t)status.st_size : 0U;
#endif
}

static uint64_t monotonic_now_ns(void) {
#if defined(_WIN32)
    return (uint64_t)clock() * nanoseconds_per_second / (uint64_t)CLOCKS_PER_SEC;
#else
    struct timespec timestamp = {0};

    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0) {
        return 0U;
    }
    return ((uint64_t)timestamp.tv_sec * nanoseconds_per_second) + (uint64_t)timestamp.tv_nsec;
#endif
}

static uint64_t process_cpu_now_ns(void) {
    clock_t ticks = clock();
    uint64_t ticks_per_second = (uint64_t)CLOCKS_PER_SEC;
    uint64_t whole_seconds = 0U;
    uint64_t remaining_ticks = 0U;

    if (ticks == (clock_t)-1 || ticks_per_second == 0U) {
        return 0U;
    }
    whole_seconds = (uint64_t)ticks / ticks_per_second;
    remaining_ticks = (uint64_t)ticks % ticks_per_second;
    return (whole_seconds * nanoseconds_per_second) +
           ((remaining_ticks * nanoseconds_per_second) / ticks_per_second);
}

static uint64_t hash_bytes(uint64_t hash, const void *bytes, size_t size) {
    static const uint64_t fnv_prime = 1099511628211ULL;
    const unsigned char *data = bytes;

    if (data == NULL) {
        return hash;
    }
    for (size_t index = 0U; index < size; ++index) {
        hash ^= data[index];
        hash *= fnv_prime;
    }
    return hash;
}

static void hash_uint64(uint64_t *hash, uint64_t value) {
    unsigned char bytes[sizeof(value)];

    for (size_t index = 0U; index < sizeof(value); ++index) {
        bytes[index] = (unsigned char)((value >> (index * bits_per_byte)) & byte_mask);
    }
    *hash = hash_bytes(*hash, bytes, sizeof(bytes));
}

static double median_elapsed_us(
    enum benchmark_engine_kind engine,
    const struct benchmark_sample_pair *samples,
    size_t sample_count
) {
    double *values = malloc(sample_count * sizeof(*values));
    double median = 0.0;

    if (values == NULL) {
        return 0.0;
    }
    for (size_t index = 0U; index < sample_count; ++index) {
        uint64_t elapsed = engine == benchmark_engine_mylite ? samples[index].mylite.elapsed_ns
                                                             : samples[index].sqlite.elapsed_ns;
        size_t operations = engine == benchmark_engine_mylite
                                ? samples[index].mylite.operation_count
                                : samples[index].sqlite.operation_count;

        values[index] = (double)elapsed / (double)nanoseconds_per_microsecond / (double)operations;
    }
    qsort(values, sample_count, sizeof(*values), compare_double);
    if (sample_count % 2U == 0U) {
        median = (values[(sample_count / 2U) - 1U] + values[sample_count / 2U]) /
                 (double)median_pair_count;
    } else {
        median = values[sample_count / 2U];
    }
    free(values);
    return median;
}

// qsort fixes the callback signature.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static int compare_double(const void *left, const void *right) {
    double left_value = *(const double *)left;
    double right_value = *(const double *)right;

    if (left_value < right_value) {
        return -1;
    }
    if (left_value > right_value) {
        return 1;
    }
    return 0;
}

static const char *engine_name(enum benchmark_engine_kind kind) {
    switch (kind) {
    case benchmark_engine_mylite:
        return "mylite";
    case benchmark_engine_sqlite:
        return "sqlite";
    case benchmark_engine_mylite_physical:
        return "mylite_physical";
    case benchmark_engine_mylite_guarded:
        return "mylite_guarded";
    }
    return "unknown";
}

static bool engine_uses_mylite_api(enum benchmark_engine_kind kind) {
    return kind == benchmark_engine_mylite;
}

static bool engine_uses_mylite_storage(enum benchmark_engine_kind kind) {
    return kind == benchmark_engine_mylite || kind == benchmark_engine_mylite_physical ||
           kind == benchmark_engine_mylite_guarded;
}

static bool engine_uses_direct_sqlite(enum benchmark_engine_kind kind) {
    return kind == benchmark_engine_mylite_physical || kind == benchmark_engine_mylite_guarded;
}

static const char *mode_name(enum benchmark_execution_mode mode) {
    switch (mode) {
    case benchmark_execution_prepare_each:
        return "prepare_each";
    case benchmark_execution_prepared:
        return "prepared";
    case benchmark_execution_write_rollback:
        return "write_rollback";
    case benchmark_execution_expected_error_rollback:
        return "expected_error_rollback";
    case benchmark_execution_bulk_import_rollback:
        return "bulk_import_rollback";
    }
    return "unknown";
}

static size_t account_count_for_rows(size_t row_count) {
    size_t count = row_count / rows_per_account;

    return count < minimum_account_count ? minimum_account_count : count;
}

static size_t tag_count_for_rows(size_t row_count) {
    size_t count = row_count / rows_per_account;

    if (count < minimum_tag_count) {
        return minimum_tag_count;
    }
    return count > maximum_tag_count ? maximum_tag_count : count;
}

static size_t fanout_parent_count_for_rows(size_t row_count) {
    return row_count < fanout_parent_count ? row_count : fanout_parent_count;
}

static size_t range_count_for_basis_points(size_t row_count, size_t basis_points) {
    size_t count = (row_count * basis_points) / selectivity_basis_points;

    return count == 0U ? 1U : count;
}

static bool scenario_includes_affected_rows(const struct benchmark_scenario *scenario) {
    if (!scenario_uses_rollback(scenario) ||
        scenario->mode == benchmark_execution_expected_error_rollback) {
        return false;
    }

    /*
     * MySQL reports two affected rows for a changed duplicate-key update while
     * SQLite reports one. Both paths are verified by successful execution and
     * transaction rollback, so affected-row comparison is intentionally omitted
     * only for the two cross-dialect UPSERT scenarios.
     */
    return scenario->id != benchmark_scenario_upsert_hit &&
           scenario->id != benchmark_scenario_upsert_miss;
}

static bool scenario_uses_rollback(const struct benchmark_scenario *scenario) {
    return scenario->mode == benchmark_execution_write_rollback ||
           scenario->mode == benchmark_execution_expected_error_rollback ||
           scenario->mode == benchmark_execution_bulk_import_rollback;
}

static size_t scenario_iterations(
    const struct benchmark_scenario *scenario,
    const struct benchmark_options *options
) {
    return options->iteration_override == 0U ? scenario->default_iterations
                                             : options->iteration_override;
}

static int parse_size(const char *text, bool allow_zero, size_t *out_value) {
    char *end = NULL;
    unsigned long long value = 0ULL;

    errno = 0;
    value = strtoull(text, &end, decimal_base);
    if (errno != 0 || end == text || *end != '\0' || (!allow_zero && value == 0ULL) ||
        value > SIZE_MAX) {
        return 1;
    }
    *out_value = (size_t)value;
    return 0;
}

static const char *option_value(int argc, char **argv, int *index) {
    if (*index + 1 >= argc) {
        return NULL;
    }
    ++*index;
    return argv[*index];
}

static const char *default_database_directory(void) {
#if defined(_WIN32)
    const char *directory = getenv("TEMP");
#else
    const char *directory = getenv("TMPDIR");
#endif

    return directory == NULL || directory[0] == '\0' ? "." : directory;
}

static long benchmark_process_id(void) {
#if defined(_WIN32)
    return (long)_getpid();
#else
    return (long)getpid();
#endif
}
