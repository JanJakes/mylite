#include <mylite/mylite.h>

#include "mylite_benchmark_csv.h"
#include "mylite_benchmark_parse_expectations.h"
#include "mylite_benchmark_sql_mode.h"
#include "sql/mylite_lexer.h"
#include "sql/mylite_parser.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <time.h>
#  include <unistd.h>
#endif

enum {
    default_iterations = 1000,
    default_csv_iterations = 1,
    runtime_database_path_capacity = 1024,
    runtime_database_suffix_capacity = 16,
    decimal_option_base = 10,
    nanoseconds_per_second = 1000000000ULL,
    milliseconds_per_second = 1000,
    microseconds_per_second = 1000000,
    parse_status_bucket_count = MYLITE_SQL_PARSE_STACK_OVERFLOW + 1,
};

enum benchmark_filter {
    benchmark_filter_all,
    benchmark_filter_lexer,
    benchmark_filter_parse,
    benchmark_filter_runtime,
};

struct benchmark_options {
    enum benchmark_filter filter;
    const char *csv_path;
    const char *parse_failure_dump_path;
    const char *expected_parse_failures_path;
    size_t iterations;
    size_t csv_iterations;
    bool csv_replay_sql_mode;
    bool list_only;
    bool show_usage;
};

struct benchmark_query {
    const char *sql;
    size_t length;
};

struct borrowed_query_list {
    const struct benchmark_query *items;
    size_t count;
};

struct benchmark_measurement {
    uint64_t elapsed_ns;
    size_t operations;
    size_t bytes;
    size_t ok_count;
    size_t error_count;
    size_t token_count;
    size_t parse_status_counts[parse_status_bucket_count];
};

struct text_coordinates {
    unsigned int line;
    unsigned int column;
};

struct expected_parse_failure_summary {
    size_t total_count;
    size_t matched_count;
    size_t unexpected_count;
    size_t mismatched_count;
    size_t missing_count;
};

struct runtime_scenario {
    const char *name;
    const struct benchmark_query *setup_queries;
    size_t setup_query_count;
    const struct benchmark_query *queries;
    size_t query_count;
};

#define QUERY(sql_literal) {sql_literal, sizeof(sql_literal) - 1U}

static const struct benchmark_query wordpress_setup_queries[] = {
    QUERY("CREATE DATABASE wp"),
    QUERY("USE wp"),
    QUERY("SET sql_mode = ''"),
    QUERY("CREATE TABLE wp_options ("
          "option_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
          "option_name VARCHAR(191) NOT NULL DEFAULT '',"
          "option_value LONGTEXT NOT NULL,"
          "autoload VARCHAR(20) NOT NULL DEFAULT 'yes',"
          "PRIMARY KEY (option_id),"
          "UNIQUE KEY option_name (option_name),"
          "KEY autoload (autoload)"
          ")"),
    QUERY("CREATE TABLE wp_posts ("
          "ID BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
          "post_author BIGINT UNSIGNED NOT NULL DEFAULT 0,"
          "post_date DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00',"
          "post_date_gmt DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00',"
          "post_content LONGTEXT NOT NULL,"
          "post_title TEXT NOT NULL,"
          "post_excerpt TEXT NOT NULL,"
          "post_status VARCHAR(20) NOT NULL DEFAULT 'publish',"
          "comment_status VARCHAR(20) NOT NULL DEFAULT 'open',"
          "ping_status VARCHAR(20) NOT NULL DEFAULT 'open',"
          "post_password VARCHAR(255) NOT NULL DEFAULT '',"
          "post_name VARCHAR(200) NOT NULL DEFAULT '',"
          "to_ping TEXT NOT NULL,"
          "pinged TEXT NOT NULL,"
          "post_modified DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00',"
          "post_modified_gmt DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00',"
          "post_content_filtered LONGTEXT NOT NULL,"
          "post_parent BIGINT UNSIGNED NOT NULL DEFAULT 0,"
          "guid VARCHAR(255) NOT NULL DEFAULT '',"
          "menu_order INT NOT NULL DEFAULT 0,"
          "post_type VARCHAR(20) NOT NULL DEFAULT 'post',"
          "post_mime_type VARCHAR(100) NOT NULL DEFAULT '',"
          "comment_count BIGINT NOT NULL DEFAULT 0,"
          "PRIMARY KEY (ID),"
          "KEY post_name (post_name(191)),"
          "KEY type_status_date (post_type, post_status, post_date, ID),"
          "KEY post_parent (post_parent),"
          "KEY post_author (post_author)"
          ")"),
    QUERY("CREATE TABLE wp_postmeta ("
          "meta_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
          "post_id BIGINT UNSIGNED NOT NULL DEFAULT 0,"
          "meta_key VARCHAR(255) DEFAULT NULL,"
          "meta_value LONGTEXT,"
          "PRIMARY KEY (meta_id),"
          "KEY post_id (post_id),"
          "KEY meta_key (meta_key(191))"
          ")"),
    QUERY("INSERT INTO wp_options (option_name, option_value, autoload) VALUES "
          "('siteurl','https://example.test','yes'),"
          "('home','https://example.test','yes'),"
          "('blogname','MyLite Bench','yes'),"
          "('stylesheet','twentytwentyfive','yes'),"
          "('template','twentytwentyfive','yes'),"
          "('cron','{}','no')"),
    QUERY("INSERT INTO wp_posts "
          "(post_author, post_date, post_date_gmt, post_content, post_title, post_excerpt, "
          "post_status, comment_status, ping_status, post_password, post_name, to_ping, pinged, "
          "post_modified, post_modified_gmt, post_content_filtered, post_parent, guid, menu_order, "
          "post_type, post_mime_type, comment_count) VALUES "
          "(1,'2026-01-01 10:00:00','2026-01-01 10:00:00','content','Hello','',"
          "'publish','open','open','','hello','','','2026-01-01 10:00:00',"
          "'2026-01-01 10:00:00','',0,'https://example.test/?p=1',0,'post','',0),"
          "(1,'2026-01-02 10:00:00','2026-01-02 10:00:00','content','Draft','',"
          "'draft','open','open','','draft','','','2026-01-02 10:00:00',"
          "'2026-01-02 10:00:00','',0,'https://example.test/?p=2',0,'post','',0),"
          "(1,'2026-01-03 10:00:00','2026-01-03 10:00:00','content','Page','',"
          "'publish','closed','closed','','page','','','2026-01-03 10:00:00',"
          "'2026-01-03 10:00:00','',0,'https://example.test/?page_id=3',0,'page','',0)"),
    QUERY("INSERT INTO wp_postmeta (post_id, meta_key, meta_value) VALUES "
          "(1,'_thumbnail_id','10'),"
          "(1,'_edit_lock','1700000000:1'),"
          "(2,'_edit_lock','1700000001:1'),"
          "(3,'_wp_page_template','default')"),
};

