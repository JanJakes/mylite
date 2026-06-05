#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    role_count_column_count = 7,
    mysql_error_column_ambiguous = 1052,
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_joined_aggregate_values_and_persistence(void);
static int test_joined_aggregate_diagnostics(void);
static int test_independent_joined_aggregate_handles(void);
static int seed_joined_aggregate_schema(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_row_count(mylite_db *database, const char *expected, const char *context);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_joined_aggregate_values_and_persistence();
    failures += test_joined_aggregate_diagnostics();
    failures += test_independent_joined_aggregate_handles();

    return failures == 0 ? 0 : 1;
}

static int test_joined_aggregate_values_and_persistence(void) {
    static const char *const count_star_columns[] = {"id", "COUNT(*)"};
    static const char *const count_star_values[] = {"1", "2", "2", "1", "3", "1", "4", "1"};
    static const char *const count_column_columns[] = {"id", "COUNT(c.id)"};
    static const char *const count_column_values[] = {"1", "2", "2", "1", "3", "0", "4", "0"};
    static const char *const min_columns[] = {"id", "MIN(c.score)"};
    static const char *const min_values[] = {"1", "5", "2", "7", "3", NULL, "4", NULL};
    static const char *const max_columns[] = {"id", "MAX(c.score)"};
    static const char *const max_values[] = {"1", "5", "2", "7", "3", NULL, "4", NULL};
    static const char *const sum_columns[] = {"id", "SUM(c.score)"};
    static const char *const sum_values[] = {"1", "5", "2", "7", "3", NULL, "4", NULL};
    static const char *const avg_columns[] = {"id", "AVG(c.score)"};
    static const char *const avg_values[] = {"1", "5.0000", "2", "7.0000", "3", NULL, "4", NULL};
    static const char *const multi_columns[] = {"id", "c", "s", "a"};
    static const char *const multi_values[] = {
        "1",
        "2",
        "5",
        "5.0000",
        "2",
        "1",
        "7",
        "7.0000",
        "3",
        "0",
        NULL,
        NULL,
        "4",
        "0",
        NULL,
        NULL,
    };
    static const char *const multi_order_values[] = {
        "2",
        "1",
        "7",
        "7.0000",
        "1",
        "2",
        "5",
        "5.0000",
    };
    static const char *const joined_multi_key_columns[] = {"id", "id", "COUNT(*)", "SUM(c.score)"};
    static const char *const joined_multi_key_values[] = {
        "1",
        "101",
        "1",
        "5",
        "1",
        "102",
        "1",
        NULL,
        "2",
        "103",
        "1",
        "7",
    };
    static const char *const comment_meta_order_columns[] = {"id"};
    static const char *const comment_meta_descriptor_order_values[] = {
        "101",
        "102",
        "103",
        "105",
    };
    static const char *const comment_meta_cast_order_values[] = {
        "102",
        "105",
        "103",
        "101",
    };
    static const char *const comment_meta_two_cast_order_values[] = {
        "101",
        "103",
        "105",
        "102",
    };
    static const char *const role_count_columns[] = {
        "administrators",
        "editors",
        "authors",
        "contributors",
        "subscribers",
        "empty_roles",
        "total_roles",
    };
    static const char *const role_count_values[] = {"1", "1", "0", "0", "1", "1", "4"};
    static const char *const distinct_join_count_columns[] = {"COUNT(*)"};
    static const char *const distinct_join_count_values[] = {"4"};
    static const char *const limited_join_count_values[] = {"4"};
    static const char *const inner_count_columns[] = {"post_id", "COUNT(*)"};
    static const char *const inner_count_values[] = {"1", "2", "2", "1"};
    static const char *const category_count_columns[] = {"category", "COUNT(c.id)"};
    static const char *const category_count_values[] = {NULL, "0", "10", "1", "20", "0"};
    static const char *const having_columns[] = {"post_id", "c"};
    static const char *const having_values[] = {"2", "1", "1", "2"};
    static const char *const offset_values[] = {"2", "1", "3", "0"};
    static const char *const bit_and_columns[] = {"category", "BIT_AND(c.score)"};
    static const char *const bit_and_values[] =
        {NULL, "18446744073709551615", "10", "5", "20", "18446744073709551615"};
    static const char *const bit_or_columns[] = {"category", "BIT_OR(c.score)"};
    static const char *const bit_or_values[] = {NULL, "0", "10", "7", "20", "0"};
    static const char *const bit_xor_columns[] = {"category", "BIT_XOR(c.score)"};
    static const char *const bit_xor_values[] = {NULL, "0", "10", "2", "20", "0"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open joined aggregate file");
    failures += seed_joined_aggregate_schema(database);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.id, COUNT(*) FROM posts AS p LEFT JOIN comments AS c "
                   "ON p.id = c.post_id GROUP BY p.id ORDER BY p.id",
            .columns = count_star_columns,
            .column_count = 2U,
            .values = count_star_values,
            .row_count = 4U,
            .context = "left join grouped count star counts null-extended rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.id, COUNT(c.id) FROM posts AS p LEFT JOIN comments AS c "
                   "ON p.id = c.post_id GROUP BY p.id ORDER BY id",
            .columns = count_column_columns,
            .column_count = 2U,
            .values = count_column_values,
            .row_count = 4U,
            .context =
                "left join grouped count column skips null-extended rows and orders by label",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.id, MIN(c.score) FROM posts AS p LEFT JOIN comments AS c "
                   "ON p.id = c.post_id GROUP BY p.id ORDER BY p.id",
            .columns = min_columns,
            .column_count = 2U,
            .values = min_values,
            .row_count = 4U,
            .context = "left join grouped min",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.id, MAX(c.score) FROM posts AS p LEFT JOIN comments AS c "
                   "ON p.id = c.post_id GROUP BY p.id ORDER BY p.id",
            .columns = max_columns,
            .column_count = 2U,
            .values = max_values,
            .row_count = 4U,
            .context = "left join grouped max",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.id, SUM(c.score) FROM posts AS p LEFT JOIN comments AS c "
                   "ON p.id = c.post_id GROUP BY p.id ORDER BY p.id",
            .columns = sum_columns,
            .column_count = 2U,
            .values = sum_values,
            .row_count = 4U,
            .context = "left join grouped sum",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.id, AVG(c.score) FROM posts AS p LEFT JOIN comments AS c "
                   "ON p.id = c.post_id GROUP BY p.id ORDER BY p.id",
            .columns = avg_columns,
            .column_count = 2U,
            .values = avg_values,
            .row_count = 4U,
            .context = "left join grouped avg",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.id, COUNT(c.id) AS c, SUM(c.score) AS s, AVG(c.score) AS a "
                   "FROM posts AS p LEFT JOIN comments AS c ON p.id = c.post_id "
                   "GROUP BY p.id ORDER BY p.id",
            .columns = multi_columns,
            .column_count = 4U,
            .values = multi_values,
            .row_count = 4U,
            .context = "left join grouped multiple aggregates",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.id, COUNT(c.id) AS c, SUM(c.score) AS s, AVG(c.score) AS a "
                   "FROM posts AS p LEFT JOIN comments AS c ON p.id = c.post_id "
                   "GROUP BY p.id ORDER BY s DESC LIMIT 2",
            .columns = multi_columns,
            .column_count = 4U,
            .values = multi_order_values,
            .row_count = 2U,
            .context = "left join grouped order by aggregate alias",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT c.post_id, COUNT(*) FROM posts AS p JOIN comments AS c "
                   "ON p.id = c.post_id GROUP BY c.post_id ORDER BY c.post_id",
            .columns = inner_count_columns,
            .column_count = 2U,
            .values = inner_count_values,
            .row_count = 2U,
            .context = "inner join grouped by right source",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.category, COUNT(c.id) FROM posts p LEFT JOIN comments c "
                   "ON p.id = c.post_id WHERE p.id >= 2 GROUP BY p.category ORDER BY p.category",
            .columns = category_count_columns,
            .column_count = 2U,
            .values = category_count_values,
            .row_count = 3U,
            .context = "joined grouped where and nullable group key order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.id AS post_id, COUNT(c.id) AS c FROM posts p LEFT JOIN comments c "
                   "ON p.id = c.post_id GROUP BY p.id HAVING c > 0 ORDER BY post_id DESC LIMIT 2",
            .columns = having_columns,
            .column_count = 2U,
            .values = having_values,
            .row_count = 2U,
            .context = "joined grouped having order alias limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.id, COUNT(c.id) FROM posts p LEFT JOIN comments c "
                   "ON p.id = c.post_id GROUP BY p.id ORDER BY p.id LIMIT 0",
            .columns = count_column_columns,
            .column_count = 2U,
            .row_count = 0U,
            .context = "joined grouped limit zero",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.id, COUNT(c.id) FROM posts p LEFT JOIN comments c "
                   "ON p.id = c.post_id GROUP BY p.id ORDER BY p.id LIMIT 2 OFFSET 1",
            .columns = count_column_columns,
            .column_count = 2U,
            .values = offset_values,
            .row_count = 2U,
            .context = "joined grouped limit offset",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.category, BIT_OR(c.score) FROM posts p LEFT JOIN comments c "
                   "ON p.id = c.post_id GROUP BY p.category ORDER BY p.category",
            .columns = bit_or_columns,
            .column_count = 2U,
            .values = bit_or_values,
            .row_count = 3U,
            .context = "joined grouped bitwise aggregate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.category, BIT_AND(c.score) FROM posts p LEFT JOIN comments c "
                   "ON p.id = c.post_id GROUP BY p.category ORDER BY p.category",
            .columns = bit_and_columns,
            .column_count = 2U,
            .values = bit_and_values,
            .row_count = 3U,
            .context = "joined grouped bitwise and aggregate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.category, BIT_XOR(c.score) FROM posts p LEFT JOIN comments c "
                   "ON p.id = c.post_id GROUP BY p.category ORDER BY p.category",
            .columns = bit_xor_columns,
            .column_count = 2U,
            .values = bit_xor_values,
            .row_count = 3U,
            .context = "joined grouped bitwise xor aggregate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.id, c.id, COUNT(*), SUM(c.score) FROM posts p LEFT JOIN comments c "
                   "ON p.id = c.post_id GROUP BY p.id, c.id HAVING c.id IS NOT NULL "
                   "ORDER BY c.id",
            .columns = joined_multi_key_columns,
            .column_count = 4U,
            .values = joined_multi_key_values,
            .row_count = 3U,
            .context = "joined grouped by two descriptor keys",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT c.id FROM comments c INNER JOIN commentmeta cm "
                   "ON c.id = cm.comment_id WHERE cm.meta_key = 'featured' "
                   "GROUP BY c.id ORDER BY c.post_id ASC, c.id ASC",
            .columns = comment_meta_order_columns,
            .column_count = 1U,
            .values = comment_meta_descriptor_order_values,
            .row_count = 4U,
            .context = "joined grouped order by multiple descriptor columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT c.id FROM comments c INNER JOIN commentmeta cm "
                   "ON c.id = cm.comment_id WHERE cm.meta_key = 'featured' "
                   "GROUP BY c.id ORDER BY CAST(cm.meta_value AS CHAR) DESC, c.id DESC",
            .columns = comment_meta_order_columns,
            .column_count = 1U,
            .values = comment_meta_cast_order_values,
            .row_count = 4U,
            .context = "joined grouped order by cast expression and descriptor tiebreaker",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT c.id FROM comments c INNER JOIN commentmeta cm "
                   "ON c.id = cm.comment_id INNER JOIN commentmeta mt1 "
                   "ON c.id = mt1.comment_id WHERE cm.meta_key = 'featured' "
                   "AND mt1.meta_key = 'secondary' GROUP BY c.id "
                   "ORDER BY CAST(cm.meta_value AS CHAR) ASC, "
                   "CAST(mt1.meta_value AS CHAR) DESC, c.id DESC",
            .columns = comment_meta_order_columns,
            .column_count = 1U,
            .values = comment_meta_two_cast_order_values,
            .row_count = 4U,
            .context = "joined grouped order by two cast expressions and descriptor tiebreaker",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = DEFAULT", NULL);
    failures += execute_ok(database, "CREATE TABLE users(ID INT NOT NULL)", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE usermeta("
        "user_id INT NOT NULL, meta_key VARCHAR(64) NOT NULL, meta_value TEXT)",
        NULL
    );
    failures += execute_ok(database, "INSERT INTO users VALUES (1), (2), (3), (4), (5)", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO usermeta VALUES "
        "(1, 'wp_capabilities', 'a:1:{s:13:\"administrator\";b:1;}'), "
        "(2, 'wp_capabilities', 'a:1:{s:6:\"editor\";b:1;}'), "
        "(3, 'wp_capabilities', 'a:1:{s:10:\"subscriber\";b:1;}'), "
        "(4, 'wp_capabilities', 'a:0:{}'), "
        "(5, 'other_meta', 'a:1:{s:13:\"administrator\";b:1;}')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "COUNT(NULLIF(`meta_value` LIKE '%\\\"administrator\\\"%', false)) AS "
                   "administrators, "
                   "COUNT(NULLIF(`meta_value` LIKE '%\\\"editor\\\"%', false)) AS editors, "
                   "COUNT(NULLIF(`meta_value` LIKE '%\\\"author\\\"%', false)) AS authors, "
                   "COUNT(NULLIF(`meta_value` LIKE '%\\\"contributor\\\"%', false)) AS "
                   "contributors, "
                   "COUNT(NULLIF(`meta_value` LIKE '%\\\"subscriber\\\"%', false)) AS subscribers, "
                   "COUNT(NULLIF(`meta_value` = 'a:0:{}', false)) AS empty_roles, "
                   "COUNT(*) AS total_roles "
                   "FROM usermeta INNER JOIN users ON user_id = ID "
                   "WHERE meta_key = 'wp_capabilities'",
            .columns = role_count_columns,
            .column_count = role_count_column_count,
            .values = role_count_values,
            .row_count = 1U,
            .context = "joined count nullif predicate role counts",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT COUNT(*) FROM usermeta INNER JOIN users ON user_id = ID "
                   "WHERE meta_key = 'wp_capabilities'",
            .columns = distinct_join_count_columns,
            .column_count = 1U,
            .values = distinct_join_count_values,
            .row_count = 1U,
            .context = "joined distinct count star",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM usermeta INNER JOIN users ON user_id = ID "
                   "WHERE meta_key = 'wp_capabilities' LIMIT 2000",
            .columns = distinct_join_count_columns,
            .column_count = 1U,
            .values = limited_join_count_values,
            .row_count = 1U,
            .context = "joined count star with limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM usermeta INNER JOIN users ON user_id = ID "
                   "WHERE meta_key = 'wp_capabilities' LIMIT 0",
            .columns = distinct_join_count_columns,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "joined count star with zero limit",
        }
    );
    failures += expect_row_count(database, "-1", "row count after joined aggregate select");

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "joined aggregate preamble unchanged"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen joined aggregate file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.id, COUNT(c.id) FROM posts p LEFT JOIN comments c "
                   "ON p.id = c.post_id GROUP BY p.id ORDER BY p.id",
            .columns = count_column_columns,
            .column_count = 2U,
            .values = count_column_values,
            .row_count = 4U,
            .context = "joined aggregate persists after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_joined_aggregate_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += seed_joined_aggregate_schema(database);

    failures += execute_error(
        database,
        "SELECT id, COUNT(*) FROM posts p JOIN comments c ON p.id = c.post_id GROUP BY id",
        (struct expected_sql_error){
            .code = mysql_error_column_ambiguous,
            .sqlstate = "23000",
            .message_part = "Column 'id' in field list is ambiguous",
        }
    );
    failures += execute_error(
        database,
        "SELECT p.id AS selected_id, COUNT(*) FROM posts p JOIN comments c "
        "ON p.id = c.post_id GROUP BY id",
        (struct expected_sql_error){
            .code = mysql_error_column_ambiguous,
            .sqlstate = "23000",
            .message_part = "Column 'id' in group statement is ambiguous",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 FROM posts p JOIN comments c ON p.id = c.post_id GROUP BY id",
        (struct expected_sql_error){
            .code = mysql_error_column_ambiguous,
            .sqlstate = "23000",
            .message_part = "Column 'id' in group statement is ambiguous",
        }
    );
    failures += execute_error(
        database,
        "SELECT p.id, c.id, COUNT(*) FROM posts p JOIN comments c ON p.id = c.post_id "
        "GROUP BY p.id, c.id HAVING id = 1",
        (struct expected_sql_error){
            .code = mysql_error_column_ambiguous,
            .sqlstate = "23000",
            .message_part = "Column 'id' in having clause is ambiguous",
        }
    );
    failures += execute_error(
        database,
        "SELECT p.id, c.id, COUNT(*) FROM posts p JOIN comments c ON p.id = c.post_id "
        "GROUP BY p.id, c.id ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_column_ambiguous,
            .sqlstate = "23000",
            .message_part = "Column 'id' in order clause is ambiguous",
        }
    );
    failures += execute_error(
        database,
        "SELECT p.id, COUNT(c.missing) FROM posts p LEFT JOIN comments c "
        "ON p.id = c.post_id GROUP BY p.id",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'c.missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT p.id, COUNT(c.id) FROM posts p LEFT JOIN comments c "
        "ON p.id = c.post_id GROUP BY p.id HAVING missing > 0",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'having clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT p.id, COUNT(c.id) FROM posts p LEFT JOIN comments c "
        "ON p.id = c.post_id GROUP BY p.id ORDER BY missing",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT p.id, COUNT(*) FROM missing p LEFT JOIN also_missing c GROUP BY p.id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT p.id, COUNT(DISTINCT c.id) FROM posts p LEFT JOIN comments c "
        "ON p.id = c.post_id GROUP BY p.id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "GROUP BY supports selected descriptor group columns followed by aggregate results",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_independent_joined_aggregate_handles(void) {
    static const char *const first_columns[] = {"id", "COUNT(c.id)"};
    static const char *const first_values[] = {"1", "2", "2", "1", "3", "0", "4", "0"};
    static const char *const second_columns[] = {"id", "COUNT(c.id)"};
    static const char *const second_values[] = {"1", "0", "2", "1"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += seed_joined_aggregate_schema(first);
    failures += execute_ok(second, "CREATE DATABASE app", NULL);
    failures += execute_ok(second, "USE app", NULL);
    failures += execute_ok(second, "CREATE TABLE posts(id INT NOT NULL, category INT NULL)", NULL);
    failures += execute_ok(
        second,
        "CREATE TABLE comments(id INT NOT NULL, post_id INT NULL, score INT NULL)",
        NULL
    );
    failures += execute_ok(second, "INSERT INTO posts VALUES (1, 10), (2, 20)", NULL);
    failures += execute_ok(second, "INSERT INTO comments VALUES (201, 2, 4)", NULL);

    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT p.id, COUNT(c.id) FROM posts p LEFT JOIN comments c "
                   "ON p.id = c.post_id GROUP BY p.id ORDER BY p.id",
            .columns = first_columns,
            .column_count = 2U,
            .values = first_values,
            .row_count = 4U,
            .context = "first handle joined aggregate",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT p.id, COUNT(c.id) FROM posts p LEFT JOIN comments c "
                   "ON p.id = c.post_id GROUP BY p.id ORDER BY p.id",
            .columns = second_columns,
            .column_count = 2U,
            .values = second_values,
            .row_count = 2U,
            .context = "second handle joined aggregate",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);

    return failures;
}

static int seed_joined_aggregate_schema(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures +=
        execute_ok(database, "CREATE TABLE posts(id INT NOT NULL, category INT NULL)", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE comments(id INT NOT NULL, post_id INT NULL, score INT NULL)",
        NULL
    );
    failures += execute_ok(
        database,
        "CREATE TABLE commentmeta("
        "comment_id INT NOT NULL, meta_key VARCHAR(64) NOT NULL, meta_value TEXT)",
        NULL
    );
    failures +=
        execute_ok(database, "INSERT INTO posts VALUES (1, 10), (2, 10), (3, 20), (4, NULL)", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO comments VALUES "
        "(101, 1, 5), (102, 1, NULL), (103, 2, 7), "
        "(104, NULL, 9), (105, 99, 11)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO commentmeta VALUES "
        "(101, 'featured', 'a'), (102, 'featured', 'c'), "
        "(103, 'featured', 'b'), (105, 'featured', 'b'), "
        "(101, 'secondary', 'y'), (102, 'secondary', 'x'), "
        "(103, 'secondary', 'z'), (105, 'secondary', 'w'), "
        "(104, 'other', 'z')",
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
            "execute '%s': expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
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
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int expect_query(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    for (size_t column_index = 0U; column_index < query.column_count; ++column_index) {
        failures += expect_text(
            mylite_result_column_name(result, column_index),
            query.columns[column_index],
            query.context
        );
    }
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row_index = 0U; row_index < query.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < query.column_count; ++column_index) {
            size_t value_index = (row_index * query.column_count) + column_index;

            failures += expect_result_value(
                result,
                row_index,
                column_index,
                query.values[value_index],
                query.context
            );
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    mylite_result_free(result);

    return failures;
}

static int expect_row_count(mylite_db *database, const char *expected, const char *context) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SELECT ROW_COUNT()", &result);

    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures += expect_text(mylite_result_column_name(result, 0U), "ROW_COUNT()", context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_text(mylite_result_value_text(result, 0U, 0U), expected, context);
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

    if (expected == NULL) {
        return expect_true(actual == NULL, context);
    }
    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_joined_aggregate_select_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
        return 1;
    }

    return 0;
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
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        (void)fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
        return 1;
    }

    return 0;
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

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected '%s', got '%s'\n",
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
            "%s: expected '%s' to contain '%s'\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
        );
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
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
