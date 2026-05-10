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
    inserted_numbers_result_row_count = 5,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
    mysql_error_data_out_of_range = 1264,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_result {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    size_t warning_count;
    int64_t affected_rows;
    const char *context;
};

static int test_where_and_predicates(void);
static int test_independent_where_and_handles(void);
static int seed_database(mylite_db *database);
static int reset_numbers(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_result(mylite_db *database, struct expected_result expected);
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
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_where_and_predicates();
    failures += test_independent_where_and_handles();

    return failures == 0 ? 0 : 1;
}

static int test_where_and_predicates(void) {
    static const char *const and_row[] = {"2"};
    static const char *const nested_row[] = {"2"};
    static const char *const null_row[] = {"3"};
    static const char *const distinct_row[] = {"9"};
    static const char *const count_row[] = {"1"};
    static const char *const max_row[] = {"2147483647"};
    static const char *const grouped_rows[] = {"1", "1", "2", "2"};
    static const char *const copy_row[] = {"2", "1", "9"};
    static const char *const inserted_rows[] = {
        "1",
        "-2",
        "2",
        "1",
        "2",
        "1",
        "3",
        "2147483647",
        "4",
        "0",
    };
    static const char *const replaced_row[] = {"2", "1"};
    static const char *const update_rows[] = {"1", NULL, "2", "11", "3", NULL, "4", "9"};
    static const char *const update_limited_rows[] = {
        "1",
        NULL,
        "2",
        "9",
        "3",
        "99",
        "4",
        "99",
    };
    static const char *const delete_rows[] = {"1", "2", "4"};
    static const char *const delete_limited_rows[] = {"1", "2", "3"};
    static const char *const persisted_row[] = {"11"};
    static const char *const warning_row[] = {
        "Warning",
        "1287",
        "'&&' is deprecated and will be removed in a future release. Please use AND instead",
    };
    static const char *const or_rows[] = {"2", "4"};
    static const char *const or_precedence_rows[] = {"2"};
    static const char *const or_null_rows[] = {"1", "2", "3", "4"};
    static const char *const or_distinct_rows[] = {NULL, "9"};
    static const char *const or_count_row[] = {"3"};
    static const char *const or_max_row[] = {"2147483647"};
    static const char *const or_grouped_rows[] = {"1", "2", "2", "2"};
    static const char *const or_copy_rows[] = {"2", "1", "9", "4", "0", "9"};
    static const char *const or_inserted_rows[] = {"2", "1", "4", "0"};
    static const char *const or_replaced_rows[] = {"2", "1", "4", "0"};
    static const char *const or_update_rows[] = {"1", NULL, "2", "11", "3", NULL, "4", "11"};
    static const char *const or_update_null_rows[] = {"1", NULL, "2", NULL, "3", NULL, "4", "9"};
    static const char *const or_update_limited_rows[] = {
        "1",
        NULL,
        "2",
        "9",
        "3",
        "99",
        "4",
        "99",
    };
    static const char *const or_delete_rows[] = {"1", "3"};
    static const char *const or_delete_limited_rows[] = {"1", "2"};
    static const char *const or_persisted_rows[] = {"22", "22"};
    static const char *const or_warning_rows[] = {"1", "2", "4"};
    static const char *const or_warning_table[] = {
        "Warning",
        "1287",
        "'|| as a synonym for OR' is deprecated and will be removed in a future release. Please "
        "use OR instead",
        "Warning",
        "1287",
        "'|| as a synonym for OR' is deprecated and will be removed in a future release. Please "
        "use OR instead",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "lifecycle") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open lifecycle database");
    failures += seed_database(database);
    failures += reset_numbers(database);

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i = 1 AND nn = 6",
            .values = and_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "comparison and predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE (i = 1 AND (nn = 6 AND n IS NOT NULL))",
            .values = nested_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "nested and predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE n IS NULL AND nn = 7",
            .values = null_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is null and predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i = 1 && nn = 6",
            .values = and_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 1U,
            .affected_rows = 0,
            .context = "deprecated symbolic and predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SHOW WARNINGS",
            .values = warning_row,
            .column_count = 3U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "deprecated symbolic and warning",
        }
    );

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i = 1 OR nn = 8 ORDER BY id",
            .values = or_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "comparison or predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE i = 1 OR nn = 8 AND n IS NULL ORDER BY id",
            .values = or_precedence_rows,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "or and precedence predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE (i = 1 OR nn = 8) AND n IS NULL ORDER BY id",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "parenthesized or predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE n IS NULL OR n = 9 ORDER BY id",
            .values = or_null_rows,
            .column_count = 1U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "or null predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers WHERE id = 1 || nn = 8 || i = 1 ORDER BY id",
            .values = or_warning_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 2U,
            .affected_rows = 0,
            .context = "deprecated symbolic or predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SHOW WARNINGS",
            .values = or_warning_table,
            .column_count = 3U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "deprecated symbolic or warnings",
        }
    );

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT DISTINCT n FROM numbers WHERE n IS NOT NULL AND tie = 1",
            .values = distinct_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "distinct source and predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE i >= 0 AND n IS NULL",
            .values = count_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "count source and predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT MAX(i) FROM numbers WHERE nn >= 6 AND n IS NULL",
            .values = max_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "aggregate source and predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT tie, COUNT(*) FROM numbers WHERE id > 1 AND nn >= 6 GROUP BY tie "
                   "ORDER BY tie",
            .values = grouped_rows,
            .column_count = 2U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "grouped aggregate source and predicate",
        }
    );

    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT DISTINCT n FROM numbers WHERE n IS NULL OR tie = 1 ORDER BY n",
            .values = or_distinct_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "distinct source or predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM numbers WHERE n = 9 OR nn = 7",
            .values = or_count_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "count source or predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT MAX(i) FROM numbers WHERE nn = 6 OR n IS NULL",
            .values = or_max_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "aggregate source or predicate",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT tie, COUNT(*) FROM numbers WHERE id = 1 OR nn >= 6 GROUP BY tie "
                   "ORDER BY tie",
            .values = or_grouped_rows,
            .column_count = 2U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "grouped aggregate source or predicate",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE copy_numbers SELECT id, i, n FROM numbers WHERE i = 1 AND n IS NOT NULL",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "create table select affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i, n FROM copy_numbers ORDER BY id",
            .values = copy_row,
            .column_count = 3U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "create table select copied rows",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE copy_or_numbers SELECT id, i, n FROM numbers WHERE i = 1 OR nn = 8",
        &result
    );
    failures += expect_int64(
        mylite_result_affected_rows(result),
        2,
        "create table select or affected rows"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i, n FROM copy_or_numbers ORDER BY id",
            .values = or_copy_rows,
            .column_count = 3U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "create table select or copied rows",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE inserted_numbers (id INT NOT NULL, i INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "INSERT INTO inserted_numbers SELECT id, i FROM numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO inserted_numbers SELECT id, i FROM numbers WHERE i = 1 AND n IS NOT NULL",
        &result
    );
    failures += expect_int64(mylite_result_affected_rows(result), 1, "insert select affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i FROM inserted_numbers ORDER BY id",
            .values = inserted_rows,
            .column_count = 2U,
            .row_count = inserted_numbers_result_row_count,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "insert select source and predicate",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE or_inserted_numbers (id INT NOT NULL, i INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO or_inserted_numbers SELECT id, i FROM numbers WHERE i = 1 OR nn = 8",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 2, "insert select or affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i FROM or_inserted_numbers ORDER BY id",
            .values = or_inserted_rows,
            .column_count = 2U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "insert select source or predicate",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE replaced_numbers (id INT NOT NULL, i INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "REPLACE INTO replaced_numbers SELECT id, i FROM numbers WHERE i = 1 AND n IS NOT NULL",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "replace select affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i FROM replaced_numbers ORDER BY id",
            .values = replaced_row,
            .column_count = 2U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "replace select source and predicate",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE or_replaced_numbers (id INT NOT NULL, i INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "REPLACE INTO or_replaced_numbers SELECT id, i FROM numbers WHERE i = 1 OR nn = 8",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 2, "replace select or affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, i FROM or_replaced_numbers ORDER BY id",
            .values = or_replaced_rows,
            .column_count = 2U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "replace select source or predicate",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(database, "UPDATE numbers SET n = 11 WHERE i = 1 AND nn = 6", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 1, "update and affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = update_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "update and row state",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(database, "UPDATE numbers SET n = 11 WHERE i = 1 OR nn = 8", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 2, "update or affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = or_update_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "update or row state",
        }
    );
    failures += reset_numbers(database);
    failures += execute_ok(database, "UPDATE numbers SET n = NULL WHERE id = 1 OR nn = 6", &result);
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "update or changed affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = or_update_null_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "update or changed row state",
        }
    );
    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "UPDATE numbers SET n = 99 WHERE id > 1 AND nn >= 6 ORDER BY id DESC LIMIT 2",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 2, "update and limit affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = update_limited_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "update and limit row state",
        }
    );
    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "UPDATE numbers SET n = 99 WHERE id = 1 OR nn >= 6 ORDER BY id DESC LIMIT 2",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 2, "update or limit affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id, n FROM numbers ORDER BY id",
            .values = or_update_limited_rows,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "update or limit row state",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(database, "DELETE FROM numbers WHERE i > 1 AND n IS NULL", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 1, "delete and affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers ORDER BY id",
            .values = delete_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "delete and row state",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(database, "DELETE FROM numbers WHERE i = 1 OR nn = 8", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 2, "delete or affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers ORDER BY id",
            .values = or_delete_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "delete or row state",
        }
    );
    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "DELETE FROM numbers WHERE id > 1 AND nn >= 6 ORDER BY id DESC LIMIT 1",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "delete and limit affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers ORDER BY id",
            .values = delete_limited_rows,
            .column_count = 1U,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "delete and limit row state",
        }
    );
    failures += reset_numbers(database);
    failures += execute_ok(
        database,
        "DELETE FROM numbers WHERE id = 1 OR nn >= 6 ORDER BY id DESC LIMIT 2",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 2, "delete or limit affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT id FROM numbers ORDER BY id",
            .values = or_delete_limited_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "delete or limit row state",
        }
    );

    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE id = 1 AND missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE id = 1 OR missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE id = 1 AND i = 2147483648",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE id = 1 OR i = 2147483648",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE id = 1 XOR nn = 8",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE NOT id = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE TRUE AND id = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers WHERE id = nn AND id = 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    failures += reset_numbers(database);
    failures += execute_ok(database, "UPDATE numbers SET n = 11 WHERE id = 2 AND nn = 6", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "UPDATE numbers SET n = 22 WHERE id = 1 OR id = 4", &result);
    mylite_result_free(result);
    result = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "preamble after where and lifecycle"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen lifecycle database");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE id = 2 AND nn = 6",
            .values = persisted_row,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "reopened updated row",
        }
    );
    failures += expect_result(
        database,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE id = 1 OR id = 4 ORDER BY id",
            .values = or_persisted_rows,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "reopened or updated rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_independent_where_and_handles(void) {
    static const char *const first_values[] = {"31"};
    static const char *const second_values[] = {"41"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first database");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second database");
    failures += seed_database(first);
    failures += seed_database(second);
    failures += execute_ok(first, "UPDATE numbers SET n = 31 WHERE id = 2 OR nn = 100", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "UPDATE numbers SET n = 41 WHERE id = 2 OR nn = 100", &result);
    mylite_result_free(result);
    result = NULL;

    failures += expect_result(
        first,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE id = 2 OR nn = 100",
            .values = first_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first independent handle",
        }
    );
    failures += expect_result(
        second,
        (struct expected_result){
            .sql = "SELECT n FROM numbers WHERE id = 2 OR nn = 100",
            .values = second_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second independent handle",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);

    return failures;
}

static int seed_database(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += reset_numbers(database);

    return failures;
}

static int reset_numbers(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = 0;

    (void)execute_ok(database, "DROP TABLE IF EXISTS numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE numbers (id INT NOT NULL, i INT, iu INT UNSIGNED, b BIGINT, "
        "bu BIGINT UNSIGNED, n INT NULL, nn INT NOT NULL, tie INT NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO numbers VALUES "
        "(1, -2, 0, -9223372036854775808, 0, NULL, 5, 1), "
        "(2, 1, 2, 3, 4, 9, 6, 1), "
        "(3, 2147483647, 4294967295, 9223372036854775807, 9223372036854775807, NULL, 7, 2), "
        "(4, 0, 8, 8, 8, 9, 8, 2)",
        &result
    );
    mylite_result_free(result);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s/%s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "%s: expected error, got status %d\n", sql, rc);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);

    return failures;
}

static int expect_result(mylite_db *database, struct expected_result expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, expected.context);
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t index = (row * expected.column_count) + column;

            failures +=
                expect_result_value(result, row, column, expected.values[index], expected.context);
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

    if (expected == NULL && actual == NULL) {
        return 0;
    }
    if (expected == NULL || actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: row %zu column %zu expected [%s], got [%s]\n",
            context,
            row,
            column,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }

    return 0;
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
        "%s/mylite_runtime_where_and_predicates_%d_%s.mylite",
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
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    remove(related);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_size = 0U;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        fprintf(stderr, "%s: failed to seek file\n", path);
        return 1;
    }
    read_size = fread(buffer, 1U, size, file);
    fclose(file);
    if (read_size != size) {
        fprintf(stderr, "%s: expected %zu bytes, read %zu\n", path, size, read_size);
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing [%s], got [%s]\n",
            context,
            needle,
            actual == NULL ? "NULL" : actual
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