static const struct benchmark_query wordpress_options_queries[] = {
    QUERY("SELECT option_value FROM wp_options WHERE option_name = 'siteurl' LIMIT 1"),
    QUERY("SELECT option_name, option_value FROM wp_options WHERE autoload = 'yes'"),
    QUERY("SELECT option_id FROM wp_options WHERE option_name = 'home'"),
    QUERY(
        "UPDATE wp_options SET option_value = 'MyLite Bench Updated' WHERE option_name = 'blogname'"
    ),
    QUERY("INSERT INTO wp_options (option_name, option_value, autoload) "
          "VALUES ('_transient_bench','1','no') "
          "ON DUPLICATE KEY UPDATE option_value = '1'"),
    QUERY("DELETE FROM wp_options WHERE option_name = '_transient_delete_miss'"),
};

static const struct benchmark_query wordpress_posts_meta_queries[] = {
    QUERY("SELECT ID, post_title FROM wp_posts "
          "WHERE post_type = 'post' AND post_status = 'publish' "
          "ORDER BY post_date DESC LIMIT 10"),
    QUERY("SELECT p.ID, pm.meta_value FROM wp_posts p "
          "LEFT JOIN wp_postmeta pm ON pm.post_id = p.ID AND pm.meta_key = '_thumbnail_id' "
          "WHERE p.ID = 1"),
    QUERY("SELECT meta_value FROM wp_postmeta WHERE post_id = 1 AND meta_key = '_edit_lock'"),
    QUERY("UPDATE wp_postmeta SET meta_value = '1700000002:1' WHERE post_id = 1 AND meta_key = "
          "'_edit_lock'"),
    QUERY("SELECT COUNT(*) FROM wp_posts WHERE post_type = 'post'"),
};

static const struct benchmark_query wordpress_metadata_queries[] = {
    QUERY("SHOW TABLE STATUS LIKE 'wp_options'"),
    QUERY("SHOW INDEX FROM wp_options"),
    QUERY("DESCRIBE wp_posts"),
    QUERY("SELECT COLUMN_NAME, DATA_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
          "WHERE TABLE_SCHEMA = 'wp' AND TABLE_NAME = 'wp_options'"),
    QUERY("SHOW CREATE TABLE wp_postmeta"),
};

static const struct benchmark_query parser_wordpress_queries[] = {
    QUERY("SELECT option_value FROM wp_options WHERE option_name = 'siteurl' LIMIT 1"),
    QUERY("SELECT option_name, option_value FROM wp_options WHERE autoload = 'yes'"),
    QUERY(
        "UPDATE wp_options SET option_value = 'MyLite Bench Updated' WHERE option_name = 'blogname'"
    ),
    QUERY("INSERT INTO wp_options (option_name, option_value, autoload) "
          "VALUES ('_transient_bench','1','no') "
          "ON DUPLICATE KEY UPDATE option_value = '1'"),
    QUERY("SELECT ID, post_title FROM wp_posts "
          "WHERE post_type = 'post' AND post_status = 'publish' "
          "ORDER BY post_date DESC LIMIT 10"),
    QUERY("SELECT p.ID, pm.meta_value FROM wp_posts p "
          "LEFT JOIN wp_postmeta pm ON pm.post_id = p.ID AND pm.meta_key = '_thumbnail_id' "
          "WHERE p.ID = 1"),
    QUERY("SHOW TABLE STATUS LIKE 'wp_options'"),
    QUERY("SHOW INDEX FROM wp_options"),
    QUERY("DESCRIBE wp_posts"),
    QUERY("SELECT COLUMN_NAME, DATA_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
          "WHERE TABLE_SCHEMA = 'wp' AND TABLE_NAME = 'wp_options'"),
    QUERY("SHOW CREATE TABLE wp_postmeta"),
};

static const struct runtime_scenario runtime_scenarios[] = {
    {
        .name = "runtime.wp_options_hot",
        .setup_queries = wordpress_setup_queries,
        .setup_query_count = sizeof(wordpress_setup_queries) / sizeof(wordpress_setup_queries[0]),
        .queries = wordpress_options_queries,
        .query_count = sizeof(wordpress_options_queries) / sizeof(wordpress_options_queries[0]),
    },
    {
        .name = "runtime.wp_posts_meta_hot",
        .setup_queries = wordpress_setup_queries,
        .setup_query_count = sizeof(wordpress_setup_queries) / sizeof(wordpress_setup_queries[0]),
        .queries = wordpress_posts_meta_queries,
        .query_count =
            sizeof(wordpress_posts_meta_queries) / sizeof(wordpress_posts_meta_queries[0]),
    },
    {
        .name = "runtime.wp_metadata_hot",
        .setup_queries = wordpress_setup_queries,
        .setup_query_count = sizeof(wordpress_setup_queries) / sizeof(wordpress_setup_queries[0]),
        .queries = wordpress_metadata_queries,
        .query_count = sizeof(wordpress_metadata_queries) / sizeof(wordpress_metadata_queries[0]),
    },
};

