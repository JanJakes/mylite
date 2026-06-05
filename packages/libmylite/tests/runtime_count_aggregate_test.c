#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "sqlite3.h"
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
    sqlite_sql_capacity = 512,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_unknown = 1105,
    mysql_error_table_does_not_exist = 1146,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_count_query {
    const char *sql;
    const char *column;
    const char *value;
    const char *context;
};

struct expected_count_rows_query {
    const char *sql;
    const char *column;
    const char *const *values;
    size_t value_count;
    const char *context;
};

static int test_count_aggregate_values_persistence_rename_and_truncate(void);
static int test_count_aggregate_diagnostics(void);
static int test_independent_count_aggregate_handles(void);
static int seed_count_schema(mylite_db *database);
static int create_count_table(mylite_db *database);
static int insert_count_rows(mylite_db *database);
static int expect_count_query(mylite_db *database, struct expected_count_query query);
static int expect_count_rows_query(mylite_db *database, struct expected_count_rows_query query);
static int expect_row_count(mylite_db *database, const char *expected, const char *context);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int drop_physical_table(sqlite3 *connection, const char *physical_name);
static int execute_sql(sqlite3 *connection, const char *sql);
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

    failures += test_count_aggregate_values_persistence_rename_and_truncate();
    failures += test_count_aggregate_diagnostics();
    failures += test_independent_count_aggregate_handles();

    return failures == 0 ? 0 : 1;
}

