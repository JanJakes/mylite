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
    mysql_error_parse = 1064,
    mysql_error_unknown_table = 1051,
    mysql_error_duplicate_key = 1062,
    mysql_error_savepoint_does_not_exist = 1305,
    mysql_error_transaction_characteristics_changed = 1568,
    mysql_error_read_only_transaction = 1792,
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_nonquery {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_transaction_control_and_dml(void);
static int test_set_transaction_lifecycle(void);
static int test_savepoint_lifecycle(void);
static int test_independent_savepoint_handles(void);
static int test_independent_transaction_characteristic_handles(void);
static int test_drop_table_missing_implicitly_commits_transaction(void);
static int test_file_close_rolls_back_transaction(void);
static int seed_schema(mylite_db *database);
static int expect_nonquery(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_nonquery_with_warnings(
    mylite_db *database,
    const char *sql,
    struct expected_nonquery expected
);
static int expect_error(mylite_db *database, const char *sql, int expected_code);
static int expect_error_details(
    mylite_db *database,
    const char *sql,
    int expected_code,
    const char *expected_sqlstate,
    const char *expected_message
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_row_count_zero(mylite_db *database);
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
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_transaction_control_and_dml();
    failures += test_set_transaction_lifecycle();
    failures += test_savepoint_lifecycle();
    failures += test_independent_savepoint_handles();
    failures += test_independent_transaction_characteristic_handles();
    failures += test_drop_table_missing_implicitly_commits_transaction();
    failures += test_file_close_rolls_back_transaction();

    return failures == 0 ? 0 : 1;
}

static int test_transaction_control_and_dml(void) {
    static const char *const one_committed[] = {"1", "10"};
    static const char *const nested_rows[] = {"10", "100"};
    static const char *const ddl_rows[] = {"10", "100", "30", "300"};
    static const char *const unique_rows[] = {"1", "2", "3"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "control") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open control file");
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE t (id INT PRIMARY KEY, v INT)", 0);

    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_row_count_zero(database);
    failures += expect_nonquery(database, "ROLLBACK WORK", 0);
    failures += expect_row_count_zero(database);

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_row_count_zero(database);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (1, 10)", 1);
    failures += expect_nonquery(database, "COMMIT WORK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = one_committed,
            .column_count = 2U,
            .row_count = 1U,
            .context = "committed insert",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (2, 20)", 1);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = one_committed,
            .column_count = 2U,
            .row_count = 1U,
            .context = "rolled back insert",
        }
    );

    failures += expect_nonquery(database, "BEGIN WORK", 0);
    failures += expect_nonquery(database, "UPDATE t SET v = 11 WHERE id = 1", 1);
    failures += expect_nonquery(database, "ROLLBACK WORK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = one_committed,
            .column_count = 2U,
            .row_count = 1U,
            .context = "rolled back update",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "DELETE FROM t WHERE id = 1", 1);
    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY id",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "committed delete",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (10, 100)", 1);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (20, 200)", 1);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = nested_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "nested START TRANSACTION commits previous transaction",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (30, 300)", 1);
    failures += expect_nonquery(database, "CREATE TABLE ddl_commit (id INT)", 0);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = ddl_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "DDL implicitly commits active transaction",
        }
    );
    failures += expect_nonquery(database, "INSERT INTO ddl_commit VALUES (1)", 1);

    failures += expect_nonquery(database, "CREATE TABLE unique_t (id INT PRIMARY KEY)", 0);
    failures += expect_nonquery(database, "INSERT INTO unique_t VALUES (1)", 1);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO unique_t VALUES (2)", 1);
    failures +=
        expect_error(database, "INSERT INTO unique_t VALUES (1)", mysql_error_duplicate_key);
    failures += expect_nonquery(database, "INSERT INTO unique_t VALUES (3)", 1);
    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM unique_t ORDER BY id",
            .values = unique_rows,
            .column_count = 1U,
            .row_count = 3U,
            .context = "statement rollback inside active transaction",
        }
    );

    failures += expect_nonquery(database, "CREATE TABLE paths (id INT, v INT)", 0);
    failures += expect_nonquery(database, "CREATE TABLE src (id INT PRIMARY KEY, v INT)", 0);
    failures += expect_nonquery(database, "INSERT INTO src VALUES (3, 30), (5, 50)", 2);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO paths VALUES (1, 10)", 1);
    failures += expect_nonquery(database, "INSERT INTO paths SET id = 2, v = 20", 1);
    failures +=
        expect_nonquery(database, "INSERT INTO paths SELECT id, v FROM src WHERE id = 3", 1);
    failures += expect_nonquery(database, "REPLACE INTO paths VALUES (2, 22)", 1);
    failures += expect_nonquery(database, "REPLACE INTO paths SET id = 4, v = 40", 1);
    failures +=
        expect_nonquery(database, "REPLACE INTO paths SELECT id, v FROM src WHERE id = 5", 1);
    failures += expect_nonquery(database, "UPDATE paths SET v = 99 WHERE id = 1", 1);
    failures += expect_nonquery(database, "DELETE FROM paths WHERE id = 3", 1);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM paths ORDER BY id",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "all DML paths roll back",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_set_transaction_lifecycle(void) {
    static const char *const one_row[] = {"1", "10"};
    static const char *const two_rows[] = {"1", "10", "2", "20"};
    static const char *const persisted_rows[] = {"1", "10", "2", "20", "6", "60", "8", "80"};
    static const char *const temp_rows[] = {"1"};
    static const char *const ddl_rows[] = {"1"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "set_transaction") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open set transaction file");
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE t (id INT PRIMARY KEY, v INT)", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (1, 10)", 1);

    failures += expect_nonquery(database, "SET TRANSACTION ISOLATION LEVEL READ COMMITTED", 0);
    failures += expect_row_count_zero(database);
    failures += expect_nonquery(database, "SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED", 0);
    failures += expect_nonquery(database, "SET TRANSACTION ISOLATION LEVEL REPEATABLE READ", 0);
    failures += expect_nonquery(database, "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE", 0);
    failures += expect_nonquery(database, "SET TRANSACTION READ WRITE", 0);
    failures +=
        expect_nonquery(database, "SET TRANSACTION READ WRITE, ISOLATION LEVEL READ COMMITTED", 0);
    failures += expect_nonquery(
        database,
        "SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ, READ WRITE",
        0
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_error_details(
        database,
        "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE",
        mysql_error_transaction_characteristics_changed,
        "25001",
        "Transaction characteristics can't be changed while a transaction is in progress"
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);

    failures += expect_nonquery(database, "SET TRANSACTION READ ONLY", 0);
    failures += expect_error_details(
        database,
        "INSERT INTO t VALUES (2, 20)",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_error_details(
        database,
        "UPDATE t SET v = 11 WHERE id = 1",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = one_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "failed read-only writes do not mutate persistent table",
        }
    );

    failures += expect_nonquery(database, "SET TRANSACTION READ ONLY", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t",
            .values = temp_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "read-only select consumes next transaction characteristic",
        }
    );
    failures += expect_nonquery(database, "INSERT INTO t VALUES (2, 20)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = two_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "write after select uses session read-write default",
        }
    );

    failures += expect_nonquery(database, "SET TRANSACTION READ ONLY", 0);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_error_details(
        database,
        "DELETE FROM t WHERE id = 1",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);

    failures += expect_nonquery(database, "CREATE TEMPORARY TABLE tmp (id INT)", 0);
    failures += expect_nonquery(database, "SET TRANSACTION READ ONLY", 0);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO tmp VALUES (1)", 1);
    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM tmp ORDER BY id",
            .values = temp_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary DML is allowed in read-only transaction",
        }
    );

    failures += expect_nonquery(database, "SET SESSION TRANSACTION READ ONLY", 0);
    failures += expect_error_details(
        database,
        "INSERT INTO t VALUES (3, 30)",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_nonquery(database, "SET TRANSACTION READ WRITE", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (6, 60)", 1);
    failures += expect_error_details(
        database,
        "INSERT INTO t VALUES (7, 70)",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_nonquery(database, "SET SESSION TRANSACTION READ WRITE", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (8, 80)", 1);

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "SET SESSION TRANSACTION READ ONLY", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (9, 90)", 1);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_error_details(
        database,
        "INSERT INTO t VALUES (10, 100)",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_nonquery(database, "SET SESSION TRANSACTION READ WRITE", 0);

    failures += expect_nonquery(database, "SET TRANSACTION READ ONLY", 0);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "CREATE TABLE ddl_read_only_marker (id INT)", 0);
    failures += expect_nonquery(database, "INSERT INTO ddl_read_only_marker VALUES (1)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM ddl_read_only_marker",
            .values = ddl_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "DDL implicit commit clears active read-only state",
        }
    );

    failures += expect_error_details(
        database,
        "SET GLOBAL TRANSACTION READ WRITE",
        mysql_error_parse,
        "42000",
        "SET GLOBAL TRANSACTION is not supported"
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = persisted_rows,
            .column_count = 2U,
            .row_count = sizeof(persisted_rows) / (2U * sizeof(persisted_rows[0])),
            .context = "set transaction persistent rows before reopen",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen set transaction file");
    failures += expect_nonquery(database, "USE app", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = persisted_rows,
            .column_count = 2U,
            .row_count = sizeof(persisted_rows) / (2U * sizeof(persisted_rows[0])),
            .context = "set transaction rows persist after reopen",
        }
    );
    mylite_close(database);

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read set transaction preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "set transaction lifecycle preserves MyLite preamble"
    );

    remove_related_files(path);
    return failures;
}