static int parse_options(int argc, char **argv, struct benchmark_options *out_options);
static int parse_option(
    int argc,
    char **argv,
    int *index,
    const char *program_name,
    struct benchmark_options *out_options
);
static const char *consume_option_value(int argc, char **argv, int *index, const char *option_name);
static int parse_filter_option(const char *text, enum benchmark_filter *out_filter);
static int parse_size_option(const char *text, size_t *out_value);
static void print_usage(const char *program_name, FILE *stream);
static void print_scenario_list(void);
static bool filter_includes(enum benchmark_filter filter, enum benchmark_filter candidate);
static int run_benchmarks(const struct benchmark_options *options);
static int run_builtin_lexer_benchmark(const struct benchmark_options *options);
static int run_builtin_parse_benchmark(const struct benchmark_options *options);
static int run_csv_benchmarks(const struct benchmark_options *options);
static int run_runtime_benchmarks(const struct benchmark_options *options);
static int run_runtime_scenario(
    const struct runtime_scenario *scenario,
    size_t iterations,
    struct benchmark_measurement *out_measurement
);
static int setup_runtime_database(mylite_db *database, const struct runtime_scenario *scenario);
static int execute_query(mylite_db *database, const struct benchmark_query *query);
static int printf_precision_from_size(size_t length);
static int make_runtime_database_path(char *path, size_t path_size, const char *scenario_name);
static int current_process_id(void);
static const char *temporary_directory(void);
static void remove_related_database_files(const char *path);
static void remove_database_file_with_suffix(const char *path, const char *suffix);
static int benchmark_lexer_queries(
    struct borrowed_query_list queries,
    size_t iterations,
    struct benchmark_measurement *out_measurement
);
static int benchmark_owned_lexer_queries(
    const struct mylite_benchmark_owned_query_list *queries,
    size_t iterations,
    struct benchmark_measurement *out_measurement
);
static int lex_query(
    const char *sql,
    size_t length,
    unsigned int modes,
    size_t *out_token_count,
    bool *out_has_error
);
static int benchmark_parse_queries(
    struct borrowed_query_list queries,
    size_t iterations,
    struct benchmark_measurement *out_measurement
);
static int benchmark_owned_parse_queries(
    const struct mylite_benchmark_owned_query_list *queries,
    size_t iterations,
    struct benchmark_measurement *out_measurement
);
static int dump_parse_failures(
    const struct mylite_benchmark_owned_query_list *queries,
    const char *path
);
static int report_expected_parse_failures(
    const struct mylite_benchmark_owned_query_list *queries,
    const char *path
);
static int classify_expected_parse_failures(
    const struct mylite_benchmark_owned_query_list *queries,
    const struct mylite_benchmark_expected_parse_failure_list *expectations,
    struct expected_parse_failure_summary *out_summary
);
static void print_parse_expectation_mismatch(
    const struct mylite_benchmark_expected_parse_failure *expectation,
    size_t query_index,
    enum mylite_sql_parse_status status,
    enum mylite_sql_token_kind token_kind
);
static void print_parse_failure_row(
    FILE *file,
    size_t query_index,
    enum mylite_sql_parse_status status,
    const struct mylite_sql_parse_result *result,
    const struct mylite_benchmark_owned_query *query
);
static struct text_coordinates compute_offset_coordinates(
    const char *text,
    size_t length,
    size_t offset
);
static void print_tsv_escaped_field(FILE *file, const char *text, size_t length);
static void record_parse_status(
    struct benchmark_measurement *measurement,
    enum mylite_sql_parse_status status
);
static uint64_t monotonic_now_ns(void);
static void print_result(
    const char *scenario,
    const char *kind,
    size_t iterations,
    size_t query_count,
    const struct benchmark_measurement *measurement
);
static void print_parse_status_counts(const struct benchmark_measurement *measurement);
static double ns_to_ms(uint64_t ns);
static double ns_to_average_us(uint64_t ns, size_t operations);
static double ns_to_ops_per_second(uint64_t ns, size_t operations);

int main(int argc, char **argv) {
    struct benchmark_options options = {
        .filter = benchmark_filter_all,
        .csv_path = NULL,
        .parse_failure_dump_path = NULL,
        .expected_parse_failures_path = NULL,
        .iterations = default_iterations,
        .csv_iterations = default_csv_iterations,
        .csv_replay_sql_mode = false,
        .list_only = false,
        .show_usage = false,
    };
    int rc = parse_options(argc, argv, &options);

    if (rc != 0) {
        return rc;
    }
    if (options.show_usage) {
        print_usage(argv[0], stdout);
        return 0;
    }
    if (options.list_only) {
        print_scenario_list();
        return 0;
    }

    return run_benchmarks(&options);
}

