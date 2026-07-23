#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

#ifndef P_tmpdir
#  define P_tmpdir "."
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    found_rows_text_capacity = 32,
    scalar_found_rows_column_count = 4,
    found_rows_state_column_count = 3,
    mysql_error_parse = 1064,
    mysql_error_incorrect_parameter_count = 1582,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_scalar_result {
    const char *const *columns;
    const char *const *values;
    size_t count;
    size_t warning_count;
    const char *context;
};

struct expected_found_rows_value {
    const char *expected;
    const char *context;
};

struct expected_found_rows_state {
    const char *found_rows;
    const char *warning_count;
    const char *row_count;
    const char *context;
};

static const char found_rows_warning_message[] =
    "FOUND_ROWS() is deprecated and will be removed in a future release. Consider using COUNT(*) "
    "instead.";
static const char sql_calc_found_rows_warning_message[] =
    "SQL_CALC_FOUND_ROWS is deprecated and will be removed in a future release. Consider using "
    "two separate queries instead.";

static int test_found_rows_scalar_warnings(void);
static int test_found_rows_select_limit_envelope(void);
static int test_sql_calc_found_rows_selects(void);
static int test_sql_calc_found_rows_wordpress_include_query(void);
static int test_sql_calc_found_rows_joined_selects(void);
static int test_found_rows_file_state_and_independent_handles(void);
static int test_found_rows_unsupported_forms(void);
static int prepare_fixture(mylite_db *database);
static int prepare_join_fixture(mylite_db *database);
static int prepare_wordpress_user_post_fixture(mylite_db *database);
static int expect_found_rows_value(mylite_db *database, struct expected_found_rows_value expected);
static int expect_found_rows_state(mylite_db *database, struct expected_found_rows_state expected);
static int expect_warning_rows(
    mylite_db *database,
    size_t row_count,
    const char *message_part,
    const char *context
);
static int expect_single_column_rows(
    const mylite_result *result,
    const char *const *values,
    size_t row_count,
    size_t warning_count,
    const char *context
);
static int expect_two_column_rows(
    const mylite_result *result,
    const char *const (*values)[2],
    size_t row_count,
    size_t warning_count,
    const char *context
);
static int expect_three_column_rows(
    const mylite_result *result,
    const char *const (*values)[3],
    size_t row_count,
    size_t warning_count,
    const char *context
);
static int expect_empty_result(
    const mylite_result *result,
    size_t warning_count,
    const char *context
);
static int expect_empty_column_result(
    const mylite_result *result,
    size_t column_count,
    size_t warning_count,
    const char *context
);
static int expect_scalar_result(
    const mylite_result *result,
    struct expected_scalar_result expected
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_found_rows_scalar_warnings();
    failures += test_found_rows_select_limit_envelope();
    failures += test_sql_calc_found_rows_selects();
    failures += test_sql_calc_found_rows_wordpress_include_query();
    failures += test_sql_calc_found_rows_joined_selects();
    failures += test_found_rows_file_state_and_independent_handles();
    failures += test_found_rows_unsupported_forms();

    return failures == 0 ? 0 : 1;
}