static int test_savepoint_lifecycle(void) {
    static const char *const missing_warning[] = {
        "Error",
        "1305",
        "SAVEPOINT outside_sp does not exist",
    };
    static const char *const first_rollback_rows[] = {"1", "10"};
    static const char *const release_rows[] = {"1", "10", "4", "40", "5", "50"};
    static const char *const replacement_rows[] = {"1"};
    static const char *const case_rows[] = {"3"};
    static const char *const nested_start_rows[] = {"1"};
    static const char *const ddl_rows[] = {"1", "1"};
    static const char *const statement_error_rows[] = {"0"};
    static const char *const reopen_rows[] =
        {"1", "10", "4", "40", "5", "50", "8", "80", "17", "170"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "savepoint") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open savepoint file");
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE t (id INT PRIMARY KEY, v INT)", 0);

    failures += expect_nonquery(database, "SAVEPOINT outside_sp", 0);
    failures += expect_row_count_zero(database);
    failures += expect_error_details(
        database,
        "ROLLBACK TO SAVEPOINT outside_sp",
        mysql_error_savepoint_does_not_exist,
        "42000",
        "SAVEPOINT outside_sp does not exist"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = missing_warning,
            .column_count = 3U,
            .row_count = 1U,
            .context = "missing savepoint warning row",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (1, 10)", 1);
    failures += expect_nonquery(database, "SAVEPOINT a", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (2, 20)", 1);
    failures += expect_nonquery(database, "SAVEPOINT b", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (3, 30)", 1);
    failures += expect_nonquery(database, "ROLLBACK TO SAVEPOINT a", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = first_rollback_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "rollback to savepoint keeps target",
        }
    );
    failures += expect_nonquery(database, "ROLLBACK TO a", 0);
    failures += expect_error(database, "ROLLBACK TO b", mysql_error_savepoint_does_not_exist);
    failures += expect_nonquery(database, "RELEASE SAVEPOINT a", 0);
    failures += expect_error(database, "RELEASE SAVEPOINT a", mysql_error_savepoint_does_not_exist);
    failures += expect_nonquery(database, "COMMIT", 0);

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (4, 40)", 1);
    failures += expect_nonquery(database, "SAVEPOINT released", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (5, 50)", 1);
    failures += expect_nonquery(database, "RELEASE SAVEPOINT released", 0);
    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = release_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "release keeps row changes",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "SAVEPOINT dup_a", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (14, 140)", 1);
    failures += expect_nonquery(database, "SAVEPOINT dup_b", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (15, 150)", 1);
    failures += expect_nonquery(database, "SAVEPOINT dup_a", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (16, 160)", 1);
    failures += expect_nonquery(database, "ROLLBACK WORK TO SAVEPOINT dup_b", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE id IN (14, 15, 16)",
            .values = replacement_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "replacement preserves different later savepoint",
        }
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "SAVEPOINT dup", 0);
    failures += expect_nonquery(database, "SAVEPOINT dup", 0);
    failures += expect_nonquery(database, "RELEASE SAVEPOINT dup", 0);
    failures +=
        expect_error(database, "ROLLBACK TO SAVEPOINT dup", mysql_error_savepoint_does_not_exist);
    failures += expect_nonquery(database, "ROLLBACK", 0);

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "SAVEPOINT MixedName", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (6, 60)", 1);
    failures += expect_nonquery(database, "ROLLBACK TO mixedname", 0);
    failures += expect_nonquery(database, "SAVEPOINT `sp ace`", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (7, 70)", 1);
    failures += expect_nonquery(database, "ROLLBACK TO SAVEPOINT `SP ACE`", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t",
            .values = case_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "case-insensitive savepoint names",
        }
    );
    failures += expect_nonquery(database, "COMMIT", 0);

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (17, 170)", 1);
    failures += expect_nonquery(database, "SAVEPOINT nested_start_sp", 0);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures +=
        expect_error(database, "ROLLBACK TO nested_start_sp", mysql_error_savepoint_does_not_exist);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE id = 17",
            .values = nested_start_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "nested START TRANSACTION clears savepoint and commits previous work",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (8, 80)", 1);
    failures += expect_nonquery(database, "SAVEPOINT ddl_sp", 0);
    failures += expect_nonquery(database, "CREATE TABLE ddl_savepoint_marker (id INT)", 0);
    failures += expect_error(database, "ROLLBACK TO ddl_sp", mysql_error_savepoint_does_not_exist);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE id = 8",
            .values = &ddl_rows[0],
            .column_count = 1U,
            .row_count = 1U,
            .context = "DDL savepoint row persisted",
        }
    );
    failures += expect_nonquery(database, "INSERT INTO ddl_savepoint_marker VALUES (1)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM ddl_savepoint_marker",
            .values = &ddl_rows[1],
            .column_count = 1U,
            .row_count = 1U,
            .context = "DDL savepoint table persisted",
        }
    );

    failures += expect_nonquery(database, "CREATE TABLE unique_savepoint (id INT PRIMARY KEY)", 0);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "SAVEPOINT before_error", 0);
    failures += expect_nonquery(database, "INSERT INTO unique_savepoint VALUES (1)", 1);
    failures += expect_error(
        database,
        "INSERT INTO unique_savepoint VALUES (1)",
        mysql_error_duplicate_key
    );
    failures += expect_nonquery(database, "ROLLBACK TO before_error", 0);
    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM unique_savepoint",
            .values = statement_error_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "statement error preserves user savepoint",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen savepoint file");
    failures += expect_nonquery(database, "USE app", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = reopen_rows,
            .column_count = 2U,
            .row_count = sizeof(reopen_rows) / (2U * sizeof(reopen_rows[0])),
            .context = "committed savepoint-controlled changes persist after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_savepoint_handles(void) {
    static const char *const first_handle_rows[] = {"1"};
    static const char *const second_handle_rows[] = {"10"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "savepoint_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "savepoint_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += seed_schema(first);
    failures += seed_schema(second);
    failures += expect_nonquery(first, "CREATE TABLE t (id INT PRIMARY KEY)", 0);
    failures += expect_nonquery(second, "CREATE TABLE t (id INT PRIMARY KEY)", 0);

    failures += expect_nonquery(first, "START TRANSACTION", 0);
    failures += expect_nonquery(first, "INSERT INTO t VALUES (1)", 1);
    failures += expect_nonquery(first, "SAVEPOINT same_name", 0);
    failures += expect_nonquery(first, "INSERT INTO t VALUES (2)", 1);

    failures += expect_nonquery(second, "START TRANSACTION", 0);
    failures += expect_nonquery(second, "INSERT INTO t VALUES (10)", 1);
    failures += expect_nonquery(second, "SAVEPOINT same_name", 0);
    failures += expect_nonquery(second, "INSERT INTO t VALUES (20)", 1);
    failures += expect_nonquery(second, "ROLLBACK TO same_name", 0);
    failures += expect_nonquery(second, "COMMIT", 0);

    failures += expect_nonquery(first, "ROLLBACK TO same_name", 0);
    failures += expect_nonquery(first, "COMMIT", 0);

    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY id",
            .values = first_handle_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first handle savepoint state",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY id",
            .values = second_handle_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second handle savepoint state",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
    return failures;
}

static int test_independent_transaction_characteristic_handles(void) {
    static const char *const first_handle_rows[] = {"1"};
    static const char *const second_handle_rows[] = {"10", "20"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "transaction_characteristics_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "transaction_characteristics_second") !=
            0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += seed_schema(first);
    failures += seed_schema(second);
    failures += expect_nonquery(first, "CREATE TABLE t (id INT PRIMARY KEY)", 0);
    failures += expect_nonquery(second, "CREATE TABLE t (id INT PRIMARY KEY)", 0);
    failures += expect_nonquery(first, "INSERT INTO t VALUES (1)", 1);
    failures += expect_nonquery(second, "INSERT INTO t VALUES (10)", 1);

    failures += expect_nonquery(first, "SET SESSION TRANSACTION READ ONLY", 0);
    failures += expect_error_details(
        first,
        "INSERT INTO t VALUES (2)",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_nonquery(second, "INSERT INTO t VALUES (20)", 1);

    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY id",
            .values = first_handle_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first handle transaction characteristics",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY id",
            .values = second_handle_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "second handle transaction characteristics",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
    return failures;
}

static int test_drop_table_missing_implicitly_commits_transaction(void) {
    static const char *const first_insert_committed[] = {"1"};
    static const char *const second_insert_committed[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "drop_missing_commit") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open drop-missing file");
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE t (id INT PRIMARY KEY, v INT)", 0);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (40, 400)", 1);
    failures += expect_error(database, "DROP TABLE missing_drop", mysql_error_unknown_table);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE id = 40",
            .values = first_insert_committed,
            .column_count = 1U,
            .row_count = 1U,
            .context = "missing DROP TABLE implicitly commits before error",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (50, 500)", 1);
    failures += expect_nonquery_with_warnings(
        database,
        "DROP TABLE IF EXISTS missing_if_exists",
        (struct expected_nonquery){0, 1U}
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE id = 50",
            .values = second_insert_committed,
            .column_count = 1U,
            .row_count = 1U,
            .context = "missing DROP TABLE IF EXISTS implicitly commits before warning",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_file_close_rolls_back_transaction(void) {
    static const char *const committed_rows[] = {"1", "10"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "close_rollback") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE persisted (id INT PRIMARY KEY, v INT)", 0);
    failures += expect_nonquery(database, "INSERT INTO persisted VALUES (1, 10)", 1);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "SAVEPOINT close_sp", 0);
    failures += expect_nonquery(database, "INSERT INTO persisted VALUES (2, 20)", 1);
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen file");
    failures += expect_nonquery(database, "USE app", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM persisted ORDER BY id",
            .values = committed_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "close rolls back active transaction",
        }
    );
    mylite_close(database);

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "transaction lifecycle preserves MyLite preamble"
    );

    remove_related_files(path);
    return failures;
}

static int seed_schema(mylite_db *database) {
    int failures = 0;

    failures += expect_nonquery(database, "CREATE DATABASE app", 1);
    failures += expect_nonquery(database, "USE app", 0);
    return failures;
}

static int expect_nonquery(mylite_db *database, const char *sql, int64_t affected_rows) {
    return expect_nonquery_with_warnings(
        database,
        sql,
        (struct expected_nonquery){affected_rows, 0U}
    );
}

static int expect_nonquery_with_warnings(
    mylite_db *database,
    const char *sql,
    struct expected_nonquery expected
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_column_count(result), 0U, "nonquery column count");
        failures += expect_size(mylite_result_row_count(result), 0U, "nonquery row count");
        failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
        failures += expect_size(
            mylite_result_warning_count(result),
            expected.warning_count,
            "nonquery warning count"
        );
    } else {
        fprintf(stderr, "%s failed: %s\n", sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_error(mylite_db *database, const char *sql, int expected_code) {
    return expect_error_details(database, sql, expected_code, NULL, NULL);
}

static int expect_error_details(
    mylite_db *database,
    const char *sql,
    int expected_code,
    const char *expected_sqlstate,
    const char *expected_message
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected_code, "diagnostic code");
    if (expected_sqlstate != NULL) {
        failures +=
            expect_text(mylite_sqlstate(database), expected_sqlstate, "diagnostic SQLSTATE");
    }
    if (expected_message != NULL) {
        failures += expect_text(mylite_errmsg(database), expected_message, "diagnostic message");
    }
    failures += expect_size((size_t)(result != NULL), 0U, "error result");
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);

    failures += expect_int(rc, MYLITE_OK, query.context);
    if (rc == MYLITE_OK) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                const size_t value_index = (row * query.column_count) + column;

                failures += expect_result_value(
                    result,
                    row,
                    column,
                    query.values == NULL ? NULL : query.values[value_index],
                    query.context
                );
            }
        }
    } else {
        fprintf(stderr, "%s failed: %s\n", query.sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_row_count_zero(mylite_db *database) {
    static const char *const values[] = {"0"};

    return expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .values = values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "transaction ROW_COUNT()",
        }
    );
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
        return expect_size((size_t)(actual != NULL), 0U, context);
    }
    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_transaction_lifecycle_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path truncated\n");
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
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-wal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    (void)remove(related);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    return bytes_read == size ? 0 : 1;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", context, expected, actual);
    return 1;
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

    fprintf(stderr, "%s: bytes differ\n", context);
    return 1;
}