static int parse_options(int argc, char **argv, struct benchmark_options *out_options) {
    for (int index = 1; index < argc; ++index) {
        int rc = parse_option(argc, argv, &index, argv[0], out_options);

        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}

static int parse_option(
    int argc,
    char **argv,
    int *index,
    const char *program_name,
    struct benchmark_options *out_options
) {
    const char *argument = argv[*index];

    if (strcmp(argument, "--help") == 0) {
        out_options->show_usage = true;
        return 0;
    }
    if (strcmp(argument, "--list") == 0) {
        out_options->list_only = true;
        return 0;
    }
    if (strcmp(argument, "--iterations") == 0) {
        const char *value = consume_option_value(argc, argv, index, "--iterations");

        if (value == NULL || parse_size_option(value, &out_options->iterations) != 0) {
            return 1;
        }
        return 0;
    }
    if (strcmp(argument, "--csv-iterations") == 0) {
        const char *value = consume_option_value(argc, argv, index, "--csv-iterations");

        if (value == NULL || parse_size_option(value, &out_options->csv_iterations) != 0) {
            return 1;
        }
        return 0;
    }
    if (strcmp(argument, "--csv") == 0) {
        const char *value = consume_option_value(argc, argv, index, "--csv");

        if (value == NULL) {
            return 1;
        }
        out_options->csv_path = value;
        return 0;
    }
    if (strcmp(argument, "--csv-replay-sql-mode") == 0) {
        out_options->csv_replay_sql_mode = true;
        return 0;
    }
    if (strcmp(argument, "--dump-parse-failures") == 0) {
        const char *value = consume_option_value(argc, argv, index, "--dump-parse-failures");

        if (value == NULL) {
            return 1;
        }
        out_options->parse_failure_dump_path = value;
        return 0;
    }
    if (strcmp(argument, "--expected-parse-failures") == 0) {
        const char *value = consume_option_value(argc, argv, index, "--expected-parse-failures");

        if (value == NULL) {
            return 1;
        }
        out_options->expected_parse_failures_path = value;
        return 0;
    }
    if (strcmp(argument, "--only") == 0) {
        const char *value = consume_option_value(argc, argv, index, "--only");

        if (value == NULL || parse_filter_option(value, &out_options->filter) != 0) {
            return 1;
        }
        return 0;
    }

    fprintf(stderr, "unknown argument: %s\n", argument);
    print_usage(program_name, stderr);
    return 1;
}

static const char *consume_option_value(
    int argc,
    char **argv,
    int *index,
    const char *option_name
) {
    if (*index + 1 >= argc) {
        fprintf(stderr, "%s requires a value\n", option_name);
        return NULL;
    }
    ++*index;
    return argv[*index];
}

static int parse_filter_option(const char *text, enum benchmark_filter *out_filter) {
    if (strcmp(text, "all") == 0) {
        *out_filter = benchmark_filter_all;
        return 0;
    }
    if (strcmp(text, "lexer") == 0) {
        *out_filter = benchmark_filter_lexer;
        return 0;
    }
    if (strcmp(text, "parse") == 0) {
        *out_filter = benchmark_filter_parse;
        return 0;
    }
    if (strcmp(text, "runtime") == 0) {
        *out_filter = benchmark_filter_runtime;
        return 0;
    }

    fprintf(stderr, "--only requires one of all, lexer, parse, runtime\n");
    return 1;
}

static int parse_size_option(const char *text, size_t *out_value) {
    char *end = NULL;
    unsigned long long value = 0ULL;

    if (text == NULL || text[0] == '\0' || out_value == NULL) {
        return 1;
    }

    errno = 0;
    value = strtoull(text, &end, decimal_option_base);
    if (errno != 0 || end == text || *end != '\0' || value == 0ULL) {
        return 1;
    }
    if (value > (unsigned long long)SIZE_MAX) {
        return 1;
    }
    *out_value = (size_t)value;
    return 0;
}

static void print_usage(const char *program_name, FILE *stream) {
    fprintf(
        stream,
        "usage: %s [--iterations N] [--csv PATH] [--csv-iterations N] "
        "[--csv-replay-sql-mode] [--dump-parse-failures PATH] "
        "[--expected-parse-failures PATH] "
        "[--only all|lexer|parse|runtime] [--list]\n",
        program_name
    );
}

static void print_scenario_list(void) {
    puts("lexer.wp_builtin");
    puts("parse.wp_builtin");
    puts("runtime.wp_options_hot");
    puts("runtime.wp_posts_meta_hot");
    puts("runtime.wp_metadata_hot");
    puts("lexer.csv.mysql_server_tests");
    puts("parse.csv.mysql_server_tests");
}

static bool filter_includes(enum benchmark_filter filter, enum benchmark_filter candidate) {
    return filter == benchmark_filter_all || filter == candidate;
}

static int run_benchmarks(const struct benchmark_options *options) {
    int rc = 0;

    if (options->parse_failure_dump_path != NULL &&
        (options->csv_path == NULL || !filter_includes(options->filter, benchmark_filter_parse))) {
        fprintf(stderr, "--dump-parse-failures requires --csv and a parse benchmark filter\n");
        return 1;
    }
    if (options->expected_parse_failures_path != NULL &&
        (options->csv_path == NULL || !filter_includes(options->filter, benchmark_filter_parse))) {
        fprintf(stderr, "--expected-parse-failures requires --csv and a parse benchmark filter\n");
        return 1;
    }

    puts("scenario,kind,iterations,queries,operations,ok,errors,tokens,bytes,total_ms,avg_us,"
         "ops_per_sec");

    if (filter_includes(options->filter, benchmark_filter_lexer)) {
        rc = run_builtin_lexer_benchmark(options);
        if (rc != 0) {
            return rc;
        }
    }
    if (filter_includes(options->filter, benchmark_filter_parse)) {
        rc = run_builtin_parse_benchmark(options);
        if (rc != 0) {
            return rc;
        }
    }
    if (filter_includes(options->filter, benchmark_filter_runtime)) {
        rc = run_runtime_benchmarks(options);
        if (rc != 0) {
            return rc;
        }
    }
    if (options->csv_path != NULL && (filter_includes(options->filter, benchmark_filter_lexer) ||
                                      filter_includes(options->filter, benchmark_filter_parse))) {
        rc = run_csv_benchmarks(options);
    }

    return rc;
}

static int run_builtin_lexer_benchmark(const struct benchmark_options *options) {
    struct benchmark_measurement measurement = {0};
    const struct borrowed_query_list queries = {
        .items = parser_wordpress_queries,
        .count = sizeof(parser_wordpress_queries) / sizeof(parser_wordpress_queries[0]),
    };
    int rc = benchmark_lexer_queries(queries, options->iterations, &measurement);

    if (rc != 0) {
        return rc;
    }
    print_result("lexer.wp_builtin", "lexer", options->iterations, queries.count, &measurement);
    return 0;
}

static int run_builtin_parse_benchmark(const struct benchmark_options *options) {
    struct benchmark_measurement measurement = {0};
    const struct borrowed_query_list queries = {
        .items = parser_wordpress_queries,
        .count = sizeof(parser_wordpress_queries) / sizeof(parser_wordpress_queries[0]),
    };
    int rc = benchmark_parse_queries(queries, options->iterations, &measurement);

    if (rc != 0) {
        return rc;
    }
    print_result("parse.wp_builtin", "parse", options->iterations, queries.count, &measurement);
    print_parse_status_counts(&measurement);
    return 0;
}

static int run_csv_benchmarks(const struct benchmark_options *options) {
    struct mylite_benchmark_owned_query_list queries = {0};
    int rc = mylite_benchmark_load_csv_queries(options->csv_path, &queries);

    if (rc != 0) {
        return rc;
    }
    if (options->csv_replay_sql_mode) {
        rc = mylite_benchmark_assign_sql_modes(&queries);
        if (rc != 0) {
            mylite_benchmark_owned_query_list_deinit(&queries);
            return rc;
        }
    }
    if (filter_includes(options->filter, benchmark_filter_lexer)) {
        struct benchmark_measurement measurement = {0};

        rc = benchmark_owned_lexer_queries(&queries, options->csv_iterations, &measurement);
        if (rc != 0) {
            mylite_benchmark_owned_query_list_deinit(&queries);
            return rc;
        }
        print_result(
            "lexer.csv.mysql_server_tests",
            "lexer",
            options->csv_iterations,
            queries.count,
            &measurement
        );
    }
    if (filter_includes(options->filter, benchmark_filter_parse)) {
        struct benchmark_measurement measurement = {0};

        rc = benchmark_owned_parse_queries(&queries, options->csv_iterations, &measurement);
        if (rc != 0) {
            mylite_benchmark_owned_query_list_deinit(&queries);
            return rc;
        }
        print_result(
            "parse.csv.mysql_server_tests",
            "parse",
            options->csv_iterations,
            queries.count,
            &measurement
        );
        print_parse_status_counts(&measurement);
        if (options->parse_failure_dump_path != NULL) {
            rc = dump_parse_failures(&queries, options->parse_failure_dump_path);
            if (rc != 0) {
                mylite_benchmark_owned_query_list_deinit(&queries);
                return rc;
            }
        }
        if (options->expected_parse_failures_path != NULL) {
            rc = report_expected_parse_failures(&queries, options->expected_parse_failures_path);
            if (rc != 0) {
                mylite_benchmark_owned_query_list_deinit(&queries);
                return rc;
            }
        }
    }

    mylite_benchmark_owned_query_list_deinit(&queries);
    return 0;
}

static int run_runtime_benchmarks(const struct benchmark_options *options) {
    for (size_t scenario_index = 0U;
         scenario_index < sizeof(runtime_scenarios) / sizeof(runtime_scenarios[0]);
         ++scenario_index) {
        struct benchmark_measurement measurement = {0};
        int rc = run_runtime_scenario(
            &runtime_scenarios[scenario_index],
            options->iterations,
            &measurement
        );

        if (rc != 0) {
            return rc;
        }
        print_result(
            runtime_scenarios[scenario_index].name,
            "execute",
            options->iterations,
            runtime_scenarios[scenario_index].query_count,
            &measurement
        );
    }
    return 0;
}

static int run_runtime_scenario(
    const struct runtime_scenario *scenario,
    size_t iterations,
    struct benchmark_measurement *out_measurement
) {
    mylite_db *database = NULL;
    char path[runtime_database_path_capacity];
    uint64_t started = 0U;
    uint64_t ended = 0U;
    int rc = make_runtime_database_path(path, sizeof(path), scenario->name);

    if (rc != 0) {
        return 1;
    }
    remove_related_database_files(path);
    rc = mylite_open(path, &database);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: failed to open benchmark database: %d\n", scenario->name, rc);
        remove_related_database_files(path);
        return 1;
    }
    if (setup_runtime_database(database, scenario) != 0) {
        mylite_close(database);
        remove_related_database_files(path);
        return 1;
    }

    started = monotonic_now_ns();
    for (size_t iteration = 0U; iteration < iterations; ++iteration) {
        for (size_t query_index = 0U; query_index < scenario->query_count; ++query_index) {
            const struct benchmark_query *query = &scenario->queries[query_index];

            ++out_measurement->operations;
            out_measurement->bytes += query->length;
            if (execute_query(database, query) == 0) {
                ++out_measurement->ok_count;
            } else {
                ++out_measurement->error_count;
                mylite_close(database);
                remove_related_database_files(path);
                return 1;
            }
        }
    }
    ended = monotonic_now_ns();
    out_measurement->elapsed_ns = ended - started;

    mylite_close(database);
    remove_related_database_files(path);
    return 0;
}