static int test_count_aggregate_values_persistence_rename_and_truncate(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");

    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*)",
            .column = "COUNT(*)",
            .value = "1",
            .context = "no-source count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT ALL COUNT(*)",
            .column = "COUNT(*)",
            .value = "1",
            .context = "no-source all count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(1)",
            .column = "COUNT(1)",
            .value = "1",
            .context = "no-source integer literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT ALL COUNT(1)",
            .column = "COUNT(1)",
            .value = "1",
            .context = "no-source all integer literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(999999999999999999999999999999999999999999999999999)",
            .column = "COUNT(999999999999999999999999999999999999999999999999999)",
            .value = "1",
            .context = "no-source large integer literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(NULL)",
            .column = "COUNT(NULL)",
            .value = "0",
            .context = "no-source null literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(TRUE)",
            .column = "COUNT(TRUE)",
            .value = "1",
            .context = "no-source true literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(false)",
            .column = "COUNT(false)",
            .value = "1",
            .context = "no-source false literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT count(*) FROM DUAL",
            .column = "count(*)",
            .value = "1",
            .context = "dual count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT ALL count(*) FROM DUAL",
            .column = "count(*)",
            .value = "1",
            .context = "dual all count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT Count( +1 ) FROM DUAL",
            .column = "Count( +1 )",
            .value = "1",
            .context = "dual positive literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT count(NULL) FROM DUAL",
            .column = "count(NULL)",
            .value = "0",
            .context = "dual null literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT Count( TRUE ) FROM DUAL",
            .column = "Count( TRUE )",
            .value = "1",
            .context = "dual true literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT count(FALSE) FROM DUAL",
            .column = "count(FALSE)",
            .value = "1",
            .context = "dual false literal count",
        }
    );
    failures += expect_row_count(database, "-1", "row count after count result set");

    failures += seed_count_schema(database);
    failures += create_count_table(database);
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers",
            .column = "COUNT(*)",
            .value = "0",
            .context = "empty table count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT ALL COUNT(*) FROM numbers",
            .column = "COUNT(*)",
            .value = "0",
            .context = "empty table all count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers AS nums",
            .column = "COUNT(*)",
            .value = "0",
            .context = "empty table aliased count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM numbers",
            .column = "COUNT(n)",
            .value = "0",
            .context = "empty table count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT n) FROM numbers",
            .column = "COUNT(DISTINCT n)",
            .value = "0",
            .context = "empty table count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT n) FROM numbers nums",
            .column = "COUNT(DISTINCT n)",
            .value = "0",
            .context = "empty table aliased distinct count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(1) FROM numbers",
            .column = "COUNT(1)",
            .value = "0",
            .context = "empty table integer literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(NULL) FROM numbers",
            .column = "COUNT(NULL)",
            .value = "0",
            .context = "empty table null literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(TRUE) FROM numbers",
            .column = "COUNT(TRUE)",
            .value = "0",
            .context = "empty table true literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(FALSE) FROM numbers",
            .column = "COUNT(FALSE)",
            .value = "0",
            .context = "empty table false literal count",
        }
    );
    failures += execute_ok(database, "CREATE TABLE all_nulls (id INT NOT NULL, n INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO all_nulls VALUES (1, NULL), (2, NULL)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM all_nulls",
            .column = "COUNT(n)",
            .value = "0",
            .context = "all-null count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT n) FROM all_nulls",
            .column = "COUNT(DISTINCT n)",
            .value = "0",
            .context = "all-null count distinct column",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE quoted_counts (`weird name` INT, `double\"quote` INT)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO quoted_counts VALUES (1, 1), (NULL, 2), (3, NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(`weird name`) FROM quoted_counts",
            .column = "COUNT(`weird name`)",
            .value = "2",
            .context = "quoted space count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(`double\"quote`) FROM quoted_counts",
            .column = "COUNT(`double\"quote`)",
            .value = "2",
            .context = "quoted double quote count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT `weird name`) FROM quoted_counts",
            .column = "COUNT(DISTINCT `weird name`)",
            .value = "2",
            .context = "quoted space count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT `double\"quote`) FROM quoted_counts",
            .column = "COUNT(DISTINCT `double\"quote`)",
            .value = "2",
            .context = "quoted double quote count distinct column",
        }
    );
    failures += insert_count_rows(database);
    failures += execute_ok(
        database,
        "CREATE TABLE posts (ID INT, post_status VARCHAR(20), post_type VARCHAR(20))",
        NULL
    );
    failures += execute_ok(
        database,
        "CREATE TABLE term_relationships (object_id INT, term_taxonomy_id INT)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO posts VALUES "
        "(1, 'publish', 'post'), "
        "(2, 'draft', 'post'), "
        "(3, 'publish', 'page'), "
        "(4, 'publish', 'post')",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO term_relationships VALUES "
        "(1, 1), (1, 2), (2, 1), (3, 1), (4, 1), (99, 1)",
        NULL
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) AS c FROM term_relationships, posts "
                   "WHERE posts.ID = term_relationships.object_id "
                   "AND post_status IN ('publish') "
                   "AND post_type IN ('post') "
                   "AND term_taxonomy_id = 1",
            .column = "c",
            .value = "2",
            .context = "comma joined count star with WordPress-style filters",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM posts AS p JOIN term_relationships AS tr "
                   "ON p.ID = tr.object_id WHERE tr.term_taxonomy_id = 1",
            .column = "COUNT(*)",
            .value = "4",
            .context = "explicit inner joined count star",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) AS c FROM posts AS p "
                   "LEFT JOIN term_relationships AS tr ON p.ID = tr.object_id "
                   "WHERE p.post_status = 'publish'",
            .column = "c",
            .value = "4",
            .context = "left joined count star",
        }
    );
    failures += execute_ok(database, "ALTER TABLE numbers ALTER COLUMN n SET INVISIBLE", &result);
    mylite_result_free(result);
    result = NULL;
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after inserts"
    );

    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers",
            .column = "COUNT(*)",
            .value = "4",
            .context = "nonempty table count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT count(*) FROM numbers",
            .column = "count(*)",
            .value = "4",
            .context = "lowercase label count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT Count( * ) FROM numbers",
            .column = "Count( * )",
            .value = "4",
            .context = "spaced argument label count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(/* inside */*) FROM numbers",
            .column = "COUNT(/* inside */ *)",
            .value = "4",
            .context = "commented argument label count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT (COUNT(*)) FROM numbers",
            .column = "(COUNT(*))",
            .value = "4",
            .context = "parenthesized label count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(1) FROM numbers",
            .column = "COUNT(1)",
            .value = "4",
            .context = "integer literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(1) FROM numbers AS nums",
            .column = "COUNT(1)",
            .value = "4",
            .context = "aliased integer literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(0) FROM numbers",
            .column = "COUNT(0)",
            .value = "4",
            .context = "zero literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(-1) FROM numbers",
            .column = "COUNT(-1)",
            .value = "4",
            .context = "negative literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(+1) FROM numbers",
            .column = "COUNT(+1)",
            .value = "4",
            .context = "positive literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(NULL) FROM numbers",
            .column = "COUNT(NULL)",
            .value = "0",
            .context = "null literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(NULL) FROM numbers AS nums",
            .column = "COUNT(NULL)",
            .value = "0",
            .context = "aliased null literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(TRUE) FROM numbers",
            .column = "COUNT(TRUE)",
            .value = "4",
            .context = "true literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(TRUE) FROM numbers AS nums",
            .column = "COUNT(TRUE)",
            .value = "4",
            .context = "aliased true literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(FALSE) FROM numbers",
            .column = "COUNT(FALSE)",
            .value = "4",
            .context = "false literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(FALSE) FROM numbers AS nums",
            .column = "COUNT(FALSE)",
            .value = "4",
            .context = "aliased false literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(/* inside */1) FROM numbers",
            .column = "COUNT(/* inside */ 1)",
            .value = "4",
            .context = "commented integer literal label count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(/* inside */NULL) FROM numbers",
            .column = "COUNT(/* inside */ NULL)",
            .value = "0",
            .context = "commented null literal label count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(/* inside */TRUE) FROM numbers",
            .column = "COUNT(/* inside */ TRUE)",
            .value = "4",
            .context = "commented true literal label count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(/* inside */FALSE) FROM numbers",
            .column = "COUNT(/* inside */ FALSE)",
            .value = "4",
            .context = "commented false literal label count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT (COUNT(1)) FROM numbers",
            .column = "(COUNT(1))",
            .value = "4",
            .context = "parenthesized literal label count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(id) FROM numbers",
            .column = "COUNT(id)",
            .value = "4",
            .context = "not-null count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(i) FROM numbers",
            .column = "COUNT(i)",
            .value = "4",
            .context = "integer count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(iu) FROM numbers",
            .column = "COUNT(iu)",
            .value = "3",
            .context = "unsigned integer count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(b) FROM numbers",
            .column = "COUNT(b)",
            .value = "3",
            .context = "bigint count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(bu) FROM numbers",
            .column = "COUNT(bu)",
            .value = "3",
            .context = "unsigned bigint count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM numbers",
            .column = "COUNT(n)",
            .value = "3",
            .context = "explicit invisible nullable count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT ALL COUNT(n) FROM numbers",
            .column = "COUNT(n)",
            .value = "3",
            .context = "all explicit invisible nullable count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM numbers AS nums",
            .column = "COUNT(n)",
            .value = "3",
            .context = "aliased nullable count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(nn) FROM numbers",
            .column = "COUNT(nn)",
            .value = "4",
            .context = "not-null integer count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(ti) FROM numbers",
            .column = "COUNT(ti)",
            .value = "3",
            .context = "tinyint count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(ti1) FROM numbers",
            .column = "COUNT(ti1)",
            .value = "3",
            .context = "tinyint width one count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(si) FROM numbers",
            .column = "COUNT(si)",
            .value = "3",
            .context = "smallint count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(mi) FROM numbers",
            .column = "COUNT(mi)",
            .value = "3",
            .context = "mediumint count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(bool_col) FROM numbers",
            .column = "COUNT(bool_col)",
            .value = "3",
            .context = "bool alias count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(boolean_col) FROM numbers",
            .column = "COUNT(boolean_col)",
            .value = "3",
            .context = "boolean alias count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT count(n) FROM numbers",
            .column = "count(n)",
            .value = "3",
            .context = "lowercase label count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT Count( n ) FROM numbers",
            .column = "Count( n )",
            .value = "3",
            .context = "spaced label count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(/* inside */n) FROM numbers",
            .column = "COUNT(/* inside */ n)",
            .value = "3",
            .context = "commented label count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT (COUNT(n)) FROM numbers",
            .column = "(COUNT(n))",
            .value = "3",
            .context = "parenthesized label count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(N) FROM numbers",
            .column = "COUNT(N)",
            .value = "3",
            .context = "case-insensitive count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT id) FROM numbers",
            .column = "COUNT(DISTINCT id)",
            .value = "4",
            .context = "not-null count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT i) FROM numbers",
            .column = "COUNT(DISTINCT i)",
            .value = "4",
            .context = "integer count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT iu) FROM numbers",
            .column = "COUNT(DISTINCT iu)",
            .value = "3",
            .context = "unsigned integer count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT b) FROM numbers",
            .column = "COUNT(DISTINCT b)",
            .value = "3",
            .context = "bigint count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT bu) FROM numbers",
            .column = "COUNT(DISTINCT bu)",
            .value = "3",
            .context = "unsigned bigint count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT n) FROM numbers",
            .column = "COUNT(DISTINCT n)",
            .value = "2",
            .context = "nullable count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT n) FROM numbers AS nums",
            .column = "COUNT(DISTINCT n)",
            .value = "2",
            .context = "aliased nullable count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT ALL COUNT(DISTINCT n) FROM numbers",
            .column = "COUNT(DISTINCT n)",
            .value = "2",
            .context = "all nullable count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT nn) FROM numbers",
            .column = "COUNT(DISTINCT nn)",
            .value = "4",
            .context = "not-null integer count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT ti) FROM numbers",
            .column = "COUNT(DISTINCT ti)",
            .value = "3",
            .context = "tinyint count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT ti1) FROM numbers",
            .column = "COUNT(DISTINCT ti1)",
            .value = "3",
            .context = "tinyint width one count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT si) FROM numbers",
            .column = "COUNT(DISTINCT si)",
            .value = "3",
            .context = "smallint count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT mi) FROM numbers",
            .column = "COUNT(DISTINCT mi)",
            .value = "3",
            .context = "mediumint count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT bool_col) FROM numbers",
            .column = "COUNT(DISTINCT bool_col)",
            .value = "2",
            .context = "bool alias count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT boolean_col) FROM numbers",
            .column = "COUNT(DISTINCT boolean_col)",
            .value = "2",
            .context = "boolean alias count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT count(distinct n) FROM numbers",
            .column = "count(distinct n)",
            .value = "2",
            .context = "lowercase count distinct label",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT Count( DISTINCT n ) FROM numbers",
            .column = "Count( DISTINCT n )",
            .value = "2",
            .context = "spaced count distinct label",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT /* inside */n) FROM numbers",
            .column = "COUNT(DISTINCT /* inside */ n)",
            .value = "2",
            .context = "commented count distinct argument label",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT/* inside */n) FROM numbers",
            .column = "COUNT(DISTINCT/* inside */ n)",
            .value = "2",
            .context = "commented count distinct adjacency label",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(/* inside */DISTINCT n) FROM numbers",
            .column = "COUNT(/* inside */ DISTINCT n)",
            .value = "2",
            .context = "commented count distinct keyword label",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT (COUNT(DISTINCT n)) FROM numbers",
            .column = "(COUNT(DISTINCT n))",
            .value = "2",
            .context = "parenthesized count distinct label",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT N) FROM numbers",
            .column = "COUNT(DISTINCT N)",
            .value = "2",
            .context = "case-insensitive count distinct column",
        }
    );

    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM app.numbers WHERE id = 1",
            .column = "COUNT(*)",
            .value = "1",
            .context = "schema-qualified target count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM app.numbers AS nums WHERE id = 1",
            .column = "COUNT(*)",
            .value = "1",
            .context = "schema-qualified aliased target count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM app.numbers WHERE id = 1",
            .column = "COUNT(n)",
            .value = "0",
            .context = "schema-qualified target count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT n) FROM app.numbers WHERE id = 1",
            .column = "COUNT(DISTINCT n)",
            .value = "0",
            .context = "schema-qualified target count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(1) FROM app.numbers WHERE id = 1",
            .column = "COUNT(1)",
            .value = "1",
            .context = "schema-qualified target literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(NULL) FROM app.numbers WHERE id = 1",
            .column = "COUNT(NULL)",
            .value = "0",
            .context = "schema-qualified target null literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(TRUE) FROM app.numbers WHERE id = 1",
            .column = "COUNT(TRUE)",
            .value = "1",
            .context = "schema-qualified target true literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE id <> 1",
            .column = "COUNT(*)",
            .value = "3",
            .context = "not equal angle count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE id != 1",
            .column = "COUNT(*)",
            .value = "3",
            .context = "not equal bang count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE id < 3",
            .column = "COUNT(*)",
            .value = "2",
            .context = "less than count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE id <= 3",
            .column = "COUNT(*)",
            .value = "3",
            .context = "less equal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE id > 2",
            .column = "COUNT(*)",
            .value = "2",
            .context = "greater than count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE id >= 2",
            .column = "COUNT(*)",
            .value = "3",
            .context = "greater equal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE id <=> 2",
            .column = "COUNT(*)",
            .value = "1",
            .context = "null safe equal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE n = 20",
            .column = "COUNT(*)",
            .value = "2",
            .context = "nullable equal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE n <> 20",
            .column = "COUNT(*)",
            .value = "1",
            .context = "nullable not equal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE n <=> 20",
            .column = "COUNT(*)",
            .value = "2",
            .context = "nullable null safe count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE n IS NULL",
            .column = "COUNT(*)",
            .value = "1",
            .context = "is null count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE n IS NOT NULL",
            .column = "COUNT(*)",
            .value = "3",
            .context = "is not null count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(1) FROM numbers WHERE n IS NULL",
            .column = "COUNT(1)",
            .value = "1",
            .context = "count literal where nullable is null",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(NULL) FROM numbers WHERE n IS NULL",
            .column = "COUNT(NULL)",
            .value = "0",
            .context = "count null literal where nullable is null",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(TRUE) FROM numbers WHERE n IS NULL",
            .column = "COUNT(TRUE)",
            .value = "1",
            .context = "count true literal where nullable is null",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(FALSE) FROM numbers WHERE n IS NULL",
            .column = "COUNT(FALSE)",
            .value = "1",
            .context = "count false literal where nullable is null",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(1) FROM numbers WHERE n IS NOT NULL",
            .column = "COUNT(1)",
            .value = "3",
            .context = "count literal where nullable is not null",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(NULL) FROM numbers WHERE n IS NOT NULL",
            .column = "COUNT(NULL)",
            .value = "0",
            .context = "count null literal where nullable is not null",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(TRUE) FROM numbers WHERE n IS NOT NULL",
            .column = "COUNT(TRUE)",
            .value = "3",
            .context = "count true literal where nullable is not null",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(FALSE) FROM numbers WHERE n IS NOT NULL",
            .column = "COUNT(FALSE)",
            .value = "3",
            .context = "count false literal where nullable is not null",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(1) FROM numbers WHERE id = 2",
            .column = "COUNT(1)",
            .value = "1",
            .context = "count literal where equal",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(NULL) FROM numbers WHERE id = 2",
            .column = "COUNT(NULL)",
            .value = "0",
            .context = "count null literal where equal",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(TRUE) FROM numbers WHERE id = 2",
            .column = "COUNT(TRUE)",
            .value = "1",
            .context = "count true literal where equal",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(FALSE) FROM numbers WHERE id = 2",
            .column = "COUNT(FALSE)",
            .value = "1",
            .context = "count false literal where equal",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(1) FROM numbers WHERE id > 99",
            .column = "COUNT(1)",
            .value = "0",
            .context = "count literal where no match",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(NULL) FROM numbers WHERE id > 99",
            .column = "COUNT(NULL)",
            .value = "0",
            .context = "count null literal where no match",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(TRUE) FROM numbers WHERE id > 99",
            .column = "COUNT(TRUE)",
            .value = "0",
            .context = "count true literal where no match",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(FALSE) FROM numbers WHERE id > 99",
            .column = "COUNT(FALSE)",
            .value = "0",
            .context = "count false literal where no match",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE iu = 4294967295",
            .column = "COUNT(*)",
            .value = "1",
            .context = "unsigned int boundary count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE b = -9223372036854775808",
            .column = "COUNT(*)",
            .value = "1",
            .context = "signed bigint minimum count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE bu = 9223372036854775807",
            .column = "COUNT(*)",
            .value = "1",
            .context = "unsigned bigint physical maximum count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(id) FROM numbers WHERE n IS NULL",
            .column = "COUNT(id)",
            .value = "1",
            .context = "count non-null id where nullable is null",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM numbers WHERE n IS NULL",
            .column = "COUNT(n)",
            .value = "0",
            .context = "count nullable where nullable is null",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM numbers WHERE n IS NOT NULL",
            .column = "COUNT(n)",
            .value = "3",
            .context = "count nullable where nullable is not null",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM numbers WHERE id = 1",
            .column = "COUNT(n)",
            .value = "0",
            .context = "count column where equal null row",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM numbers WHERE id <> 1",
            .column = "COUNT(n)",
            .value = "3",
            .context = "count column where not equal angle",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM numbers WHERE id != 1",
            .column = "COUNT(n)",
            .value = "3",
            .context = "count column where not equal bang",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM numbers WHERE id < 3",
            .column = "COUNT(n)",
            .value = "1",
            .context = "count column where less than",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM numbers WHERE id <= 3",
            .column = "COUNT(n)",
            .value = "2",
            .context = "count column where less equal",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM numbers WHERE id > 2",
            .column = "COUNT(n)",
            .value = "2",
            .context = "count column where greater than",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM numbers WHERE id >= 2",
            .column = "COUNT(n)",
            .value = "3",
            .context = "count column where greater equal",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM numbers WHERE id <=> 2",
            .column = "COUNT(n)",
            .value = "1",
            .context = "count column where null safe equal",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM numbers WHERE iu = 4294967295",
            .column = "COUNT(n)",
            .value = "1",
            .context = "count column where unsigned int boundary",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM numbers WHERE b = -9223372036854775808",
            .column = "COUNT(n)",
            .value = "0",
            .context = "count column where signed bigint minimum",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM numbers WHERE bu = 9223372036854775807",
            .column = "COUNT(n)",
            .value = "1",
            .context = "count column where unsigned bigint physical maximum",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT n) FROM numbers WHERE n IS NULL",
            .column = "COUNT(DISTINCT n)",
            .value = "0",
            .context = "count distinct nullable is null",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT n) FROM numbers WHERE n IS NOT NULL",
            .column = "COUNT(DISTINCT n)",
            .value = "2",
            .context = "count distinct nullable is not null",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT n) FROM numbers WHERE id = 2",
            .column = "COUNT(DISTINCT n)",
            .value = "1",
            .context = "count distinct where equal",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT n) FROM numbers WHERE id > 99",
            .column = "COUNT(DISTINCT n)",
            .value = "0",
            .context = "count distinct where no match",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT n) FROM numbers WHERE n = 20",
            .column = "COUNT(DISTINCT n)",
            .value = "1",
            .context = "count distinct where nullable equal",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT n) FROM numbers WHERE n <> 20",
            .column = "COUNT(DISTINCT n)",
            .value = "1",
            .context = "count distinct where nullable not equal",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT n) FROM numbers WHERE iu = 4294967295",
            .column = "COUNT(DISTINCT n)",
            .value = "1",
            .context = "count distinct where unsigned int boundary",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT n) FROM numbers WHERE b = -9223372036854775808",
            .column = "COUNT(DISTINCT n)",
            .value = "0",
            .context = "count distinct where signed bigint minimum",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT n) FROM numbers WHERE bu = 9223372036854775807",
            .column = "COUNT(DISTINCT n)",
            .value = "1",
            .context = "count distinct where unsigned bigint physical maximum",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE n IS NULL",
            .column = "COUNT(*)",
            .value = "1",
            .context = "reopened nullable count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM numbers WHERE n IS NOT NULL",
            .column = "COUNT(n)",
            .value = "3",
            .context = "reopened count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT n) FROM numbers WHERE n IS NOT NULL",
            .column = "COUNT(DISTINCT n)",
            .value = "2",
            .context = "reopened count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(1) FROM numbers WHERE n IS NOT NULL",
            .column = "COUNT(1)",
            .value = "3",
            .context = "reopened literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(TRUE) FROM numbers WHERE n IS NOT NULL",
            .column = "COUNT(TRUE)",
            .value = "3",
            .context = "reopened true literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(FALSE) FROM numbers WHERE n IS NULL",
            .column = "COUNT(FALSE)",
            .value = "1",
            .context = "reopened false literal count",
        }
    );
    failures += execute_ok(database, "RENAME TABLE numbers TO counted_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.numbers' doesn't exist",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM counted_numbers",
            .column = "COUNT(*)",
            .value = "4",
            .context = "renamed table count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM counted_numbers",
            .column = "COUNT(n)",
            .value = "3",
            .context = "renamed table count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT n) FROM counted_numbers",
            .column = "COUNT(DISTINCT n)",
            .value = "2",
            .context = "renamed table count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(1) FROM counted_numbers",
            .column = "COUNT(1)",
            .value = "4",
            .context = "renamed table literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(TRUE) FROM counted_numbers",
            .column = "COUNT(TRUE)",
            .value = "4",
            .context = "renamed table true literal count",
        }
    );
    failures += execute_ok(database, "TRUNCATE TABLE counted_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM counted_numbers",
            .column = "COUNT(*)",
            .value = "0",
            .context = "truncated table count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(n) FROM counted_numbers",
            .column = "COUNT(n)",
            .value = "0",
            .context = "truncated table count column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT n) FROM counted_numbers",
            .column = "COUNT(DISTINCT n)",
            .value = "0",
            .context = "truncated table count distinct column",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(1) FROM counted_numbers",
            .column = "COUNT(1)",
            .value = "0",
            .context = "truncated table literal count",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(FALSE) FROM counted_numbers",
            .column = "COUNT(FALSE)",
            .value = "0",
            .context = "truncated table false literal count",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after count and truncate"
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_count_aggregate_diagnostics(void) {
    static const char *const grouped_counts[] = {"1", "1", "1", "1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM numbers AS nums",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(1) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(TRUE) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT n) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );

    failures += seed_count_schema(database);
    failures += create_count_table(database);
    failures += insert_count_rows(database);

    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM missing",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM missing AS nums",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(n) FROM missing",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(1) FROM missing",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(TRUE) FROM missing",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT n) FROM missing",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM missing_schema.numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM missing_schema.numbers AS nums",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(n) FROM missing_schema.numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(1) FROM missing_schema.numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(FALSE) FROM missing_schema.numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT n) FROM missing_schema.numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM _mylite_reserved.numbers",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(n) FROM _mylite_reserved.numbers",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(1) FROM _mylite_reserved.numbers",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(TRUE) FROM _mylite_reserved.numbers",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT n) FROM _mylite_reserved.numbers",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(n) FROM _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(1) FROM _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(FALSE) FROM _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT n) FROM _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM numbers WHERE missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(n) FROM numbers WHERE missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(1) FROM numbers WHERE missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(TRUE) FROM numbers WHERE missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT n) FROM numbers WHERE missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(missing) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT missing) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(n)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'n' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(n) FROM DUAL",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'n' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT n)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'n' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT n) FROM DUAL",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'n' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(1.0) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT('x') FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(id + 1) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(+TRUE) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(-FALSE) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(NOT TRUE) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(TRUE + 1) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(t.n) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 't.n' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT() FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(n, id) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(t.*) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT t.n) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 't.n' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT *) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT n, id) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT 1) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT TRUE) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT n + 1) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT (*) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT/**/(*) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*), COUNT(*) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "COUNT(*) supports exactly one aggregate select item",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(n), COUNT(id) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "COUNT(column) supports exactly one aggregate select item",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(1), COUNT(0) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "COUNT(literal) supports exactly one aggregate select item",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT n), COUNT(DISTINCT id) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "COUNT(DISTINCT column) supports exactly one aggregate select item",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*), id FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "COUNT(*) supports exactly one aggregate select item",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(n), id FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "COUNT(column) supports exactly one aggregate select item",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(1), id FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "COUNT(literal) supports exactly one aggregate select item",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT n), id FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "COUNT(DISTINCT column) supports exactly one aggregate select item",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM numbers ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "COUNT(*) supports only WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(n) FROM numbers ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "COUNT(column) supports only WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(1) FROM numbers ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "COUNT(literal) supports only WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT n) FROM numbers ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "COUNT(DISTINCT column) supports only WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM numbers LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "COUNT(*) supports only WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(n) FROM numbers LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "COUNT(column) supports only WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(1) FROM numbers LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "COUNT(literal) supports only WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT n) FROM numbers LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "COUNT(DISTINCT column) supports only WHERE",
        }
    );
    failures += expect_count_query(
        database,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) AS c FROM numbers",
            .column = "c",
            .value = "4",
            .context = "count star select item alias",
        }
    );
    failures += expect_count_rows_query(
        database,
        (struct expected_count_rows_query){
            .sql = "SELECT COUNT(*) AS c FROM numbers GROUP BY id ORDER BY id",
            .column = "c",
            .values = grouped_counts,
            .value_count = sizeof(grouped_counts) / sizeof(grouped_counts[0]),
            .context = "grouped aggregate without selected group column",
        }
    );
    failures += execute_error(
        database,
        "WITH cte AS (SELECT COUNT(*) FROM numbers) SELECT COUNT(*) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read diagnostics schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "numbers", &table),
        MYLITE_OK,
        "read diagnostics table"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += drop_physical_table(sqlite, table.physical_name);
    }
    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite row operation failed",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(n) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite row operation failed",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(1) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite row operation failed",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(NULL) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite row operation failed",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(TRUE) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite row operation failed",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT n) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite row operation failed",
        }
    );

    mylite_result_free(result);
    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_independent_count_aggregate_handles(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0) {
        return 1;
    }
    if (make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += seed_count_schema(first);
    failures += seed_count_schema(second);
    failures += execute_ok(first, "CREATE TABLE numbers (id INT NOT NULL)", NULL);
    failures += execute_ok(second, "CREATE TABLE numbers (id INT NOT NULL)", NULL);
    failures += execute_ok(first, "INSERT INTO numbers VALUES (1)", NULL);
    failures += execute_ok(second, "INSERT INTO numbers VALUES (1), (2)", NULL);
    failures += expect_count_query(
        first,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers",
            .column = "COUNT(*)",
            .value = "1",
            .context = "first independent count",
        }
    );
    failures += expect_count_query(
        first,
        (struct expected_count_query){
            .sql = "SELECT COUNT(id) FROM numbers",
            .column = "COUNT(id)",
            .value = "1",
            .context = "first independent count column",
        }
    );
    failures += expect_count_query(
        first,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT id) FROM numbers",
            .column = "COUNT(DISTINCT id)",
            .value = "1",
            .context = "first independent count distinct column",
        }
    );
    failures += expect_count_query(
        first,
        (struct expected_count_query){
            .sql = "SELECT COUNT(1) FROM numbers",
            .column = "COUNT(1)",
            .value = "1",
            .context = "first independent literal count",
        }
    );
    failures += expect_count_query(
        first,
        (struct expected_count_query){
            .sql = "SELECT COUNT(NULL) FROM numbers",
            .column = "COUNT(NULL)",
            .value = "0",
            .context = "first independent null literal count",
        }
    );
    failures += expect_count_query(
        first,
        (struct expected_count_query){
            .sql = "SELECT COUNT(TRUE) FROM numbers",
            .column = "COUNT(TRUE)",
            .value = "1",
            .context = "first independent true literal count",
        }
    );
    failures += expect_count_query(
        second,
        (struct expected_count_query){
            .sql = "SELECT COUNT(*) FROM numbers",
            .column = "COUNT(*)",
            .value = "2",
            .context = "second independent count",
        }
    );
    failures += expect_count_query(
        second,
        (struct expected_count_query){
            .sql = "SELECT COUNT(id) FROM numbers",
            .column = "COUNT(id)",
            .value = "2",
            .context = "second independent count column",
        }
    );
    failures += expect_count_query(
        second,
        (struct expected_count_query){
            .sql = "SELECT COUNT(DISTINCT id) FROM numbers",
            .column = "COUNT(DISTINCT id)",
            .value = "2",
            .context = "second independent count distinct column",
        }
    );
    failures += expect_count_query(
        second,
        (struct expected_count_query){
            .sql = "SELECT COUNT(1) FROM numbers",
            .column = "COUNT(1)",
            .value = "2",
            .context = "second independent literal count",
        }
    );
    failures += expect_count_query(
        second,
        (struct expected_count_query){
            .sql = "SELECT COUNT(NULL) FROM numbers",
            .column = "COUNT(NULL)",
            .value = "0",
            .context = "second independent null literal count",
        }
    );
    failures += expect_count_query(
        second,
        (struct expected_count_query){
            .sql = "SELECT COUNT(FALSE) FROM numbers",
            .column = "COUNT(FALSE)",
            .value = "2",
            .context = "second independent false literal count",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);

    return failures;
}