static int test_found_rows_scalar_warnings(void) {
    static const char *const scalar_columns[scalar_found_rows_column_count] = {
        "FOUND_ROWS()",
        "Found_Rows()",
        "FOUND_ROWS ()",
        "(FOUND_ROWS())",
    };
    static const char *const scalar_values[scalar_found_rows_column_count] = {
        "1",
        "1",
        "1",
        "1",
    };
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open scalar found rows");

    failures += execute_ok(
        database,
        "SELECT FOUND_ROWS(), Found_Rows(), FOUND_ROWS (), (FOUND_ROWS()) FROM DUAL",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = scalar_columns,
            .values = scalar_values,
            .count = scalar_found_rows_column_count,
            .warning_count = scalar_found_rows_column_count,
            .context = "found rows scalar labels and warnings",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_warning_rows(
        database,
        scalar_found_rows_column_count,
        found_rows_warning_message,
        "found rows warning rows"
    );

    failures += expect_found_rows_state(
        database,
        (struct expected_found_rows_state){
            .found_rows = "1",
            .warning_count = "1",
            .row_count = "-1",
            .context = "found rows same-statement state",
        }
    );
    failures += expect_warning_rows(
        database,
        1U,
        found_rows_warning_message,
        "single found rows warning row"
    );

    failures += execute_statement_ok(database, "DO 0");
    failures += expect_found_rows_value(
        database,
        (struct expected_found_rows_value){
            .expected = "1",
            .context = "found rows preserved by DO",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_found_rows_select_limit_envelope(void) {
    static const char *const limit_rows[] = {"1", "2"};
    static const char *const offset_rows[] = {"2", "3"};
    static const char *const no_limit_rows[] = {"1", "2", "3", "4"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "limit") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open limit found rows");
    failures += prepare_fixture(database);

    failures += execute_ok(database, "SELECT id FROM t ORDER BY id LIMIT 2", &result);
    failures += expect_single_column_rows(result, limit_rows, 2U, 0U, "ordinary limit rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_state(
        database,
        (struct expected_found_rows_state){
            .found_rows = "2",
            .warning_count = "1",
            .row_count = "-1",
            .context = "ordinary limit found rows",
        }
    );

    failures += execute_ok(database, "SELECT id FROM t ORDER BY id LIMIT 1, 2", &result);
    failures += expect_single_column_rows(result, offset_rows, 2U, 0U, "ordinary offset rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_value(
        database,
        (struct expected_found_rows_value){
            .expected = "3",
            .context = "ordinary offset found rows",
        }
    );

    failures += execute_ok(database, "SELECT id FROM t ORDER BY id LIMIT 10, 2", &result);
    failures += expect_empty_result(result, 0U, "ordinary beyond-offset rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_value(
        database,
        (struct expected_found_rows_value){
            .expected = "4",
            .context = "ordinary beyond-offset found rows",
        }
    );

    failures += execute_ok(database, "SELECT id FROM t ORDER BY id LIMIT 2, 0", &result);
    failures += expect_empty_result(result, 0U, "ordinary zero row-count offset rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_value(
        database,
        (struct expected_found_rows_value){
            .expected = "2",
            .context = "ordinary zero row-count offset found rows",
        }
    );

    failures += execute_ok(database, "SELECT id FROM t ORDER BY id LIMIT 0", &result);
    failures += expect_empty_result(result, 0U, "ordinary limit zero rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_value(
        database,
        (struct expected_found_rows_value){
            .expected = "0",
            .context = "ordinary limit zero found rows",
        }
    );

    failures += execute_ok(database, "SELECT id FROM t ORDER BY id", &result);
    failures += expect_single_column_rows(result, no_limit_rows, 4U, 0U, "ordinary no-limit rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_value(
        database,
        (struct expected_found_rows_value){
            .expected = "4",
            .context = "ordinary no-limit found rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_sql_calc_found_rows_selects(void) {
    static const char *const limit_rows[] = {"1", "2"};
    static const char *const filtered_rows[] = {"2"};
    static const char *const no_limit_rows[] = {"1", "2", "3", "4"};
    static const char *const distinct_n_rows[] = {NULL};
    static const char *const count_rows[] = {"4"};
    static const char *const row_scalar_filtered_rows[][2] = {{"2", "1"}};
    static const char *const row_scalar_wildcard_rows[][3] = {{"1", NULL, "1"}, {"2", "20", "1"}};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "sql_calc") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open sql calc found rows");
    failures += prepare_fixture(database);

    failures +=
        execute_ok(database, "SELECT SQL_CALC_FOUND_ROWS id FROM t ORDER BY id LIMIT 2", &result);
    failures += expect_single_column_rows(result, limit_rows, 2U, 1U, "sql calc limit rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_warning_rows(
        database,
        1U,
        sql_calc_found_rows_warning_message,
        "sql calc warning row"
    );
    failures += expect_found_rows_state(
        database,
        (struct expected_found_rows_state){
            .found_rows = "4",
            .warning_count = "1",
            .row_count = "-1",
            .context = "sql calc limit found rows",
        }
    );

    failures += execute_ok(
        database,
        "SELECT SQL_CALC_FOUND_ROWS id FROM t WHERE n <=> 20 ORDER BY id LIMIT 1",
        &result
    );
    failures += expect_single_column_rows(result, filtered_rows, 1U, 1U, "filtered sql calc row");
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_value(
        database,
        (struct expected_found_rows_value){
            .expected = "2",
            .context = "filtered sql calc found rows",
        }
    );

    failures +=
        execute_ok(database, "SELECT SQL_CALC_FOUND_ROWS id FROM t ORDER BY id LIMIT 0", &result);
    failures += expect_empty_result(result, 1U, "sql calc limit zero rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_value(
        database,
        (struct expected_found_rows_value){
            .expected = "4",
            .context = "sql calc limit zero found rows",
        }
    );

    failures += execute_ok(database, "SELECT SQL_CALC_FOUND_ROWS id FROM t ORDER BY id", &result);
    failures += expect_single_column_rows(result, no_limit_rows, 4U, 1U, "sql calc no-limit rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_value(
        database,
        (struct expected_found_rows_value){
            .expected = "4",
            .context = "sql calc no-limit found rows",
        }
    );

    failures += execute_ok(
        database,
        "SELECT DISTINCT SQL_CALC_FOUND_ROWS n FROM t ORDER BY n LIMIT 1",
        &result
    );
    failures +=
        expect_single_column_rows(result, distinct_n_rows, 1U, 1U, "distinct sql calc rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_value(
        database,
        (struct expected_found_rows_value){
            .expected = "3",
            .context = "distinct sql calc found rows",
        }
    );

    failures += execute_ok(database, "SELECT SQL_CALC_FOUND_ROWS COUNT(*) FROM t", &result);
    failures += expect_single_column_rows(result, count_rows, 1U, 1U, "sql calc count rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_value(
        database,
        (struct expected_found_rows_value){
            .expected = "1",
            .context = "sql calc count found rows",
        }
    );

    failures += execute_ok(
        database,
        "SELECT SQL_CALC_FOUND_ROWS id, 1 AS marker "
        "FROM t WHERE n <=> 20 ORDER BY id LIMIT 1",
        &result
    );
    failures += expect_two_column_rows(
        result,
        row_scalar_filtered_rows,
        1U,
        1U,
        "row-scalar sql calc descriptor and literal row"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_value(
        database,
        (struct expected_found_rows_value){
            .expected = "2",
            .context = "row-scalar filtered sql calc found rows",
        }
    );

    failures += execute_ok(
        database,
        "SELECT SQL_CALC_FOUND_ROWS t.*, 1 AS marker FROM t ORDER BY id LIMIT 2",
        &result
    );
    failures += expect_three_column_rows(
        result,
        row_scalar_wildcard_rows,
        2U,
        1U,
        "row-scalar wildcard sql calc rows"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_value(
        database,
        (struct expected_found_rows_value){
            .expected = "4",
            .context = "row-scalar wildcard sql calc found rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_sql_calc_found_rows_wordpress_include_query(void) {
    static const char *const include_rows[] = {"333", "332"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "wp_include") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open wp include found rows"
    );
    failures += prepare_wordpress_user_post_fixture(database);

    failures += execute_ok(
        database,
        "SELECT SQL_CALC_FOUND_ROWS wptests_users.ID "
        "FROM wptests_users "
        "WHERE 1 = 1 "
        "AND wptests_users.ID IN ("
        "SELECT DISTINCT wptests_posts.post_author FROM wptests_posts "
        "WHERE wptests_posts.post_status = 'publish' "
        "AND wptests_posts.post_type IN ('post', 'page', 'attachment')"
        ") "
        "AND wptests_users.ID IN (333,332) "
        "ORDER BY FIELD(wptests_users.ID, 333,332) ASC "
        "LIMIT 0, 10",
        &result
    );
    failures += expect_single_column_rows(
        result,
        include_rows,
        2U,
        1U,
        "wordpress include query sql calc rows"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_value(
        database,
        (struct expected_found_rows_value){
            .expected = "2",
            .context = "wordpress include query found rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_sql_calc_found_rows_joined_selects(void) {
    static const char *const inner_limit_rows[][2] = {{"1", "7"}};
    static const char *const multi_inner_limit_rows[][3] = {{"1", "7", "30"}};
    static const char *const distinct_inner_limit_rows[] = {"1"};
    static const char *const cartesian_limit_rows[][2] = {{"1", "7"}, {"1", "8"}};
    static const char *const left_limit_rows[][2] = {{"1", "7"}, {"1", "8"}};
    static const char *const right_limit_rows[][2] = {{"1", "7"}, {"1", "8"}};
    static const char *const grouped_left_limit_rows[] = {"1"};
    static const char *const grouped_not_exists_rows[] = {"2"};
    static const char *const ordinary_offset_rows[][2] = {{"1", "8"}};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "joined_sql_calc") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open joined sql calc found rows"
    );
    failures += prepare_join_fixture(database);

    failures += execute_ok(
        database,
        "SELECT SQL_CALC_FOUND_ROWS lefts.id, rights.id "
        "FROM lefts JOIN rights ON lefts.k = rights.k "
        "ORDER BY rights.id LIMIT 1",
        &result
    );
    failures +=
        expect_two_column_rows(result, inner_limit_rows, 1U, 1U, "joined sql calc limit row");
    mylite_result_free(result);
    result = NULL;
    failures += expect_warning_rows(
        database,
        1U,
        sql_calc_found_rows_warning_message,
        "joined sql calc warning row"
    );
    failures += expect_found_rows_state(
        database,
        (struct expected_found_rows_state){
            .found_rows = "2",
            .warning_count = "1",
            .row_count = "-1",
            .context = "joined sql calc found rows",
        }
    );

    failures += execute_ok(
        database,
        "SELECT SQL_CALC_FOUND_ROWS lefts.id, rights.id, extras.id "
        "FROM lefts JOIN rights ON lefts.k = rights.k "
        "JOIN extras ON rights.w = extras.right_w "
        "ORDER BY extras.id LIMIT 1",
        &result
    );
    failures += expect_three_column_rows(
        result,
        multi_inner_limit_rows,
        1U,
        1U,
        "multi-source joined sql calc limit row"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_state(
        database,
        (struct expected_found_rows_state){
            .found_rows = "2",
            .warning_count = "1",
            .row_count = "-1",
            .context = "multi-source joined sql calc found rows",
        }
    );

    failures += execute_ok(
        database,
        "SELECT DISTINCT SQL_CALC_FOUND_ROWS lefts.id "
        "FROM lefts JOIN rights ON lefts.k = rights.k "
        "ORDER BY lefts.id LIMIT 1",
        &result
    );
    failures += expect_single_column_rows(
        result,
        distinct_inner_limit_rows,
        1U,
        1U,
        "distinct joined sql calc rows"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_state(
        database,
        (struct expected_found_rows_state){
            .found_rows = "1",
            .warning_count = "1",
            .row_count = "-1",
            .context = "distinct joined sql calc found rows",
        }
    );

    failures += execute_ok(
        database,
        "SELECT SQL_CALC_FOUND_ROWS lefts.id, rights.id "
        "FROM lefts, rights "
        "ORDER BY lefts.id, rights.id LIMIT 2",
        &result
    );
    failures += expect_two_column_rows(
        result,
        cartesian_limit_rows,
        2U,
        1U,
        "cartesian joined sql calc rows"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_state(
        database,
        (struct expected_found_rows_state){
            .found_rows = "9",
            .warning_count = "1",
            .row_count = "-1",
            .context = "cartesian joined sql calc found rows",
        }
    );

    failures += execute_ok(
        database,
        "SELECT SQL_CALC_FOUND_ROWS lefts.id, rights.id "
        "FROM lefts, rights "
        "WHERE lefts.k = rights.k "
        "ORDER BY rights.id LIMIT 0",
        &result
    );
    failures += expect_empty_column_result(result, 2U, 1U, "comma joined sql calc limit zero");
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_state(
        database,
        (struct expected_found_rows_state){
            .found_rows = "2",
            .warning_count = "1",
            .row_count = "-1",
            .context = "comma joined sql calc found rows",
        }
    );

    failures += execute_ok(
        database,
        "SELECT SQL_CALC_FOUND_ROWS lefts.id, rights.id "
        "FROM lefts LEFT JOIN rights ON lefts.k = rights.k "
        "ORDER BY lefts.id, rights.id LIMIT 2",
        &result
    );
    failures +=
        expect_two_column_rows(result, left_limit_rows, 2U, 1U, "left joined sql calc rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_state(
        database,
        (struct expected_found_rows_state){
            .found_rows = "4",
            .warning_count = "1",
            .row_count = "-1",
            .context = "left joined sql calc found rows",
        }
    );

    failures += execute_ok(
        database,
        "SELECT SQL_CALC_FOUND_ROWS lefts.id, rights.id "
        "FROM lefts RIGHT JOIN rights ON lefts.k = rights.k "
        "ORDER BY rights.id, lefts.id LIMIT 2",
        &result
    );
    failures +=
        expect_two_column_rows(result, right_limit_rows, 2U, 1U, "right joined sql calc rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_state(
        database,
        (struct expected_found_rows_state){
            .found_rows = "3",
            .warning_count = "1",
            .row_count = "-1",
            .context = "right joined sql calc found rows",
        }
    );

    failures += execute_ok(
        database,
        "SELECT SQL_CALC_FOUND_ROWS lefts.id "
        "FROM lefts LEFT JOIN rights ON (lefts.k = rights.k) "
        "GROUP BY lefts.id ORDER BY lefts.id LIMIT 1",
        &result
    );
    failures += expect_single_column_rows(
        result,
        grouped_left_limit_rows,
        1U,
        1U,
        "grouped joined sql calc rows"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_state(
        database,
        (struct expected_found_rows_state){
            .found_rows = "3",
            .warning_count = "1",
            .row_count = "-1",
            .context = "grouped joined sql calc found rows",
        }
    );

    failures += execute_ok(
        database,
        "SELECT SQL_CALC_FOUND_ROWS lefts.id "
        "FROM lefts LEFT JOIN rights ON (lefts.k = rights.k) "
        "WHERE NOT EXISTS ("
        "SELECT 1 FROM rights r2 WHERE r2.k = rights.k LIMIT 1"
        ") "
        "GROUP BY lefts.id ORDER BY lefts.id LIMIT 1",
        &result
    );
    failures += expect_single_column_rows(
        result,
        grouped_not_exists_rows,
        1U,
        1U,
        "grouped joined not exists sql calc rows"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_state(
        database,
        (struct expected_found_rows_state){
            .found_rows = "2",
            .warning_count = "1",
            .row_count = "-1",
            .context = "grouped joined not exists sql calc found rows",
        }
    );

    failures += execute_ok(
        database,
        "SELECT lefts.id, rights.id "
        "FROM lefts JOIN rights ON lefts.k = rights.k "
        "ORDER BY rights.id LIMIT 1, 1",
        &result
    );
    failures +=
        expect_two_column_rows(result, ordinary_offset_rows, 1U, 0U, "ordinary joined offset row");
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_state(
        database,
        (struct expected_found_rows_state){
            .found_rows = "2",
            .warning_count = "1",
            .row_count = "-1",
            .context = "ordinary joined offset found rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_found_rows_file_state_and_independent_handles(void) {
    static const char *const selected_rows[] = {"1"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(first_path, sizeof(first_path), "first") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += prepare_fixture(first);
    failures +=
        execute_ok(first, "SELECT SQL_CALC_FOUND_ROWS id FROM t ORDER BY id LIMIT 1", &result);
    failures +=
        expect_single_column_rows(result, selected_rows, 1U, 1U, "first handle sql calc row");
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_value(
        first,
        (struct expected_found_rows_value){
            .expected = "4",
            .context = "first handle sql calc found rows",
        }
    );
    mylite_close(first);
    first = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(first_path, &first), MYLITE_OK, "reopen first file");
    failures += expect_found_rows_value(
        first,
        (struct expected_found_rows_value){
            .expected = "1",
            .context = "reopened handle session found rows",
        }
    );
    failures += execute_statement_ok(first, "USE app");
    session = mylite_connection_session_state(first);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;
    failures += execute_ok(first, "SELECT id FROM t ORDER BY id LIMIT 1", &result);
    failures += expect_single_column_rows(result, selected_rows, 1U, 0U, "reopened stored row");
    mylite_result_free(result);
    result = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += expect_found_rows_value(
        second,
        (struct expected_found_rows_value){
            .expected = "1",
            .context = "second handle initial found rows",
        }
    );
    failures += expect_found_rows_value(
        first,
        (struct expected_found_rows_value){
            .expected = "1",
            .context = "first handle remains independent",
        }
    );

    session = mylite_connection_session_state(first);
    failures += mylite_test_expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "found rows catalog generation unchanged"
    );
    failures += mylite_test_expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "found rows sqlite schema generation unchanged"
    );
    failures += mylite_test_expect_int(
        read_file_at(first_path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read found rows preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "found rows preamble unchanged"
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int test_found_rows_unsupported_forms(void) {
    static const char *const source_backed_found_rows[] = {"4", "4"};
    static const char *const source_backed_where_rows[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open unsupported found rows"
    );
    failures += prepare_fixture(database);

    failures += execute_error(
        database,
        "SELECT FOUND_ROWS(1)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "FOUND_ROWS",
        }
    );
    failures += execute_error(
        database,
        "SELECT FOUND_ROWS(1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "FOUND_ROWS",
        }
    );
    failures += execute_error(
        database,
        "SELECT FOUND_ROWS",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT",
        }
    );

    failures +=
        execute_ok(database, "SELECT SQL_CALC_FOUND_ROWS id FROM t ORDER BY id LIMIT 2", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT FOUND_ROWS() FROM t ORDER BY id LIMIT 2", &result);
    failures += expect_single_column_rows(
        result,
        source_backed_found_rows,
        2U,
        1U,
        "source-backed found rows projection"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_state(
        database,
        (struct expected_found_rows_state){
            .found_rows = "2",
            .warning_count = "1",
            .row_count = "-1",
            .context = "source-backed found rows projection state",
        }
    );
    failures += execute_ok(
        database,
        "SELECT id FROM t WHERE FOUND_ROWS() = 1 ORDER BY id LIMIT 1",
        &result
    );
    failures += expect_single_column_rows(
        result,
        source_backed_where_rows,
        1U,
        1U,
        "source-backed found rows predicate warning"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_found_rows_state(
        database,
        (struct expected_found_rows_state){
            .found_rows = "1",
            .warning_count = "1",
            .row_count = "-1",
            .context = "source-backed found rows predicate state",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE copy AS SELECT SQL_CALC_FOUND_ROWS id FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CREATE TABLE ... SELECT does not support SQL_CALC_FOUND_ROWS",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t (id) SELECT SQL_CALC_FOUND_ROWS id FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INSERT ... SELECT does not support SQL_CALC_FOUND_ROWS",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO t (id) SELECT SQL_CALC_FOUND_ROWS id FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "REPLACE ... SELECT does not support SQL_CALC_FOUND_ROWS",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int prepare_fixture(mylite_db *database) {
    int failures = 0;

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE t (id INT NOT NULL, n INT NULL)");
    failures +=
        execute_statement_ok(database, "INSERT INTO t VALUES (1, NULL), (2, 20), (3, 20), (4, 30)");
    return failures;
}

static int prepare_join_fixture(mylite_db *database) {
    int failures = 0;

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE lefts (id INT NOT NULL, k INT NULL, v INT NULL)"
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE rights (id INT NOT NULL, k INT NULL, w INT NULL)"
    );
    failures +=
        execute_statement_ok(database, "CREATE TABLE extras (id INT NOT NULL, right_w INT NULL)");
    failures += execute_statement_ok(
        database,
        "INSERT INTO lefts VALUES (1, 10, 100), (2, 20, 200), (3, NULL, 300)"
    );
    failures += execute_statement_ok(
        database,
        "INSERT INTO rights VALUES (7, 10, 700), (8, 10, 800), (9, NULL, 900)"
    );
    failures += execute_statement_ok(
        database,
        "INSERT INTO extras VALUES (30, 700), (31, 800), (32, NULL)"
    );
    return failures;
}

static int prepare_wordpress_user_post_fixture(mylite_db *database) {
    int failures = 0;

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE wptests_users (ID BIGINT UNSIGNED NOT NULL PRIMARY KEY)"
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE wptests_posts ("
        "ID BIGINT UNSIGNED NOT NULL PRIMARY KEY, "
        "post_author BIGINT UNSIGNED NOT NULL, "
        "post_status VARCHAR(20) NOT NULL, "
        "post_type VARCHAR(20) NOT NULL"
        ")"
    );
    failures += execute_statement_ok(database, "INSERT INTO wptests_users VALUES (332), (333)");
    failures += execute_statement_ok(
        database,
        "INSERT INTO wptests_posts VALUES "
        "(10, 333, 'publish', 'post'), "
        "(11, 332, 'publish', 'page'), "
        "(12, 333, 'draft', 'post'), "
        "(13, 331, 'publish', 'post')"
    );
    return failures;
}

static int expect_found_rows_value(mylite_db *database, struct expected_found_rows_value expected) {
    static const char *const columns[] = {"FOUND_ROWS()"};
    const char *values[] = {expected.expected};
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, "SELECT FOUND_ROWS()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = columns,
            .values = values,
            .count = 1U,
            .warning_count = 1U,
            .context = expected.context,
        }
    );
    mylite_result_free(result);
    return failures;
}

static int expect_found_rows_state(mylite_db *database, struct expected_found_rows_state expected) {
    static const char *const columns[found_rows_state_column_count] = {
        "FOUND_ROWS()",
        "@@warning_count",
        "ROW_COUNT()",
    };
    const char *values[found_rows_state_column_count] = {
        expected.found_rows,
        expected.warning_count,
        expected.row_count,
    };
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, "SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = columns,
            .values = values,
            .count = found_rows_state_column_count,
            .warning_count = 1U,
            .context = expected.context,
        }
    );
    mylite_result_free(result);
    return failures;
}

static int expect_warning_rows(
    mylite_db *database,
    size_t row_count,
    const char *message_part,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, "SHOW WARNINGS", &result);
    failures += mylite_test_expect_size(mylite_result_column_count(result), 3U, context);
    failures += mylite_test_expect_size(mylite_result_row_count(result), row_count, context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, context);
    for (size_t row = 0U; row < row_count; ++row) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, row, 0U),
            "Warning",
            context
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, row, 1U),
            "1287",
            context
        );
        failures += mylite_test_expect_contains(
            mylite_result_value_text(result, row, 2U),
            message_part,
            context
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_single_column_rows(
    const mylite_result *result,
    const char *const *values,
    size_t row_count,
    size_t warning_count,
    const char *context
) {
    int failures = 0;

    failures += mylite_test_expect_size(mylite_result_column_count(result), 1U, context);
    failures += mylite_test_expect_size(mylite_result_row_count(result), row_count, context);
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, context);
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), warning_count, context);
    for (size_t row = 0U; row < row_count; ++row) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, row, 0U),
            values[row],
            context
        );
    }
    return failures;
}

static int expect_two_column_rows(
    const mylite_result *result,
    const char *const (*values)[2],
    size_t row_count,
    size_t warning_count,
    const char *context
) {
    int failures = 0;

    failures += mylite_test_expect_size(mylite_result_column_count(result), 2U, context);
    failures += mylite_test_expect_size(mylite_result_row_count(result), row_count, context);
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, context);
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), warning_count, context);
    for (size_t row = 0U; row < row_count; ++row) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, row, 0U),
            values[row][0],
            context
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, row, 1U),
            values[row][1],
            context
        );
    }
    return failures;
}