static int setup_runtime_database(mylite_db *database, const struct runtime_scenario *scenario) {
    for (size_t query_index = 0U; query_index < scenario->setup_query_count; ++query_index) {
        if (execute_query(database, &scenario->setup_queries[query_index]) != 0) {
            fprintf(stderr, "%s: setup query %zu failed\n", scenario->name, query_index + 1U);
            return 1;
        }
    }
    return 0;
}

static int execute_query(mylite_db *database, const struct benchmark_query *query) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query->sql, query->length, &result);

    if (rc != MYLITE_OK) {
        int display_length = printf_precision_from_size(query->length);

        fprintf(
            stderr,
            "query failed: rc=%d err=%d state=%s message=%s\n",
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        fprintf(stderr, "sql: %.*s\n", display_length, query->sql);
        mylite_result_free(result);
        return 1;
    }
    mylite_result_free(result);
    return 0;
}

static int printf_precision_from_size(size_t length) {
    if (length > (size_t)INT_MAX) {
        return INT_MAX;
    }
    return (int)length;
}

static int make_runtime_database_path(char *path, size_t path_size, const char *scenario_name) {
    const char *directory = temporary_directory();
#if defined(_WIN32)
    const char separator = '\\';
#else
    const char separator = '/';
#endif
    int written = snprintf(
        path,
        path_size,
        "%s%cmylite_benchmark_%d_%s.mylite",
        directory,
        separator,
        current_process_id(),
        scenario_name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "%s: benchmark database path is too long\n", scenario_name);
        return 1;
    }
    return 0;
}