static int seed_count_schema(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "CREATE DATABASE app", &result);

    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);

    return failures;
}

static int create_count_table(mylite_db *database) {
    return execute_ok(
        database,
        "CREATE TABLE numbers ("
        "id INT NOT NULL, "
        "i INTEGER, "
        "iu INT UNSIGNED, "
        "b BIGINT, "
        "bu BIGINT UNSIGNED, "
        "n INT, "
        "nn INT NOT NULL, "
        "ti TINYINT, "
        "ti1 TINYINT(1), "
        "si SMALLINT, "
        "mi MEDIUMINT, "
        "bool_col BOOL, "
        "boolean_col BOOLEAN)",
        NULL
    );
}

static int insert_count_rows(mylite_db *database) {
    return execute_ok(
        database,
        "INSERT INTO numbers VALUES "
        "(1, -2147483648, 0, -9223372036854775808, 0, NULL, 10, "
        "-128, -1, -32768, -8388608, 1, 0), "
        "(2, 0, 2, -1, 2, 20, 20, 0, 0, 0, 0, NULL, 1), "
        "(3, 2147483647, 4294967295, 9223372036854775807, "
        "9223372036854775807, 20, 30, 127, 1, 32767, 8388607, 0, NULL), "
        "(4, 5, NULL, NULL, NULL, 30, 40, NULL, NULL, NULL, NULL, 1, 1)",
        NULL
    );
}

