#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
    mysql_error_incorrect_arguments = 1210,
    seed_post_count = 7,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static int test_window_rank_distribution_results(void);
static int test_window_navigation_results(void);
static int test_window_function_metadata(void);
static int test_window_function_diagnostics(void);
static int open_app_database(mylite_db **out_database, char *path, size_t path_size);
static int seed_posts(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_window_rank_distribution_results();
    failures += test_window_navigation_results();
    failures += test_window_function_metadata();
    failures += test_window_function_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_window_rank_distribution_results(void) {
    static const char *const no_source_columns[] = {"r", "dr", "pr", "cd", "nt"};
    static const char *const no_source_values[] = {"1", "1", "0", "1", "1"};
    static const char *const ranking_columns[] = {
        "id",
        "created_at",
        "r",
        "dr",
        "pr",
        "cd",
        "nt",
    };
    static const char *const ranking_values[] = {
        "4",
        NULL,
        "1",
        "1",
        "0",
        "0.14285714285714285",
        "1",
        "6",
        "10",
        "2",
        "2",
        "0.16666666666666666",
        "0.2857142857142857",
        "1",
        "7",
        "20",
        "3",
        "3",
        "0.3333333333333333",
        "0.42857142857142855",
        "1",
        "5",
        "50",
        "4",
        "4",
        "0.5",
        "0.5714285714285714",
        "2",
        "1",
        "100",
        "5",
        "5",
        "0.6666666666666666",
        "0.7142857142857143",
        "2",
        "2",
        "200",
        "6",
        "6",
        "0.8333333333333334",
        "1",
        "3",
        "3",
        "200",
        "6",
        "6",
        "0.8333333333333334",
        "1",
        "3",
    };
    static const char *const partition_columns[] =
        {"id", "author_id", "created_at", "r", "dr", "nt"};
    static const char *const partition_values[] = {
        "7",   NULL, "20", "1",  "1",  "1",  "6",   NULL, "10", "2",  "2",  "2",  "2",   "10",
        "200", "1",  "1",  "1",  "3",  "10", "200", "1",  "1",  "1",  "1",  "10", "100", "3",
        "2",   "2",  "5",  "20", "50", "1",  "1",   "1",  "4",  "20", NULL, "2",  "2",   "2",
    };
    static const char *const frame_columns[] = {"id", "r", "dr", "pr", "cd", "nt"};
    static const char *const frame_values[] = {
        "4",
        "1",
        "1",
        "0",
        "0.14285714285714285",
        "1",
        "6",
        "2",
        "2",
        "0.16666666666666666",
        "0.2857142857142857",
        "1",
        "7",
        "3",
        "3",
        "0.3333333333333333",
        "0.42857142857142855",
        "1",
        "5",
        "4",
        "4",
        "0.5",
        "0.5714285714285714",
        "2",
        "1",
        "5",
        "5",
        "0.6666666666666666",
        "0.7142857142857143",
        "2",
        "2",
        "6",
        "6",
        "0.8333333333333334",
        "1",
        "3",
        "3",
        "6",
        "6",
        "0.8333333333333334",
        "1",
        "3",
    };
    static const char *const partition_frame_columns[] = {"id", "r", "dr", "nt"};
    static const char *const partition_frame_values[] = {
        "7", "1", "1", "1", "6", "2", "2", "2", "2", "1", "1", "1", "3", "1",
        "1", "1", "1", "3", "2", "2", "5", "1", "1", "1", "4", "2", "2", "2",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = open_app_database(&database, path, sizeof(path));

    if (failures == 0) {
        failures += seed_posts(database);
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT RANK() OVER () AS r, DENSE_RANK() OVER () AS dr, "
                   "PERCENT_RANK() OVER () AS pr, CUME_DIST() OVER () AS cd, "
                   "NTILE(1) OVER () AS nt",
            .columns = no_source_columns,
            .column_count = sizeof(no_source_columns) / sizeof(no_source_columns[0]),
            .values = no_source_values,
            .row_count = 1U,
            .context = "no-source rank distribution",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, created_at, RANK() OVER (ORDER BY created_at) AS r, "
                   "DENSE_RANK() OVER (ORDER BY created_at) AS dr, "
                   "PERCENT_RANK() OVER (ORDER BY created_at) AS pr, "
                   "CUME_DIST() OVER (ORDER BY created_at) AS cd, "
                   "NTILE(3) OVER (ORDER BY created_at) AS nt "
                   "FROM posts ORDER BY created_at, id",
            .columns = ranking_columns,
            .column_count = sizeof(ranking_columns) / sizeof(ranking_columns[0]),
            .values = ranking_values,
            .row_count = seed_post_count,
            .context = "rank distribution ntile nullable order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, author_id, created_at, "
                   "RANK() OVER (PARTITION BY author_id ORDER BY created_at DESC) AS r, "
                   "DENSE_RANK() OVER "
                   "(PARTITION BY author_id ORDER BY created_at DESC) AS dr, "
                   "NTILE(2) OVER (PARTITION BY author_id ORDER BY created_at DESC) AS nt "
                   "FROM posts ORDER BY author_id, created_at DESC, id",
            .columns = partition_columns,
            .column_count = sizeof(partition_columns) / sizeof(partition_columns[0]),
            .values = partition_values,
            .row_count = seed_post_count,
            .context = "partitioned rank distribution",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "RANK() OVER (ORDER BY created_at "
                   "ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS r, "
                   "DENSE_RANK() OVER (ORDER BY created_at RANGE CURRENT ROW) AS dr, "
                   "PERCENT_RANK() OVER (ORDER BY created_at ROWS 1 PRECEDING) AS pr, "
                   "CUME_DIST() OVER "
                   "(ORDER BY created_at ROWS BETWEEN CURRENT ROW AND UNBOUNDED FOLLOWING) AS cd, "
                   "NTILE(3) OVER "
                   "(ORDER BY created_at ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING) AS nt "
                   "FROM posts ORDER BY created_at, id",
            .columns = frame_columns,
            .column_count = sizeof(frame_columns) / sizeof(frame_columns[0]),
            .values = frame_values,
            .row_count = seed_post_count,
            .context = "rank distribution frame clauses",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "RANK() OVER (PARTITION BY author_id ORDER BY created_at DESC "
                   "ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS r, "
                   "DENSE_RANK() OVER (PARTITION BY author_id ORDER BY created_at DESC "
                   "RANGE CURRENT ROW) AS dr, "
                   "NTILE(2) OVER (PARTITION BY author_id ORDER BY created_at DESC "
                   "ROWS 1 PRECEDING) AS nt "
                   "FROM posts ORDER BY author_id, created_at DESC, id",
            .columns = partition_frame_columns,
            .column_count = sizeof(partition_frame_columns) / sizeof(partition_frame_columns[0]),
            .values = partition_frame_values,
            .row_count = seed_post_count,
            .context = "partitioned rank distribution frame clauses",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_window_navigation_results(void) {
    static const char *const navigation_columns[] = {
        "id",
        "title",
        "lag_one",
        "lag_two",
        "lead_one",
        "lead_two",
        "first_title",
        "last_title",
        "nth_title",
    };
    static const char *const navigation_values[] = {
        "1", "a", NULL, "x", "b", "c", "a", "a", NULL, "2", "b",  "a", "x", "c", "d", "a",
        "b", "b", "3",  "c", "b", "a", "d", "e", "a",  "c", "b",  "4", "d", "c", "b", "e",
        "f", "a", "d",  "b", "5", "e", "d", "c", "f",  "g", "a",  "e", "b", "6", "f", "e",
        "d", "g", "z",  "a", "f", "b", "7", "g", "f",  "e", NULL, "z", "a", "g", "b",
    };
    static const char *const partition_columns[] = {
        "id",
        "author_id",
        "title",
        "first_title",
        "last_title",
        "nth_title",
    };
    static const char *const partition_values[] = {
        "6", NULL, "f", "f",  "f", NULL, "7", NULL, "g", "f",  "g", "g",  "1", "10",
        "a", "a",  "a", NULL, "2", "10", "b", "a",  "b", "b",  "3", "10", "c", "a",
        "c", "b",  "4", "20", "d", "d",  "d", NULL, "5", "20", "e", "d",  "e", "e",
    };
    static const char *const zero_offset_columns[] = {"id", "lag_zero", "lead_zero"};
    static const char *const zero_offset_values[] = {
        "1", "a", "a", "2", "b", "b", "3", "c", "c", "4", "d",
        "d", "5", "e", "e", "6", "f", "f", "7", "g", "g",
    };
    static const char *const expression_argument_columns[] = {
        "id",
        "lag_expr",
        "lead_expr",
        "first_expr",
        "nth_expr",
    };
    static const char *const expression_argument_values[] = {
        "1",      "xa",     "12",     "aalpha", NULL,     "2",      "aalpha", "13",     "aalpha",
        "12",     "3",      "balpha", "14",     "aalpha", "12",     "4",      "cAlpha", "15",
        "aalpha", "12",     "5",      NULL,     "16",     "aalpha", "12",     "6",      "ebeta",
        "17",     "aalpha", "12",     "7",      NULL,     "107",    "aalpha", "12",
    };
    static const char *const column_default_columns[] = {"id", "lag_default"};
    static const char *const column_default_values[] = {
        "1",
        "alpha",
        "2",
        "a",
        "3",
        "b",
        "4",
        "c",
        "5",
        "d",
        "6",
        "e",
        "7",
        "f",
    };
    static const char *const value_frame_columns[] = {
        "id",
        "first_running",
        "last_running",
        "nth_running",
        "first_forward",
        "last_forward",
        "nth_forward",
    };
    static const char *const value_frame_values[] = {
        "4", "d", "d", NULL, "d", "c", "f", "6", "d", "f", "f", "f", "c", "g", "7",  "d", "g",
        "f", "g", "c", "e",  "5", "d", "e", "f", "e", "c", "a", "1", "d", "a", "f",  "a", "c",
        "b", "2", "d", "b",  "f", "b", "c", "c", "3", "d", "c", "f", "c", "c", NULL,
    };
    static const char *const range_frame_columns[] =
        {"id", "first_range", "last_range", "nth_range"};
    static const char *const range_frame_values[] = {
        "4", "d", "d", NULL, "6", "d", "f", "f", "7", "d", "g", "f", "5", "d",
        "e", "f", "1", "d",  "a", "f", "2", "d", "c", "f", "3", "d", "c", "f",
    };
    static const char *const offset_frame_columns[] = {
        "id",
        "first_rows",
        "last_rows",
        "nth_rows",
        "first_between",
        "last_between",
        "nth_between",
    };
    static const char *const offset_frame_values[] = {
        "4", "d", "d", NULL, "d", "f", "f", "6", "d", "f", "f", "d", "g", "f", "7", "f", "g",
        "g", "f", "e", "g",  "5", "g", "e", "e", "g", "a", "e", "1", "e", "a", "a", "e", "b",
        "a", "2", "a", "b",  "b", "a", "c", "b", "3", "b", "c", "c", "b", "c", "c",
    };
    static const char *const empty_offset_frame_columns[] =
        {"id", "empty_preceding", "empty_following"};
    static const char *const empty_offset_frame_values[] = {
        "4",  NULL, NULL, "6",  NULL, NULL, "7",  NULL, NULL, "5",  NULL,
        NULL, "1",  NULL, NULL, "2",  NULL, NULL, "3",  NULL, NULL,
    };
    static const char *const lag_lead_frame_columns[] = {"id", "lag_title", "lead_title"};
    static const char *const lag_lead_frame_values[] = {
        "4", NULL, "f", "6", "d", "g", "7", "f", "e", "5",  "g",
        "a", "1",  "e", "b", "2", "a", "c", "3", "b", NULL,
    };
    static const char *const ignored_range_offset_columns[] = {"id", "rank_range", "lag_range"};
    static const char *const ignored_range_offset_values[] = {
        "4", "1", NULL, "6", "2", "d", "7", "3", "f", "5", "4",
        "g", "1", "5",  "e", "2", "6", "a", "3", "6", "b",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = open_app_database(&database, path, sizeof(path));

    if (failures == 0) {
        failures += seed_posts(database);
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, title, LAG(title) OVER (ORDER BY id) AS lag_one, "
                   "LAG(title, 2, 'x') OVER (ORDER BY id) AS lag_two, "
                   "LEAD(title) OVER (ORDER BY id) AS lead_one, "
                   "LEAD(title, 2, 'z') OVER (ORDER BY id) AS lead_two, "
                   "FIRST_VALUE(title) OVER (ORDER BY id) AS first_title, "
                   "LAST_VALUE(title) OVER (ORDER BY id) AS last_title, "
                   "NTH_VALUE(title, 2) OVER (ORDER BY id) AS nth_title "
                   "FROM posts ORDER BY id",
            .columns = navigation_columns,
            .column_count = sizeof(navigation_columns) / sizeof(navigation_columns[0]),
            .values = navigation_values,
            .row_count = seed_post_count,
            .context = "navigation functions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, author_id, title, "
                   "FIRST_VALUE(title) OVER (PARTITION BY author_id ORDER BY id) AS first_title, "
                   "LAST_VALUE(title) OVER (PARTITION BY author_id ORDER BY id) AS last_title, "
                   "NTH_VALUE(title, 2) OVER (PARTITION BY author_id ORDER BY id) AS nth_title "
                   "FROM posts ORDER BY author_id, id",
            .columns = partition_columns,
            .column_count = sizeof(partition_columns) / sizeof(partition_columns[0]),
            .values = partition_values,
            .row_count = seed_post_count,
            .context = "partitioned frame value functions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LAG(title, 0, 'x') OVER (ORDER BY id) AS lag_zero, "
                   "LEAD(title, 0, 'z') OVER (ORDER BY id) AS lead_zero "
                   "FROM posts ORDER BY id",
            .columns = zero_offset_columns,
            .column_count = sizeof(zero_offset_columns) / sizeof(zero_offset_columns[0]),
            .values = zero_offset_values,
            .row_count = seed_post_count,
            .context = "zero offset navigation",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "LAG(CONCAT(title, category), 1, CONCAT('x', title)) "
                   "OVER (ORDER BY id) AS lag_expr, "
                   "LEAD(id + 10, 1, id + 100) OVER (ORDER BY id) AS lead_expr, "
                   "FIRST_VALUE(CONCAT(title, category)) OVER (ORDER BY id) AS first_expr, "
                   "NTH_VALUE(id + 10, 2) OVER (ORDER BY id) AS nth_expr "
                   "FROM posts ORDER BY id",
            .columns = expression_argument_columns,
            .column_count =
                sizeof(expression_argument_columns) / sizeof(expression_argument_columns[0]),
            .values = expression_argument_values,
            .row_count = seed_post_count,
            .context = "window value and default expression arguments",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LAG(title, 1, category) OVER (ORDER BY id) AS lag_default "
                   "FROM posts ORDER BY id",
            .columns = column_default_columns,
            .column_count = sizeof(column_default_columns) / sizeof(column_default_columns[0]),
            .values = column_default_values,
            .row_count = seed_post_count,
            .context = "window column default argument",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "FIRST_VALUE(title) OVER (ORDER BY created_at "
                   "ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS first_running, "
                   "LAST_VALUE(title) OVER (ORDER BY created_at "
                   "ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS last_running, "
                   "NTH_VALUE(title, 2) OVER (ORDER BY created_at "
                   "ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS nth_running, "
                   "FIRST_VALUE(title) OVER (ORDER BY created_at "
                   "ROWS BETWEEN CURRENT ROW AND UNBOUNDED FOLLOWING) AS first_forward, "
                   "LAST_VALUE(title) OVER (ORDER BY created_at "
                   "ROWS BETWEEN CURRENT ROW AND UNBOUNDED FOLLOWING) AS last_forward, "
                   "NTH_VALUE(title, 2) OVER (ORDER BY created_at "
                   "ROWS BETWEEN CURRENT ROW AND UNBOUNDED FOLLOWING) AS nth_forward "
                   "FROM posts ORDER BY created_at, id",
            .columns = value_frame_columns,
            .column_count = sizeof(value_frame_columns) / sizeof(value_frame_columns[0]),
            .values = value_frame_values,
            .row_count = seed_post_count,
            .context = "value function row frames",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "FIRST_VALUE(title) OVER (ORDER BY created_at "
                   "RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS first_range, "
                   "LAST_VALUE(title) OVER (ORDER BY created_at "
                   "RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS last_range, "
                   "NTH_VALUE(title, 2) OVER (ORDER BY created_at "
                   "RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS nth_range "
                   "FROM posts ORDER BY created_at, id",
            .columns = range_frame_columns,
            .column_count = sizeof(range_frame_columns) / sizeof(range_frame_columns[0]),
            .values = range_frame_values,
            .row_count = seed_post_count,
            .context = "value function range frames",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "FIRST_VALUE(title) OVER (ORDER BY created_at ROWS 1 PRECEDING) "
                   "AS first_rows, "
                   "LAST_VALUE(title) OVER (ORDER BY created_at ROWS 1 PRECEDING) "
                   "AS last_rows, "
                   "NTH_VALUE(title, 2) OVER (ORDER BY created_at ROWS 1 PRECEDING) "
                   "AS nth_rows, "
                   "FIRST_VALUE(title) OVER "
                   "(ORDER BY created_at ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING) "
                   "AS first_between, "
                   "LAST_VALUE(title) OVER "
                   "(ORDER BY created_at ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING) "
                   "AS last_between, "
                   "NTH_VALUE(title, 2) OVER "
                   "(ORDER BY created_at ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING) "
                   "AS nth_between "
                   "FROM posts ORDER BY created_at, id",
            .columns = offset_frame_columns,
            .column_count = sizeof(offset_frame_columns) / sizeof(offset_frame_columns[0]),
            .values = offset_frame_values,
            .row_count = seed_post_count,
            .context = "value function offset frames",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "FIRST_VALUE(title) OVER "
                   "(ORDER BY created_at ROWS BETWEEN 1 PRECEDING AND 2 PRECEDING) "
                   "AS empty_preceding, "
                   "FIRST_VALUE(title) OVER "
                   "(ORDER BY created_at ROWS BETWEEN 2 FOLLOWING AND 1 FOLLOWING) "
                   "AS empty_following "
                   "FROM posts ORDER BY created_at, id",
            .columns = empty_offset_frame_columns,
            .column_count =
                sizeof(empty_offset_frame_columns) / sizeof(empty_offset_frame_columns[0]),
            .values = empty_offset_frame_values,
            .row_count = seed_post_count,
            .context = "value function empty offset frames",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "LAG(title) OVER (ORDER BY created_at "
                   "ROWS BETWEEN CURRENT ROW AND CURRENT ROW) AS lag_title, "
                   "LEAD(title) OVER (ORDER BY created_at "
                   "ROWS BETWEEN CURRENT ROW AND CURRENT ROW) AS lead_title "
                   "FROM posts ORDER BY created_at, id",
            .columns = lag_lead_frame_columns,
            .column_count = sizeof(lag_lead_frame_columns) / sizeof(lag_lead_frame_columns[0]),
            .values = lag_lead_frame_values,
            .row_count = seed_post_count,
            .context = "lag lead ignored frames",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "RANK() OVER (ORDER BY created_at RANGE 1 PRECEDING) AS rank_range, "
                   "LAG(title) OVER (ORDER BY created_at RANGE 1 PRECEDING) AS lag_range "
                   "FROM posts ORDER BY created_at, id",
            .columns = ignored_range_offset_columns,
            .column_count =
                sizeof(ignored_range_offset_columns) / sizeof(ignored_range_offset_columns[0]),
            .values = ignored_range_offset_values,
            .row_count = seed_post_count,
            .context = "ignored range offset frames",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_window_function_metadata(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = open_app_database(&database, path, sizeof(path));

    if (failures == 0) {
        failures += seed_posts(database);
    }
    if (failures == 0) {
        failures += execute_ok(
            database,
            "SELECT RANK() OVER (ORDER BY id) AS r, "
            "PERCENT_RANK() OVER (ORDER BY id) AS pr, "
            "NTILE(2) OVER (ORDER BY id) AS nt, "
            "LAG(title) OVER (ORDER BY id) AS lag_title "
            "FROM posts ORDER BY id LIMIT 1",
            &result
        );
    }
    if (failures != 0) {
        mylite_result_free(result);
        mylite_close(database);
        remove_related_files(path);
        return failures;
    }

    failures += expect_size(mylite_result_column_count(result), 4U, "metadata column count");
    failures += expect_int(
        mylite_result_column_type(result, 0U),
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
        "rank metadata type"
    );
    failures += expect_int(mylite_result_column_nullable(result, 0U), 0, "rank nullable");
    failures += expect_int(
        (int)(mylite_result_column_flags(result, 0U) & MYLITE_RESULT_COLUMN_FLAG_UNSIGNED),
        MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
        "rank unsigned"
    );
    failures += expect_int(
        (int)(mylite_result_column_flags(result, 0U) & MYLITE_RESULT_COLUMN_FLAG_NOT_NULL),
        MYLITE_RESULT_COLUMN_FLAG_NOT_NULL,
        "rank not null"
    );
    failures += expect_int(
        mylite_result_column_type(result, 1U),
        MYLITE_RESULT_COLUMN_TYPE_DOUBLE,
        "percent_rank metadata type"
    );
    failures += expect_int(mylite_result_column_nullable(result, 1U), 0, "percent_rank nullable");
    failures += expect_int(
        mylite_result_column_type(result, 2U),
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
        "ntile metadata type"
    );
    failures += expect_int(
        mylite_result_column_type(result, 3U),
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        "lag metadata type"
    );
    failures += expect_int(mylite_result_column_nullable(result, 3U), 1, "lag nullable");
    failures += expect_int(
        (int)(mylite_result_column_flags(result, 3U) & MYLITE_RESULT_COLUMN_FLAG_NOT_NULL),
        0,
        "lag not-not-null"
    );

    mylite_result_free(result);
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_window_function_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = open_app_database(&database, path, sizeof(path));

    if (failures == 0) {
        failures += seed_posts(database);
    }

    failures += execute_error(
        database,
        "SELECT COUNT(*) OVER () FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "aggregate window functions are not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, SUM(id) OVER (PARTITION BY author_id ORDER BY id) AS running_id FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "aggregate window functions are not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT RANK(1) OVER ()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "SELECT RANK() OVER (ORDER BY id)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "RANK() without a table source supports only OVER ()",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, RANK() OVER (ORDER BY missing) AS r FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, ROW_NUMBER() OVER w AS rn FROM posts WINDOW w AS (ORDER BY id)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "does not yet support WINDOW clauses",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, RANK() OVER (ORDER BY id ROWS BETWEEN UNBOUNDED FOLLOWING AND "
        "CURRENT ROW) AS r FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "RANK() frame start cannot be UNBOUNDED FOLLOWING",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, FIRST_VALUE(title) OVER (ORDER BY id RANGE 1 PRECEDING) AS first_title "
        "FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FIRST_VALUE() RANGE frame offsets are not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, FIRST_VALUE(title) OVER "
        "(ORDER BY id ROWS BETWEEN CURRENT ROW AND 1 PRECEDING) AS first_title FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FIRST_VALUE() frame start cannot be after frame end",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, LAG(missing) OVER (ORDER BY id) AS previous FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, NTILE(0) OVER (ORDER BY id) AS bucket FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_arguments,
            .sqlstate = "HY000",
            .message_part = "Incorrect arguments to ntile",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, NTILE(NULL) OVER (ORDER BY id) AS bucket FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, NTH_VALUE(title, 0) OVER (ORDER BY id) AS nth_title FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_arguments,
            .sqlstate = "HY000",
            .message_part = "Incorrect arguments to nth_value",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, NTH_VALUE(title, -1) OVER (ORDER BY id) AS nth_title FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_arguments,
            .sqlstate = "HY000",
            .message_part = "Incorrect arguments to nth_value",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, LAG(title, NULL) OVER (ORDER BY id) AS previous FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, LEAD(title, -1) OVER (ORDER BY id) AS next_title FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, LAG(title, id) OVER (ORDER BY id) AS previous FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "offset arguments support only scalar literals",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int open_app_database(mylite_db **out_database, char *path, size_t path_size) {
    int failures = 0;

    if (make_test_path(path, path_size, "window-rank-navigation") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, "open database");

    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
}

static int seed_posts(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(
        database,
        "CREATE TABLE posts("
        "id INT, author_id INT, created_at INT, category VARCHAR(20), title VARCHAR(20)"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO posts VALUES "
        "(1, 10, 100, 'alpha', 'a'), "
        "(2, 10, 200, 'alpha', 'b'), "
        "(3, 10, 200, 'Alpha', 'c'), "
        "(4, 20, NULL, NULL, 'd'), "
        "(5, 20, 50, 'beta', 'e'), "
        "(6, NULL, 10, NULL, 'f'), "
        "(7, NULL, 20, 'beta', 'g')",
        NULL
    );
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': rc=%d err=%d state=%s message=%s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[value_index],
                expected.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s row %zu column %zu: expected %s, got %s\n",
                context,
                row,
                column,
                expected == NULL ? "(null)" : expected,
                actual == NULL ? "(null)" : actual
            );
            return 1;
        }
        return 0;
    }
    return expect_text(actual, expected, context);
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
        );
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected %s, got %s\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
        );
        return 1;
    }
    return 0;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(path, path_size, "/tmp/mylite-%s-%d.mylite", name, current_process_id());

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}