static int expect_three_column_rows(
    const mylite_result *result,
    const char *const (*values)[3],
    size_t row_count,
    size_t warning_count,
    const char *context
) {
    int failures = 0;

    failures += mylite_test_expect_size(mylite_result_column_count(result), 3U, context);
    failures += mylite_test_expect_size(mylite_result_row_count(result), row_count, context);
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, context);
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), warning_count, context);
    for (size_t row = 0U; row < row_count; ++row) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, row, 0U),
            values[row][0],
            context
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, row, 1U),
            values[row][1],
            context
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, row, 2U),
            values[row][2],
            context
        );
    }
    return failures;
}

static int expect_empty_result(
    const mylite_result *result,
    size_t warning_count,
    const char *context
) {
    int failures = 0;

    failures += mylite_test_expect_size(mylite_result_column_count(result), 1U, context);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, context);
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, context);
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), warning_count, context);
    return failures;
}

static int expect_empty_column_result(
    const mylite_result *result,
    size_t column_count,
    size_t warning_count,
    const char *context
) {
    int failures = 0;

    failures += mylite_test_expect_size(mylite_result_column_count(result), column_count, context);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, context);
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, context);
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), warning_count, context);
    return failures;
}

static int expect_scalar_result(
    const mylite_result *result,
    struct expected_scalar_result expected
) {
    int failures = 0;

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.count,
        expected.context
    );
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        expected.context
    );
    for (size_t column = 0U; column < expected.count; ++column) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0U, column),
            expected.values[column],
            expected.context
        );
    }
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        (void)fprintf(
            stderr,
            "%s: expected success, got rc=%d err=%d state=%s message=%s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    *out_result = result;
    return 0;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        (void)fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    failures += mylite_test_expect_true(result == NULL, "error result is null");
    return failures;
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    (void)remove(related);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_size = 0U;

    if (file == NULL) {
        return -1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        (void)fclose(file);
        return -1;
    }
    read_size = fread(buffer, 1U, size, file);
    if (fclose(file) != 0) {
        return -1;
    }
    return read_size == size ? 0 : -1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) == 0) {
        return 0;
    }
    (void)fprintf(stderr, "%s: byte comparison failed\n", context);
    return 1;
}