static int expect_count_query(mylite_db *database, struct expected_count_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), 1U, query.context);
    failures += expect_text(mylite_result_column_name(result, 0U), query.column, query.context);
    failures += expect_size(mylite_result_row_count(result), 1U, query.context);
    failures += expect_text(mylite_result_value_text(result, 0U, 0U), query.value, query.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    mylite_result_free(result);

    return failures;
}

static int expect_count_rows_query(mylite_db *database, struct expected_count_rows_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), 1U, query.context);
    failures += expect_text(mylite_result_column_name(result, 0U), query.column, query.context);
    failures += expect_size(mylite_result_row_count(result), query.value_count, query.context);
    for (size_t row_index = 0U; row_index < query.value_count; ++row_index) {
        failures += expect_text(
            mylite_result_value_text(result, row_index, 0U),
            query.values[row_index],
            query.context
        );
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
    if (mylite_errcode(database) != expected.code) {
        fprintf(
            stderr,
            "execute '%s': expected error code %d, got %d\n",
            sql,
            expected.code,
            mylite_errcode(database)
        );
        failures += 1;
    }
    if (strcmp(mylite_sqlstate(database), expected.sqlstate) != 0) {
        fprintf(
            stderr,
            "execute '%s': expected SQLSTATE '%s', got '%s'\n",
            sql,
            expected.sqlstate,
            mylite_sqlstate(database)
        );
        failures += 1;
    }
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
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
        "%s/mylite_count_aggregate_%d_%s.mylite",
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

static int drop_physical_table(sqlite3 *connection, const char *physical_name) {
    char sql[sqlite_sql_capacity];
    int written = snprintf(sql, sizeof(sql), "DROP TABLE \"%s\"", physical_name);

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "drop physical table SQL is too long\n");
        return 1;
    }

    return execute_sql(connection, sql);
}

static int execute_sql(sqlite3 *connection, const char *sql) {
    int rc = sqlite3_exec(connection, sql, NULL, NULL, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQLite exec failed for '%s': %d\n", sql, rc);
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
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }

    return 0;
}