static int current_process_id(void) {
#if defined(_WIN32)
    return (int)GetCurrentProcessId();
#else
    return getpid();
#endif
}

static const char *temporary_directory(void) {
    const char *directory = getenv("TMPDIR");

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
#if defined(_WIN32)
        directory = ".";
#else
        directory = "/tmp";
#endif
    }
    return directory;
}

static void remove_related_database_files(const char *path) {
    remove_database_file_with_suffix(path, "");
    remove_database_file_with_suffix(path, "-journal");
    remove_database_file_with_suffix(path, "-wal");
    remove_database_file_with_suffix(path, "-shm");
}

static void remove_database_file_with_suffix(const char *path, const char *suffix) {
    char related_path[runtime_database_path_capacity + runtime_database_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int benchmark_lexer_queries(
    struct borrowed_query_list queries,
    size_t iterations,
    struct benchmark_measurement *out_measurement
) {
    uint64_t started = monotonic_now_ns();

    for (size_t iteration = 0U; iteration < iterations; ++iteration) {
        for (size_t query_index = 0U; query_index < queries.count; ++query_index) {
            const struct benchmark_query *query = &queries.items[query_index];
            bool has_error = false;
            size_t token_count = 0U;
            int rc = lex_query(query->sql, query->length, 0U, &token_count, &has_error);

            if (rc != 0) {
                return rc;
            }
            ++out_measurement->operations;
            out_measurement->bytes += query->length;
            out_measurement->token_count += token_count;
            if (has_error) {
                ++out_measurement->error_count;
            } else {
                ++out_measurement->ok_count;
            }
        }
    }
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
    return 0;
}

static int benchmark_owned_lexer_queries(
    const struct mylite_benchmark_owned_query_list *queries,
    size_t iterations,
    struct benchmark_measurement *out_measurement
) {
    uint64_t started = monotonic_now_ns();

    for (size_t iteration = 0U; iteration < iterations; ++iteration) {
        for (size_t query_index = 0U; query_index < queries->count; ++query_index) {
            const struct mylite_benchmark_owned_query *query = &queries->items[query_index];
            bool has_error = false;
            size_t token_count = 0U;
            int rc = lex_query(query->sql, query->length, query->modes, &token_count, &has_error);

            if (rc != 0) {
                return rc;
            }
            ++out_measurement->operations;
            out_measurement->bytes += query->length;
            out_measurement->token_count += token_count;
            if (has_error) {
                ++out_measurement->error_count;
            } else {
                ++out_measurement->ok_count;
            }
        }
    }
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
    return 0;
}

static int lex_query(
    const char *sql,
    size_t length,
    unsigned int modes,
    size_t *out_token_count,
    bool *out_has_error
) {
    struct mylite_sql_lexer lexer = {0};
    struct mylite_sql_token token = {0};

    *out_token_count = 0U;
    *out_has_error = false;
    mylite_sql_lexer_init(
        &lexer,
        (struct mylite_sql_lexer_config){
            .input = sql,
            .length = length,
            .modes = modes,
        }
    );
    for (;;) {
        if (mylite_sql_lexer_next(&lexer, &token) != 0) {
            return 1;
        }
        if (token.kind == MYLITE_SQL_TOKEN_ERROR) {
            *out_has_error = true;
        }
        if (token.kind == MYLITE_SQL_TOKEN_EOF) {
            break;
        }
        ++*out_token_count;
    }
    return 0;
}

static int benchmark_parse_queries(
    struct borrowed_query_list queries,
    size_t iterations,
    struct benchmark_measurement *out_measurement
) {
    uint64_t started = monotonic_now_ns();

    for (size_t iteration = 0U; iteration < iterations; ++iteration) {
        for (size_t query_index = 0U; query_index < queries.count; ++query_index) {
            struct mylite_sql_parse_result result = {0};
            const struct benchmark_query *query = &queries.items[query_index];
            enum mylite_sql_parse_status status = mylite_sql_parse(
                (struct mylite_sql_parse_config){
                    .input = query->sql,
                    .length = query->length,
                    .modes = 0U,
                },
                &result
            );

            ++out_measurement->operations;
            out_measurement->bytes += query->length;
            record_parse_status(out_measurement, status);
            mylite_sql_parse_result_deinit(&result);
        }
    }
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
    return 0;
}

static int benchmark_owned_parse_queries(
    const struct mylite_benchmark_owned_query_list *queries,
    size_t iterations,
    struct benchmark_measurement *out_measurement
) {
    uint64_t started = monotonic_now_ns();

    for (size_t iteration = 0U; iteration < iterations; ++iteration) {
        for (size_t query_index = 0U; query_index < queries->count; ++query_index) {
            struct mylite_sql_parse_result result = {0};
            const struct mylite_benchmark_owned_query *query = &queries->items[query_index];
            enum mylite_sql_parse_status status = mylite_sql_parse(
                (struct mylite_sql_parse_config){
                    .input = query->sql,
                    .length = query->length,
                    .modes = query->modes,
                },
                &result
            );

            ++out_measurement->operations;
            out_measurement->bytes += query->length;
            record_parse_status(out_measurement, status);
            mylite_sql_parse_result_deinit(&result);
        }
    }
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
    return 0;
}

static int dump_parse_failures(
    const struct mylite_benchmark_owned_query_list *queries,
    const char *path
) {
    FILE *file = fopen(path, "wb");

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open parse failure dump: %s\n", path, strerror(errno));
        return 1;
    }
    for (size_t query_index = 0U; query_index < queries->count; ++query_index) {
        struct mylite_sql_parse_result result = {0};
        const struct mylite_benchmark_owned_query *query = &queries->items[query_index];
        enum mylite_sql_parse_status status = mylite_sql_parse(
            (struct mylite_sql_parse_config){
                .input = query->sql,
                .length = query->length,
                .modes = query->modes,
            },
            &result
        );

        if (status != MYLITE_SQL_PARSE_OK) {
            print_parse_failure_row(file, query_index + 1U, status, &result, query);
        }
        mylite_sql_parse_result_deinit(&result);
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "%s: failed to close parse failure dump\n", path);
        return 1;
    }
    return 0;
}

static int report_expected_parse_failures(
    const struct mylite_benchmark_owned_query_list *queries,
    const char *path
) {
    struct mylite_benchmark_expected_parse_failure_list expectations = {0};
    struct expected_parse_failure_summary summary = {0};
    int rc = mylite_benchmark_load_expected_parse_failures(path, &expectations);

    if (rc != 0) {
        return rc;
    }
    rc = classify_expected_parse_failures(queries, &expectations, &summary);
    printf(
        "# expected_parse_failures total=%zu matched=%zu unexpected=%zu mismatched=%zu "
        "missing=%zu\n",
        summary.total_count,
        summary.matched_count,
        summary.unexpected_count,
        summary.mismatched_count,
        summary.missing_count
    );
    mylite_benchmark_expected_parse_failure_list_deinit(&expectations);
    return rc;
}

static int classify_expected_parse_failures(
    const struct mylite_benchmark_owned_query_list *queries,
    const struct mylite_benchmark_expected_parse_failure_list *expectations,
    struct expected_parse_failure_summary *out_summary
) {
    bool *seen = NULL;

    out_summary->total_count = expectations->count;
    if (expectations->count > 0U) {
        seen = (bool *)calloc(expectations->count, sizeof(*seen));
        if (seen == NULL) {
            fprintf(stderr, "out of memory while classifying expected parse failures\n");
            return 1;
        }
    }
    for (size_t query_index = 0U; query_index < queries->count; ++query_index) {
        struct mylite_sql_parse_result result = {0};
        const struct mylite_benchmark_owned_query *query = &queries->items[query_index];
        size_t one_based_query_index = query_index + 1U;
        const struct mylite_benchmark_expected_parse_failure *expectation =
            mylite_benchmark_expected_parse_failure_find(expectations, one_based_query_index);
        enum mylite_sql_parse_status status = mylite_sql_parse(
            (struct mylite_sql_parse_config){
                .input = query->sql,
                .length = query->length,
                .modes = query->modes,
            },
            &result
        );

        if (status != MYLITE_SQL_PARSE_OK) {
            if (expectation == NULL) {
                ++out_summary->unexpected_count;
                print_parse_failure_row(stderr, one_based_query_index, status, &result, query);
            } else {
                size_t expectation_index = (size_t)(expectation - expectations->items);

                seen[expectation_index] = true;
                if (mylite_benchmark_expected_parse_failure_matches(
                        expectation,
                        status,
                        result.error_token.kind
                    )) {
                    ++out_summary->matched_count;
                } else {
                    ++out_summary->mismatched_count;
                    print_parse_expectation_mismatch(
                        expectation,
                        one_based_query_index,
                        status,
                        result.error_token.kind
                    );
                }
            }
        }
        mylite_sql_parse_result_deinit(&result);
    }
    for (size_t index = 0U; index < expectations->count; ++index) {
        if (!seen[index]) {
            ++out_summary->missing_count;
            fprintf(
                stderr,
                "expected parse failure is now missing: query=%zu status=%s token=%s reason=%s\n",
                expectations->items[index].query_index,
                expectations->items[index].status_name,
                expectations->items[index].token_kind_name,
                expectations->items[index].reason
            );
        }
    }
    free(seen);
    return out_summary->unexpected_count == 0U && out_summary->mismatched_count == 0U &&
                   out_summary->missing_count == 0U
               ? 0
               : 1;
}

static void print_parse_expectation_mismatch(
    const struct mylite_benchmark_expected_parse_failure *expectation,
    size_t query_index,
    enum mylite_sql_parse_status status,
    enum mylite_sql_token_kind token_kind
) {
    fprintf(
        stderr,
        "expected parse failure mismatch: query=%zu expected=%s/%s actual=%s/%s reason=%s\n",
        query_index,
        expectation->status_name,
        expectation->token_kind_name,
        mylite_sql_parse_status_name(status),
        mylite_sql_token_kind_name(token_kind),
        expectation->reason
    );
}

static void print_parse_failure_row(
    FILE *file,
    size_t query_index,
    enum mylite_sql_parse_status status,
    const struct mylite_sql_parse_result *result,
    const struct mylite_benchmark_owned_query *query
) {
    struct text_coordinates coordinates =
        compute_offset_coordinates(query->sql, query->length, result->error_token.offset);

    fprintf(
        file,
        "%zu\t%s\t%s\t%zu\t%u\t%u\t",
        query_index,
        mylite_sql_parse_status_name(status),
        mylite_sql_token_kind_name(result->error_token.kind),
        result->error_token.offset,
        coordinates.line,
        coordinates.column
    );
    print_tsv_escaped_field(file, result->error_token.text, result->error_token.length);
    fputc('\t', file);
    print_tsv_escaped_field(file, query->sql, query->length);
    fputc('\n', file);
}

static struct text_coordinates compute_offset_coordinates(
    const char *text,
    size_t length,
    size_t offset
) {
    struct text_coordinates coordinates = {
        .line = 1U,
        .column = 1U,
    };
    size_t limit = offset < length ? offset : length;

    for (size_t index = 0U; text != NULL && index < limit; ++index) {
        if (text[index] == '\r' ||
            (text[index] == '\n' && (index == 0U || text[index - 1U] != '\r'))) {
            ++coordinates.line;
            coordinates.column = 1U;
        } else {
            ++coordinates.column;
        }
    }

    return coordinates;
}

static void print_tsv_escaped_field(FILE *file, const char *text, size_t length) {
    if (text == NULL) {
        return;
    }
    for (size_t index = 0U; index < length; ++index) {
        switch (text[index]) {
        case '\\':
            fputs("\\\\", file);
            break;
        case '\t':
            fputs("\\t", file);
            break;
        case '\n':
            fputs("\\n", file);
            break;
        case '\r':
            fputs("\\r", file);
            break;
        default:
            fputc((unsigned char)text[index], file);
            break;
        }
    }
}

static void record_parse_status(
    struct benchmark_measurement *measurement,
    enum mylite_sql_parse_status status
) {
    if (status == MYLITE_SQL_PARSE_OK) {
        ++measurement->ok_count;
    } else {
        ++measurement->error_count;
    }
    if (status >= 0 && status <= MYLITE_SQL_PARSE_STACK_OVERFLOW) {
        ++measurement->parse_status_counts[(size_t)status];
    }
}

static uint64_t monotonic_now_ns(void) {
#if defined(_WIN32)
    LARGE_INTEGER frequency = {0};
    LARGE_INTEGER counter = {0};

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * (LONGLONG)nanoseconds_per_second) / frequency.QuadPart);
#elif defined(CLOCK_MONOTONIC)
    struct timespec timestamp = {0};

    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    return ((uint64_t)timestamp.tv_sec * nanoseconds_per_second) + (uint64_t)timestamp.tv_nsec;
#else
    struct timespec timestamp = {0};

    timespec_get(&timestamp, TIME_UTC);
    return ((uint64_t)timestamp.tv_sec * nanoseconds_per_second) + (uint64_t)timestamp.tv_nsec;
#endif
}

static void print_result(
    const char *scenario,
    const char *kind,
    size_t iterations,
    size_t query_count,
    const struct benchmark_measurement *measurement
) {
    printf(
        "%s,%s,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%.3f,%.3f,%.3f\n",
        scenario,
        kind,
        iterations,
        query_count,
        measurement->operations,
        measurement->ok_count,
        measurement->error_count,
        measurement->token_count,
        measurement->bytes,
        ns_to_ms(measurement->elapsed_ns),
        ns_to_average_us(measurement->elapsed_ns, measurement->operations),
        ns_to_ops_per_second(measurement->elapsed_ns, measurement->operations)
    );
}

static void print_parse_status_counts(const struct benchmark_measurement *measurement) {
    printf(
        "# parse_status ok=%zu misuse=%zu nomem=%zu lexer_error=%zu syntax_error=%zu "
        "stack_overflow=%zu\n",
        measurement->parse_status_counts[MYLITE_SQL_PARSE_OK],
        measurement->parse_status_counts[MYLITE_SQL_PARSE_MISUSE],
        measurement->parse_status_counts[MYLITE_SQL_PARSE_NOMEM],
        measurement->parse_status_counts[MYLITE_SQL_PARSE_LEXER_ERROR],
        measurement->parse_status_counts[MYLITE_SQL_PARSE_SYNTAX_ERROR],
        measurement->parse_status_counts[MYLITE_SQL_PARSE_STACK_OVERFLOW]
    );
}

static double ns_to_ms(uint64_t ns) {
    return (double)ns / ((double)nanoseconds_per_second / (double)milliseconds_per_second);
}

static double ns_to_average_us(uint64_t ns, size_t operations) {
    if (operations == 0U) {
        return 0.0;
    }
    return ((double)ns / ((double)nanoseconds_per_second / (double)microseconds_per_second)) /
           (double)operations;
}

static double ns_to_ops_per_second(uint64_t ns, size_t operations) {
    if (operations == 0U || ns == 0U) {
        return 0.0;
    }
    return ((double)operations * (double)nanoseconds_per_second) / (double)ns;
}
