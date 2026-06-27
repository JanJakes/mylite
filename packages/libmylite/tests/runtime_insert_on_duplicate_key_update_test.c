#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef P_tmpdir
#  define P_tmpdir "."
#endif

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    typed_column_count = 6,
    arithmetic_column_count = 8,
    arithmetic_multi_duplicate_affected_rows = 5,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
    mysql_error_duplicate_key = 1062,
    mysql_error_bad_null = 1048,
    mysql_error_field_no_default = 1364,
    mysql_error_data_out_of_range = 1264,
    mysql_error_data_too_long = 1406,
    mysql_error_bigint_out_of_range = 1690,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_dml {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_duplicate_update_success_warnings_and_persistence(void);
static int test_duplicate_update_diagnostics(void);
static int test_duplicate_update_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, struct expected_dml expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
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

    failures += test_duplicate_update_success_warnings_and_persistence();
    failures += test_duplicate_update_diagnostics();
    failures += test_duplicate_update_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_duplicate_update_success_warnings_and_persistence(void) {
    static const char *const primary_rows[] = {"1", "30", NULL, "2", "40", "5"};
    static const char *const multi_rows[] = {"1", "20", "33"};
    static const char *const warning_row[] = {
        "Warning",
        "1287",
        "'VALUES function' is deprecated and will be removed in a future release. Please use "
        "an alias (INSERT INTO ... VALUES (...) AS alias) and replace VALUES(col) in the ON "
        "DUPLICATE KEY UPDATE clause with alias.col instead",
    };
    static const char *const multi_warning_rows[] = {
        "Warning",
        "1287",
        "'VALUES function' is deprecated and will be removed in a future release. Please use "
        "an alias (INSERT INTO ... VALUES (...) AS alias) and replace VALUES(col) in the ON "
        "DUPLICATE KEY UPDATE clause with alias.col instead",
        "Warning",
        "1287",
        "'VALUES function' is deprecated and will be removed in a future release. Please use "
        "an alias (INSERT INTO ... VALUES (...) AS alias) and replace VALUES(col) in the ON "
        "DUPLICATE KEY UPDATE clause with alias.col instead",
    };
    static const char *const no_key_rows[] = {"1", "10", "1", "20"};
    static const char *const multi_no_key_rows[] = {"1", "10", "20"};
    static const char *const unique_rows[] = {"1", "10", "200"};
    static const char *const unique_multi_rows[] = {"1", "10", "200", "300"};
    static const char *const values_cross_rows[] = {"1", "30", "1"};
    static const char *const primary_unique_rows[] = {"1", "10", "500", "2", "20", "400"};
    static const char *const two_unique_rows[] = {"1", "10", "500", "2", "20", "400"};
    static const char *const altered_unique_rows[] = {"1", "10", "500", "2", "20", "200"};
    static const char *const temporary_shadow_rows[] = {"1", "10", "500", "2", "20", "200"};
    static const char *const persistent_shadow_rows[] = {"9", "90", "900"};
    static const char *const composite_plus_unique_rows[] = {
        "1",
        "1",
        "10",
        "300",
        "2",
        "2",
        "20",
        "400",
    };
    static const char *const multi_key_null_rows[] = {
        "NULL",
        "10",
        "100",
        "NULL",
        "20",
        "200",
    };
    static const char *const multi_key_no_op_rows[] = {"1", "10", "100"};
    static const char *const composite_primary_rows[] = {
        "1",
        "2",
        "30",
        "1",
        "1",
        "40",
        "2",
        "2",
        "50",
    };
    static const char *const composite_unique_rows[] = {
        "NULL",
        "1",
        "11",
        "1",
        "1",
        "20",
        "NULL",
        "1",
        "99",
    };
    static const char *const composite_multi_rows[] = {"1", "1", "20", "40"};
    static const char *const composite_prefix_rows[] = {"abcdef", "xyzz", "2"};
    static const char *const key_assign_rows[] = {"2", "20", "200", "3", "10", "100"};
    static const char *const key_no_op_rows[] = {"1", "10", "100"};
    static const char *const key_primary_rows[] = {"2", "20", "200", "3", "10", "300"};
    static const char *const key_composite_rows[] = {"2", "1", "200", "3", "1", "100"};
    static const char *const key_prefix_rows[] = {
        "defghi",
        "xyzz",
        "100",
        "qqqaaa",
        "rstu",
        "200",
    };
    static const char *const default_rows[] = {"1", "7", NULL};
    static const char *const multi_default_rows[] = {"1", "7", NULL};
    static const char *const nonstrict_default_rows[] = {"1", "0"};
    static const char *const nonstrict_string_rows[] = {"1", "ab"};
    static const char *const nonstrict_null_rows[] = {"1", "0", "2", "20"};
    static const char *const no_default_warning_rows[] = {
        "Warning",
        "1364",
        "Field 'v' doesn't have a default value",
    };
    static const char *const string_truncation_warning_rows[] = {
        "Warning",
        "1265",
        "Data truncated for column 's' at row 1",
    };
    static const char *const bad_null_warning_rows[] = {
        "Warning",
        "1048",
        "Column 'v' cannot be null",
    };
    static const char *const auto_increment_rows[] = {"1", "10", "200", "3", "20", "300"};
    static const char *const auto_increment_values_key_rows[] = {"1", "200"};
    static const char *const auto_increment_primary_unique_rows[] = {
        "1",
        "10",
        "300",
        "2",
        "20",
        "200",
        "4",
        "30",
        "400",
    };
    static const char *const auto_key_assignment_rows[] = {
        "1",
        "30",
        "300",
        "2",
        "20",
        "200",
        "4",
        "40",
        "400",
    };
    static const char *const last_insert_id_one[] = {"1"};
    static const char *const last_insert_id_three[] = {"3"};
    static const char *const last_insert_id_four[] = {"4"};
    static const char *const priority_rows[] = {"1", "30"};
    static const char *const typed_rows[] = {
        "9.99",
        "new",
        "2024-02-29",
        "-12:34:56",
        "2024-05-06 07:08:09",
        "2024-05-06 07:08:09",
    };
    static const char *const arithmetic_rows[] = {
        "1",
        "15",
        "18",
        "33",
        "44",
        "45",
        "66",
        NULL,
    };
    static const char *const arithmetic_set_rows[] = {"1", "14"};
    static const char *const arithmetic_noop_rows[] = {"8", NULL};
    static const char *const arithmetic_multi_rows[] = {"1", "12", "2", "20"};
    static const char *const arithmetic_mixed_rows[] = {"1", "12", "88", "5"};
    static const char *const arithmetic_row_scalar_rows[] = {"1", "1", "5"};
    static const char *const arithmetic_values_row_scalar_rows[] = {"1", "20", "21"};
    static const char *const arithmetic_values_direct_rows[] = {"1", "20", "38"};
    static const char *const string_row_scalar_rows[] = {"1", "Alpha Beta", "2", "lp"};
    static const char *const string_values_row_scalar_rows[] = {"1", "base", "new:base"};
    static const char *const temporal_row_scalar_rows[] = {"1", "12", "12:34:56"};
    static const char *const arithmetic_unsigned_row_scalar_rows[] = {"1", "1"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE pk_t(id INT PRIMARY KEY, v INT NOT NULL, n INT NULL)"
    );
    failures += expect_statement_ok(database, "INSERT INTO pk_t VALUES (1, 10, NULL)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO pk_t VALUES (1, 20, 4) ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = warning_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "values function warning",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO pk_t VALUES (1, 20, 4) ON DUPLICATE KEY UPDATE v = 20",
        (struct expected_dml){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO pk_t VALUES (1, 30, NULL), (2, 40, 5) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 3, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, n FROM pk_t ORDER BY id",
            .values = primary_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "primary duplicate rows",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE multi_t(id INT PRIMARY KEY, a INT, b INT)");
    failures += expect_statement_ok(database, "INSERT INTO multi_t VALUES (1, 10, 20)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO multi_t VALUES (1, 20, 30) "
        "ON DUPLICATE KEY UPDATE a = VALUES(a), b = VALUES(b)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 2U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = multi_warning_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "multiple values function warnings",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO multi_t VALUES (1, 20, 30) "
        "ON DUPLICATE KEY UPDATE a = VALUES(a), b = VALUES(b)",
        (struct expected_dml){.affected_rows = 0, .warning_count = 2U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO multi_t VALUES (1, 20, 99) "
        "ON DUPLICATE KEY UPDATE a = VALUES(a), b = 33",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, b FROM multi_t",
            .values = multi_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "multiple duplicate assignment row",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE values_cross_t(id INT PRIMARY KEY, v INT, n INT)"
    );
    failures += expect_statement_ok(database, "INSERT INTO values_cross_t VALUES (1, 10, 20)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO values_cross_t VALUES (1, 40, 30) "
        "ON DUPLICATE KEY UPDATE n = VALUES(id), v = VALUES(n)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 2U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, n FROM values_cross_t",
            .values = values_cross_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "cross-column VALUES duplicate assignment row",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE nk(id INT, v INT)");
    failures += expect_statement_ok(database, "INSERT INTO nk VALUES (1, 10)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO nk VALUES (1, 20) ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM nk ORDER BY v",
            .values = no_key_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "no-key duplicate tail inserts",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE nk_multi(id INT, a INT, b INT)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO nk_multi VALUES (1, 10, 20) "
        "ON DUPLICATE KEY UPDATE a = VALUES(a), b = VALUES(b)",
        (struct expected_dml){.affected_rows = 1, .warning_count = 2U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, b FROM nk_multi",
            .values = multi_no_key_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "no-key multiple duplicate tail inserts",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE unique_t(id INT, email INT UNIQUE, v INT)");
    failures += expect_statement_ok(database, "INSERT INTO unique_t VALUES (1, 10, 100)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO unique_t VALUES (2, 10, 200) ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, email, v FROM unique_t ORDER BY id",
            .values = unique_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "unique duplicate row",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE unique_multi_t(id INT, email INT UNIQUE, v INT, n INT)"
    );
    failures += expect_statement_ok(database, "INSERT INTO unique_multi_t VALUES (1, 10, 100, 50)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO unique_multi_t VALUES (2, 10, 200, 300) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v), n = VALUES(n)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 2U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, email, v, n FROM unique_multi_t ORDER BY id",
            .values = unique_multi_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "unique duplicate multiple row",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE primary_unique_t(id INT PRIMARY KEY, u INT UNIQUE, v INT)"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO primary_unique_t VALUES (1, 10, 100), (2, 20, 200)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO primary_unique_t VALUES (1, 30, 300) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO primary_unique_t VALUES (3, 20, 400) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO primary_unique_t VALUES (1, 20, 500) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, u, v FROM primary_unique_t ORDER BY id",
            .values = primary_unique_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "primary and secondary unique duplicate rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE two_unique_t(a INT UNIQUE, b INT UNIQUE, v INT)"
    );
    failures +=
        expect_statement_ok(database, "INSERT INTO two_unique_t VALUES (1, 10, 100), (2, 20, 200)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO two_unique_t VALUES (1, 30, 300) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO two_unique_t VALUES (3, 20, 400) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO two_unique_t VALUES (1, 20, 500) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM two_unique_t ORDER BY a",
            .values = two_unique_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "two secondary unique duplicate rows",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE altered_unique_t(a INT UNIQUE, b INT, v INT)");
    failures += expect_statement_ok(database, "ALTER TABLE altered_unique_t ADD UNIQUE KEY u_b(b)");
    failures += expect_statement_ok(
        database,
        "INSERT INTO altered_unique_t VALUES (1, 10, 100), (2, 20, 200)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO altered_unique_t VALUES (1, 20, 500) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM altered_unique_t ORDER BY a",
            .values = altered_unique_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "altered unique key duplicate order",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE shadow_key_t(a INT UNIQUE, b INT UNIQUE, v INT)"
    );
    failures += expect_statement_ok(database, "INSERT INTO shadow_key_t VALUES (9, 90, 900)");
    failures += expect_statement_ok(
        database,
        "CREATE TEMPORARY TABLE shadow_key_t(a INT UNIQUE, b INT UNIQUE, v INT)"
    );
    failures +=
        expect_statement_ok(database, "INSERT INTO shadow_key_t VALUES (1, 10, 100), (2, 20, 200)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO shadow_key_t VALUES (1, 20, 500) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM shadow_key_t ORDER BY a",
            .values = temporary_shadow_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "temporary shadow duplicate rows",
        }
    );
    failures += expect_statement_ok(database, "DROP TEMPORARY TABLE shadow_key_t");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM shadow_key_t",
            .values = persistent_shadow_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "persistent table after temporary shadow duplicate rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE composite_plus_unique(a INT NOT NULL, b INT NOT NULL, u INT UNIQUE, v INT, "
        "PRIMARY KEY(a,b))"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO composite_plus_unique VALUES (1, 1, 10, 100), (2, 2, 20, 200)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO composite_plus_unique VALUES (1, 1, 30, 300) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO composite_plus_unique VALUES (3, 3, 20, 400) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, u, v FROM composite_plus_unique ORDER BY a",
            .values = composite_plus_unique_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "composite primary plus unique duplicate rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE multi_key_nullable(a INT UNIQUE, b INT UNIQUE, v INT)"
    );
    failures +=
        expect_statement_ok(database, "INSERT INTO multi_key_nullable VALUES (NULL, 10, 100)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO multi_key_nullable VALUES (NULL, 20, 200) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT IFNULL(a, 'NULL'), b, v FROM multi_key_nullable ORDER BY b",
            .values = multi_key_null_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "multi-key nullable unique inserts",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE multi_key_no_op(a INT UNIQUE, b INT UNIQUE, v INT)"
    );
    failures += expect_statement_ok(database, "INSERT INTO multi_key_no_op VALUES (1, 10, 100)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO multi_key_no_op VALUES (1, 20, 100) ON DUPLICATE KEY UPDATE v = 100",
        (struct expected_dml){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM multi_key_no_op",
            .values = multi_key_no_op_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "multi-key no-op duplicate row",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE composite_pk(a INT NOT NULL, b INT NOT NULL, v INT, PRIMARY KEY(a,b))"
    );
    failures += expect_statement_ok(database, "INSERT INTO composite_pk VALUES (1, 1, 10)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO composite_pk VALUES (1, 1, 20) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO composite_pk VALUES (1, 1, 20) ON DUPLICATE KEY UPDATE v = 20",
        (struct expected_dml){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO composite_pk VALUES (1, 2, 30), (1, 1, 40), (2, 2, 50) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 4, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM composite_pk ORDER BY v",
            .values = composite_primary_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "composite primary duplicate rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE composite_unique(a INT, b INT, v INT, UNIQUE KEY u_ab(a,b))"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO composite_unique VALUES (1, 1, 10), (NULL, 1, 11)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO composite_unique VALUES (1, 1, 20) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO composite_unique VALUES (NULL, 1, 99) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT IFNULL(a, 'NULL'), b, v FROM composite_unique "
                   "ORDER BY v",
            .values = composite_unique_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "composite unique duplicate rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE composite_multi(a INT, b INT, v INT, n INT, UNIQUE KEY u_ab(a,b))"
    );
    failures += expect_statement_ok(database, "INSERT INTO composite_multi VALUES (1, 1, 10, 30)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO composite_multi VALUES (1, 1, 20, 50) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v), n = 40",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v, n FROM composite_multi",
            .values = composite_multi_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "composite unique multiple duplicate assignments",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE composite_prefix(a VARCHAR(20), b VARCHAR(20), n INT, "
        "UNIQUE KEY u_ab(a(3), b(2)))"
    );
    failures +=
        expect_statement_ok(database, "INSERT INTO composite_prefix VALUES ('abcdef', 'xyzz', 1)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO composite_prefix VALUES ('abcuvw', 'xyqq', 2) "
        "ON DUPLICATE KEY UPDATE n = VALUES(n)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, n FROM composite_prefix",
            .values = composite_prefix_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "composite prefix duplicate row",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE key_assign(a INT UNIQUE, b INT UNIQUE, v INT)");
    failures += expect_statement_ok(database, "INSERT INTO key_assign VALUES (1, 10, 100)");
    failures += expect_statement_ok(database, "INSERT INTO key_assign VALUES (2, 20, 200)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO key_assign VALUES (1, 30, 300) ON DUPLICATE KEY UPDATE a = 3",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM key_assign ORDER BY a",
            .values = key_assign_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "unique key assignment rows",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE key_no_op(a INT UNIQUE, b INT, v INT)");
    failures += expect_statement_ok(database, "INSERT INTO key_no_op VALUES (1, 10, 100)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO key_no_op VALUES (1, 20, 200) ON DUPLICATE KEY UPDATE a = VALUES(a)",
        (struct expected_dml){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM key_no_op",
            .values = key_no_op_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "same key VALUES assignment no-op",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE key_primary(id INT PRIMARY KEY, u INT UNIQUE, v INT)"
    );
    failures +=
        expect_statement_ok(database, "INSERT INTO key_primary VALUES (1, 10, 100), (2, 20, 200)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO key_primary VALUES (1, 30, 300) "
        "ON DUPLICATE KEY UPDATE id = 3, v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, u, v FROM key_primary ORDER BY id",
            .values = key_primary_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "primary key assignment rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE key_composite(a INT NOT NULL, b INT NOT NULL, v INT, "
        "UNIQUE KEY u_ab(a,b))"
    );
    failures +=
        expect_statement_ok(database, "INSERT INTO key_composite VALUES (1, 1, 100), (2, 1, 200)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO key_composite VALUES (1, 1, 300) ON DUPLICATE KEY UPDATE a = 3",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM key_composite ORDER BY a",
            .values = key_composite_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "composite unique key assignment rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE key_prefix(name VARCHAR(20), code VARCHAR(20), v INT, "
        "UNIQUE KEY u_name_code(name(3), code(2)))"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO key_prefix VALUES ('abcdef', 'xyzz', 100), ('qqqaaa', 'rstu', 200)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO key_prefix VALUES ('abcuvw', 'xy11', 300) "
        "ON DUPLICATE KEY UPDATE name = 'defghi'",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT name, code, v FROM key_prefix ORDER BY name",
            .values = key_prefix_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "prefix unique key assignment rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE set_t(id INT PRIMARY KEY, v INT NOT NULL DEFAULT 7, n INT NULL)"
    );
    failures += expect_statement_ok(database, "INSERT INTO set_t SET id = 1, v = 10, n = NULL");
    failures += expect_dml_ok(
        database,
        "INSERT INTO set_t SET id = 1, v = 99, n = 3 ON DUPLICATE KEY UPDATE v = DEFAULT",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO set_t SET id = 1, v = 99, n = 3 ON DUPLICATE KEY UPDATE n = NULL",
        (struct expected_dml){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, n FROM set_t",
            .values = default_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "set duplicate default and null",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE set_multi_t(id INT PRIMARY KEY, v INT NOT NULL DEFAULT 7, n INT NULL)"
    );
    failures += expect_statement_ok(database, "INSERT INTO set_multi_t SET id = 1, v = 10, n = 20");
    failures += expect_dml_ok(
        database,
        "INSERT INTO set_multi_t SET id = 1, v = 99, n = 3 "
        "ON DUPLICATE KEY UPDATE v = DEFAULT, n = NULL",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, n FROM set_multi_t",
            .values = multi_default_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "multiple duplicate default and null",
        }
    );

    failures += expect_statement_ok(database, "SET sql_mode = ''");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ns_default(id INT PRIMARY KEY, v INT NOT NULL)"
    );
    failures += expect_statement_ok(database, "INSERT INTO ns_default VALUES (1, 10)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO ns_default VALUES (1, 20) ON DUPLICATE KEY UPDATE v = DEFAULT",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = no_default_warning_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "nonstrict duplicate default warning",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM ns_default",
            .values = nonstrict_default_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "nonstrict duplicate default row",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE ns_string(id INT PRIMARY KEY, s VARCHAR(2) NOT NULL)"
    );
    failures += expect_statement_ok(database, "INSERT INTO ns_string VALUES (1, 'aa')");
    failures += expect_dml_ok(
        database,
        "INSERT INTO ns_string VALUES (1, 'bb') ON DUPLICATE KEY UPDATE s = 'abcd'",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = string_truncation_warning_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "nonstrict duplicate string warning",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, s FROM ns_string",
            .values = nonstrict_string_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "nonstrict duplicate string row",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE ns_null(id INT PRIMARY KEY, v INT NOT NULL)");
    failures += expect_statement_ok(database, "INSERT INTO ns_null VALUES (1, 10)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO ns_null VALUES (2, 20), (1, 30) ON DUPLICATE KEY UPDATE v = NULL",
        (struct expected_dml){.affected_rows = 3, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = bad_null_warning_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "nonstrict duplicate null warning",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM ns_null ORDER BY id",
            .values = nonstrict_null_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "nonstrict duplicate null rows",
        }
    );
    failures += expect_statement_ok(database, "SET sql_mode = DEFAULT");

    failures += expect_statement_ok(
        database,
        "CREATE TABLE auto_t(id INT AUTO_INCREMENT, email INT UNIQUE, v INT, KEY(id))"
    );
    failures += expect_statement_ok(database, "INSERT INTO auto_t(email, v) VALUES (10, 100)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO auto_t(email, v) VALUES (10, 200) ON DUPLICATE KEY UPDATE v = 200",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = last_insert_id_one,
            .column_count = 1U,
            .row_count = 1U,
            .context = "duplicate generated auto increment leaves last insert id",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO auto_t(email, v) VALUES (20, 300)",
        (struct expected_dml){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = last_insert_id_three,
            .column_count = 1U,
            .row_count = 1U,
            .context = "post-duplicate generated auto increment id",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, email, v FROM auto_t ORDER BY id",
            .values = auto_increment_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "duplicate generated auto increment rows",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE auto_values_key(id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_ok(database, "INSERT INTO auto_values_key VALUES (1, 100)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO auto_values_key VALUES (1, 200) "
        "ON DUPLICATE KEY UPDATE id = VALUES(id), v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 2U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM auto_values_key",
            .values = auto_increment_values_key_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "auto increment same-column VALUES assignment",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE auto_multi_key(id INT AUTO_INCREMENT PRIMARY KEY, u INT UNIQUE, v INT)"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO auto_multi_key(u, v) VALUES (10, 100), (20, 200)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO auto_multi_key(u, v) VALUES (10, 300) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = last_insert_id_one,
            .column_count = 1U,
            .row_count = 1U,
            .context = "multi-key duplicate generated auto increment leaves last insert id",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO auto_multi_key(u, v) VALUES (30, 400)",
        (struct expected_dml){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = last_insert_id_four,
            .column_count = 1U,
            .row_count = 1U,
            .context = "post-multi-key-duplicate generated auto increment id",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, u, v FROM auto_multi_key ORDER BY id",
            .values = auto_increment_primary_unique_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "multi-key duplicate generated auto increment rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE auto_key_assign(id INT AUTO_INCREMENT PRIMARY KEY, u INT UNIQUE, v INT)"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO auto_key_assign(u, v) VALUES (10, 100), (20, 200)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO auto_key_assign(u, v) VALUES (10, 300) "
        "ON DUPLICATE KEY UPDATE u = 30, v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = last_insert_id_one,
            .column_count = 1U,
            .row_count = 1U,
            .context = "non-auto key assignment leaves last insert id",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO auto_key_assign(u, v) VALUES (40, 400)",
        (struct expected_dml){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = last_insert_id_four,
            .column_count = 1U,
            .row_count = 1U,
            .context = "post-non-auto-key-assignment generated auto increment id",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, u, v FROM auto_key_assign ORDER BY id",
            .values = auto_key_assignment_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "non-auto key assignment with auto increment rows",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE priority_t(id INT PRIMARY KEY, v INT)");
    failures += expect_dml_ok(
        database,
        "INSERT LOW_PRIORITY INTO priority_t VALUES (1, 10) "
        "ON DUPLICATE KEY UPDATE v = 20",
        (struct expected_dml){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT HIGH_PRIORITY INTO priority_t VALUES (1, 10) "
        "ON DUPLICATE KEY UPDATE v = 30",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM priority_t",
            .values = priority_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "priority modifiers with duplicate update",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE typed_t(id INT PRIMARY KEY, amount DECIMAL(5,2), name VARCHAR(10), "
        "d DATE, tm TIME, dt DATETIME, ts TIMESTAMP NULL)"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO typed_t VALUES (1, 1.25, 'old', '2024-01-01', '01:02:03', "
        "'2024-01-01 01:02:03', '2024-01-01 01:02:03')"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO typed_t VALUES (1, 9.99, 'proposed', '2024-03-04', '04:05:06', "
        "'2024-03-04 04:05:06', '2024-03-04 04:05:06') "
        "ON DUPLICATE KEY UPDATE amount = 9.99",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO typed_t VALUES (1, 9.99, 'proposed', '2024-03-04', '04:05:06', "
        "'2024-03-04 04:05:06', '2024-03-04 04:05:06') "
        "ON DUPLICATE KEY UPDATE name = 'new'",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO typed_t VALUES (1, 9.99, 'proposed', '2024-03-04', '04:05:06', "
        "'2024-03-04 04:05:06', '2024-03-04 04:05:06') "
        "ON DUPLICATE KEY UPDATE d = '2024-02-29'",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO typed_t VALUES (1, 9.99, 'proposed', '2024-03-04', '04:05:06', "
        "'2024-03-04 04:05:06', '2024-03-04 04:05:06') "
        "ON DUPLICATE KEY UPDATE tm = '-12:34:56'",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO typed_t VALUES (1, 9.99, 'proposed', '2024-03-04', '04:05:06', "
        "'2024-03-04 04:05:06', '2024-03-04 04:05:06') "
        "ON DUPLICATE KEY UPDATE dt = '2024-05-06 07:08:09'",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO typed_t VALUES (1, 9.99, 'proposed', '2024-03-04', '04:05:06', "
        "'2024-03-04 04:05:06', '2024-03-04 04:05:06') "
        "ON DUPLICATE KEY UPDATE ts = '2024-05-06 07:08:09'",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT amount, name, d, tm, dt, ts FROM typed_t",
            .values = typed_rows,
            .column_count = typed_column_count,
            .row_count = 1U,
            .context = "typed duplicate assignments",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE arithmetic_t(id INT PRIMARY KEY, i INT, ii INTEGER, bi BIGINT, "
        "ui INT UNSIGNED, uii INTEGER UNSIGNED, ubi BIGINT UNSIGNED, n INT NULL)"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO arithmetic_t VALUES (1, 10, 20, 30, 40, 50, 60, NULL)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO arithmetic_t VALUES (1, 0, 0, 0, 0, 0, 0, 1) "
        "ON DUPLICATE KEY UPDATE i = i + 5, ii = ii - 2, bi = bi + 3, "
        "ui = ui + 4, uii = uii - 5, ubi = ubi + 6, n = n + 7",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, ii, bi, ui, uii, ubi, n FROM arithmetic_t",
            .values = arithmetic_rows,
            .column_count = arithmetic_column_count,
            .row_count = 1U,
            .context = "same-column arithmetic duplicate update",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE arithmetic_set(id INT PRIMARY KEY, n INT)");
    failures += expect_statement_ok(database, "INSERT INTO arithmetic_set SET id = 1, n = 10");
    failures += expect_dml_ok(
        database,
        "INSERT INTO arithmetic_set SET id = 1, n = 0 ON DUPLICATE KEY UPDATE n = n + 4",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n FROM arithmetic_set",
            .values = arithmetic_set_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "same-column arithmetic duplicate update in INSERT SET",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE arithmetic_noop(id INT PRIMARY KEY, n INT NULL, m INT NULL)"
    );
    failures += expect_statement_ok(database, "INSERT INTO arithmetic_noop VALUES (1, 8, NULL)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO arithmetic_noop VALUES (1, 0, 0) ON DUPLICATE KEY UPDATE n = n + 0",
        (struct expected_dml){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO arithmetic_noop VALUES (1, 0, 0) ON DUPLICATE KEY UPDATE m = m + 1",
        (struct expected_dml){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO arithmetic_noop VALUES (1, 0, 0) "
        "ON DUPLICATE KEY UPDATE m = m + 9223372036854775808",
        (struct expected_dml){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT n, m FROM arithmetic_noop",
            .values = arithmetic_noop_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "same-column arithmetic no-op and null",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE arithmetic_multi(id INT PRIMARY KEY, n INT)");
    failures += expect_statement_ok(database, "INSERT INTO arithmetic_multi VALUES (1, 10)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO arithmetic_multi VALUES (2, 20), (1, 0), (1, 0) "
        "ON DUPLICATE KEY UPDATE n = n + 1",
        (struct expected_dml){
            .affected_rows = arithmetic_multi_duplicate_affected_rows,
            .warning_count = 0U,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n FROM arithmetic_multi ORDER BY id",
            .values = arithmetic_multi_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "same-column arithmetic multi-row accumulation",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE arithmetic_mixed(id INT PRIMARY KEY, a INT, b INT, c INT)"
    );
    failures +=
        expect_statement_ok(database, "INSERT INTO arithmetic_mixed VALUES (1, 10, 20, 30)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO arithmetic_mixed VALUES (1, 99, 88, 77) "
        "ON DUPLICATE KEY UPDATE a = a + 2, b = VALUES(b), c = 5",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, b, c FROM arithmetic_mixed",
            .values = arithmetic_mixed_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "same-column arithmetic mixed duplicate assignments",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE arithmetic_row_scalar(id INT PRIMARY KEY, users INT, cap INT)"
    );
    failures += expect_statement_ok(database, "INSERT INTO arithmetic_row_scalar VALUES (1, 0, 7)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO arithmetic_row_scalar VALUES (1, 0, 0) "
        "ON DUPLICATE KEY UPDATE users = GREATEST(users + 1, 0), cap = LEAST(cap, 5)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO arithmetic_row_scalar VALUES (1, 0, 0) "
        "ON DUPLICATE KEY UPDATE cap = LEAST(cap, 5)",
        (struct expected_dml){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, users, cap FROM arithmetic_row_scalar",
            .values = arithmetic_row_scalar_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "row-scalar arithmetic duplicate assignment",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE arithmetic_values_row_scalar(id INT PRIMARY KEY, n INT, out_n INT)"
    );
    failures +=
        expect_statement_ok(database, "INSERT INTO arithmetic_values_row_scalar VALUES (1, 10, 0)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO arithmetic_values_row_scalar VALUES (1, 19, 0) "
        "ON DUPLICATE KEY UPDATE n = GREATEST(VALUES(n) + 1, 0), "
        "out_n = GREATEST(VALUES(n) + 2, 0)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 2U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO arithmetic_values_row_scalar VALUES (1, 19, 0) "
        "ON DUPLICATE KEY UPDATE n = GREATEST(VALUES(n) + 1, 0), "
        "out_n = GREATEST(VALUES(n) + 2, 0)",
        (struct expected_dml){.affected_rows = 0, .warning_count = 2U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, out_n FROM arithmetic_values_row_scalar",
            .values = arithmetic_values_row_scalar_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "row-scalar arithmetic VALUES duplicate assignment",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE arithmetic_values_direct(id INT PRIMARY KEY, n INT, out_n INT)"
    );
    failures +=
        expect_statement_ok(database, "INSERT INTO arithmetic_values_direct VALUES (1, 10, 0)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO arithmetic_values_direct VALUES (1, 19, 0) "
        "ON DUPLICATE KEY UPDATE n = VALUES(n) + 1, out_n = VALUES(n) * 2",
        (struct expected_dml){.affected_rows = 2, .warning_count = 2U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO arithmetic_values_direct VALUES (1, 19, 0) "
        "ON DUPLICATE KEY UPDATE n = VALUES(n) + 1, out_n = VALUES(n) * 2",
        (struct expected_dml){.affected_rows = 0, .warning_count = 2U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, out_n FROM arithmetic_values_direct",
            .values = arithmetic_values_direct_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "direct arithmetic VALUES duplicate assignment",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE string_row_scalar(id INT PRIMARY KEY, s VARCHAR(32), n INT, out_s "
        "VARCHAR(32))"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO string_row_scalar VALUES (1, 'Alpha Beta', 2, NULL)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO string_row_scalar VALUES (1, 'ignored', 9, NULL) "
        "ON DUPLICATE KEY UPDATE out_s = SUBSTRING(s, 2, n)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO string_row_scalar VALUES (1, 'ignored', 9, NULL) "
        "ON DUPLICATE KEY UPDATE out_s = SUBSTRING(s, 2, n)",
        (struct expected_dml){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, s, n, out_s FROM string_row_scalar",
            .values = string_row_scalar_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "row-scalar string duplicate assignment",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE string_values_row_scalar(id INT PRIMARY KEY, s VARCHAR(32), out_s "
        "VARCHAR(32))"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO string_values_row_scalar VALUES (1, 'base', '')"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO string_values_row_scalar VALUES (1, 'new', '') "
        "ON DUPLICATE KEY UPDATE out_s = CONCAT(VALUES(s), ':', s)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO string_values_row_scalar VALUES (1, 'new', '') "
        "ON DUPLICATE KEY UPDATE out_s = CONCAT(VALUES(s), ':', s)",
        (struct expected_dml){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, s, out_s FROM string_values_row_scalar",
            .values = string_values_row_scalar_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "row-scalar string VALUES duplicate assignment",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE temporal_row_scalar(id INT PRIMARY KEY, h INT, out_tm VARCHAR(32))"
    );
    failures +=
        expect_statement_ok(database, "INSERT INTO temporal_row_scalar VALUES (1, 12, NULL)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO temporal_row_scalar VALUES (1, 99, NULL) "
        "ON DUPLICATE KEY UPDATE out_tm = MAKETIME(h, 34, 56)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO temporal_row_scalar VALUES (1, 99, NULL) "
        "ON DUPLICATE KEY UPDATE out_tm = MAKETIME(h, 34, 56)",
        (struct expected_dml){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, h, out_tm FROM temporal_row_scalar",
            .values = temporal_row_scalar_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "row-scalar temporal duplicate assignment",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE arithmetic_unsigned_row_scalar(id INT PRIMARY KEY, users BIGINT UNSIGNED)"
    );
    failures +=
        expect_statement_ok(database, "INSERT INTO arithmetic_unsigned_row_scalar VALUES (1, 0)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO arithmetic_unsigned_row_scalar VALUES (1, 0) "
        "ON DUPLICATE KEY UPDATE users = GREATEST(users + 1, 0)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, users FROM arithmetic_unsigned_row_scalar",
            .values = arithmetic_unsigned_row_scalar_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "unsigned row-scalar arithmetic duplicate assignment",
        }
    );

    failures += execute_ok(
        database,
        "INSERT DELAYED INTO pk_t VALUES (3, 50, NULL) ON DUPLICATE KEY UPDATE v = 50",
        &result
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "delayed insert affected rows");
    failures +=
        expect_size(mylite_result_warning_count(result), 1U, "delayed insert warning count");
    mylite_result_free(result);
    result = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "duplicate update preserves preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, n FROM pk_t WHERE id <= 2 ORDER BY id",
            .values = primary_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "persisted duplicate update rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM key_assign ORDER BY a",
            .values = key_assign_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "persisted key assignment rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, ii, bi, ui, uii, ubi, n FROM arithmetic_t",
            .values = arithmetic_rows,
            .column_count = arithmetic_column_count,
            .row_count = 1U,
            .context = "persisted arithmetic duplicate update rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_duplicate_update_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL)");
    failures += expect_statement_ok(database, "INSERT INTO t VALUES (1, 10)");
    failures += expect_statement_ok(database, "CREATE TABLE select_source(id INT, v INT)");
    failures += expect_statement_ok(database, "INSERT INTO select_source VALUES (1, 20)");

    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing'",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = VALUES(missing)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing'",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = 20, v2 = VALUES(missing)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'v2'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE values_mismatch_t(id INT PRIMARY KEY, v VARCHAR(3), n VARCHAR(5))"
    );
    failures +=
        expect_statement_ok(database, "INSERT INTO values_mismatch_t VALUES (1, 'a', 'abc')");
    failures += execute_error(
        database,
        "INSERT INTO values_mismatch_t VALUES (1, 'b', 'abcd') "
        "ON DUPLICATE KEY UPDATE v = VALUES(n)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "does not support implicit VALUES() conversion",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = NULL",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'v' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_field_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'v' doesn't have a default value",
        }
    );
    failures += expect_statement_ok(database, "SET sql_mode = ''");
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = NULL",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'v' cannot be null",
        }
    );
    failures += expect_statement_ok(database, "SET sql_mode = DEFAULT");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE strict_string(id INT PRIMARY KEY, s VARCHAR(2) NOT NULL)"
    );
    failures += expect_statement_ok(database, "INSERT INTO strict_string VALUES (1, 'aa')");
    failures += execute_error(
        database,
        "INSERT INTO strict_string VALUES (1, 'bb') ON DUPLICATE KEY UPDATE s = 'abcd'",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long for column 's' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT IGNORE INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = 20",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INSERT IGNORE ... ON DUPLICATE KEY UPDATE is not supported",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = 20, v = 30",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "does not support duplicate assignment targets",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = 20, missing = 30",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing'",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE t.v = 20",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "supports only unqualified assignment columns",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = VALUES(t.v)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "VALUES() in ON DUPLICATE KEY UPDATE supports only unqualified columns",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = CONCAT(VALUES(t.v), '')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "VALUES() in ON DUPLICATE KEY UPDATE supports only unqualified columns",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = missing + 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing'",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = id + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "arithmetic assignment supports only same-column integer",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE id = id + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "arithmetic assignment supports only same-column integer",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = v + +1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = t.v + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = v + 1.5",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = v + '1'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = v + 0x1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = v + b'1'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = v + ABS(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = v + ?",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = v + (SELECT 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t SELECT id, v FROM select_source ON DUPLICATE KEY UPDATE v = v + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "INSERT ... SELECT ... ON DUPLICATE KEY UPDATE arithmetic assignment is not "
                "supported",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE arithmetic_string(id INT PRIMARY KEY, s VARCHAR(10))"
    );
    failures += expect_statement_ok(database, "INSERT INTO arithmetic_string VALUES (1, 'a')");
    failures += execute_error(
        database,
        "INSERT INTO arithmetic_string VALUES (1, 'b') ON DUPLICATE KEY UPDATE s = s + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "arithmetic assignment supports only integer columns",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE arithmetic_range(id INT PRIMARY KEY, i INT, u INT UNSIGNED, "
        "b BIGINT, bu BIGINT UNSIGNED, n BIGINT)"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO arithmetic_range VALUES "
        "(1, 2147483647, 0, 9223372036854775807, 0, 1)"
    );
    failures += execute_error(
        database,
        "INSERT INTO arithmetic_range VALUES (1, 0, 0, 0, 0, 0) "
        "ON DUPLICATE KEY UPDATE i = i + 1",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO arithmetic_range VALUES (1, 0, 0, 0, 0, 0) "
        "ON DUPLICATE KEY UPDATE u = u - 1",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT UNSIGNED value is out of range",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO arithmetic_range VALUES (1, 0, 0, 0, 0, 0) "
        "ON DUPLICATE KEY UPDATE b = b + 1",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO arithmetic_range VALUES (1, 0, 0, 0, 0, 0) "
        "ON DUPLICATE KEY UPDATE bu = bu - 1",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT UNSIGNED value is out of range",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO arithmetic_range VALUES (1, 0, 0, 0, 0, 0) "
        "ON DUPLICATE KEY UPDATE n = n + 9223372036854775808",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "arithmetic assignment supports only unsigned integer deltas",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE auto_key(id INT AUTO_INCREMENT PRIMARY KEY, v INT UNIQUE)"
    );
    failures += expect_statement_ok(database, "INSERT INTO auto_key(v) VALUES (10)");
    failures += execute_error(
        database,
        "INSERT INTO auto_key(v) VALUES (10) ON DUPLICATE KEY UPDATE id = 3",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "does not support auto-increment assignments",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO auto_key(v) VALUES (10) ON DUPLICATE KEY UPDATE id = id + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "arithmetic assignment supports only same-column integer",
        }
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE parent_key(id INT PRIMARY KEY, u INT UNIQUE)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE child_key(id INT PRIMARY KEY, parent_id INT, "
        "FOREIGN KEY(parent_id) REFERENCES parent_key(id) ON UPDATE CASCADE)"
    );
    failures += expect_statement_ok(database, "INSERT INTO parent_key VALUES (1, 10)");
    failures += expect_statement_ok(database, "INSERT INTO child_key VALUES (1, 1)");
    failures += execute_error(
        database,
        "INSERT INTO parent_key VALUES (1, 30) ON DUPLICATE KEY UPDATE id = 3",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "does not support parent foreign-key key assignments",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE two_keys(id INT PRIMARY KEY, u INT UNIQUE, v INT)"
    );
    failures += expect_statement_ok(database, "INSERT INTO two_keys VALUES (1, 1, 1)");
    failures += expect_statement_ok(database, "INSERT INTO two_keys VALUES (2, 2, 2)");
    failures += execute_error(
        database,
        "INSERT INTO two_keys VALUES (1, 3, 3) ON DUPLICATE KEY UPDATE u = 2",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '2' for key 'two_keys.u'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE two_secondary_keys(a INT UNIQUE, b INT UNIQUE, v INT)"
    );
    failures += expect_statement_ok(database, "INSERT INTO two_secondary_keys VALUES (1, 10, 100)");
    failures += expect_statement_ok(database, "INSERT INTO two_secondary_keys VALUES (2, 20, 200)");
    failures += execute_error(
        database,
        "INSERT INTO two_secondary_keys VALUES (1, 30, 300) ON DUPLICATE KEY UPDATE b = 20",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '20' for key 'two_secondary_keys.b'",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO two_secondary_keys VALUES (3, 30, 300), (1, 40, 400) "
        "ON DUPLICATE KEY UPDATE a = 2",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '2' for key 'two_secondary_keys.a'",
        }
    );
    {
        static const char *const key_conflict_rollback_rows[] = {
            "1",
            "10",
            "100",
            "2",
            "20",
            "200",
        };

        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SELECT a, b, v FROM two_secondary_keys ORDER BY a",
                .values = key_conflict_rollback_rows,
                .column_count = 3U,
                .row_count = 2U,
                .context = "key assignment conflict rolls back statement",
            }
        );
    }
    failures += expect_statement_ok(
        database,
        "CREATE TABLE composite_key(a INT NOT NULL, b INT NOT NULL, v INT, PRIMARY KEY(a,b))"
    );
    failures += expect_statement_ok(database, "INSERT INTO composite_key VALUES (1, 2, 1)");
    failures += expect_statement_ok(database, "INSERT INTO composite_key VALUES (3, 2, 2)");
    failures += execute_error(
        database,
        "INSERT INTO composite_key VALUES (1, 2, 3) ON DUPLICATE KEY UPDATE a = 3",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '3-2' for key 'composite_key.PRIMARY'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE prefix_conflict(name VARCHAR(20), code VARCHAR(20), v INT, "
        "UNIQUE KEY u_name_code(name(3), code(2)))"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO prefix_conflict VALUES ('abcdef', 'xyzz', 100), ('defghi', 'xy99', 200)"
    );
    failures += execute_error(
        database,
        "INSERT INTO prefix_conflict VALUES ('abcuvw', 'xy11', 300) "
        "ON DUPLICATE KEY UPDATE name = 'defuvw'",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'def-xy' for key 'prefix_conflict.u_name_code'",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE rollback_t(id INT PRIMARY KEY, ti TINYINT, v INT)"
    );
    failures += expect_statement_ok(database, "INSERT INTO rollback_t VALUES (1, 1, 10)");
    failures += execute_error(
        database,
        "INSERT INTO rollback_t VALUES (2, 2, 20), (1, 3, 30) "
        "ON DUPLICATE KEY UPDATE v = 20, ti = 128",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'ti' at row 2",
        }
    );
    {
        static const char *const rollback_rows[] = {"1", "1", "10"};

        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SELECT id, ti, v FROM rollback_t ORDER BY id",
                .values = rollback_rows,
                .column_count = 3U,
                .row_count = 1U,
                .context = "duplicate branch failure rolls back statement",
            }
        );
    }

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_duplicate_update_independent_handles(void) {
    static const char *const first_rows[] = {"1", "20", "22"};
    static const char *const second_rows[] = {"1", "30", "33"};
    static const char *const first_key_rows[] = {"2", "200", "3", "100"};
    static const char *const second_key_rows[] = {"2", "200", "4", "100"};
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
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t(id INT PRIMARY KEY, v INT, n INT)");
    failures += expect_statement_ok(second, "CREATE TABLE t(id INT PRIMARY KEY, v INT, n INT)");
    failures += expect_statement_ok(first, "CREATE TABLE key_t(a INT UNIQUE, v INT)");
    failures += expect_statement_ok(second, "CREATE TABLE key_t(a INT UNIQUE, v INT)");
    failures += expect_statement_ok(first, "INSERT INTO t VALUES (1, 10, 11)");
    failures += expect_statement_ok(second, "INSERT INTO t VALUES (1, 10, 11)");
    failures += expect_statement_ok(first, "INSERT INTO key_t VALUES (1, 100), (2, 200)");
    failures += expect_statement_ok(second, "INSERT INTO key_t VALUES (1, 100), (2, 200)");
    failures += expect_dml_ok(
        first,
        "INSERT INTO t VALUES (1, 20, 21) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v), n = VALUES(n)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 2U}
    );
    failures += expect_dml_ok(
        second,
        "INSERT INTO t VALUES (1, 30, 31) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v), n = VALUES(n)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 2U}
    );
    failures += expect_dml_ok(
        first,
        "INSERT INTO t VALUES (1, 20, 0) ON DUPLICATE KEY UPDATE n = n + 1",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        second,
        "INSERT INTO t VALUES (1, 30, 0) ON DUPLICATE KEY UPDATE n = n + 2",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        first,
        "INSERT INTO key_t VALUES (1, 300) ON DUPLICATE KEY UPDATE a = 3",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        second,
        "INSERT INTO key_t VALUES (1, 400) ON DUPLICATE KEY UPDATE a = 4",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, v, n FROM t",
            .values = first_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "first handle row",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, v, n FROM t",
            .values = second_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "second handle row",
        }
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT a, v FROM key_t ORDER BY a",
            .values = first_key_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "first handle key assignment rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT a, v FROM key_t ORDER BY a",
            .values = second_key_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "second handle key assignment rows",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc == MYLITE_OK) {
        *out_result = result;
    } else {
        fprintf(
            stderr,
            "%s: unexpected error %d / %s: %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        *out_result = NULL;
    }
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, struct expected_dml expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
    failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);
    size_t value_index = 0U;

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
            ++value_index;
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

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s: expected NULL at %zu,%zu, got [%s]\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }

    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "%s/mylite_insert_on_duplicate_key_update_%d_%s.mylite",
        P_tmpdir,
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
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
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return;
    }
    (void)remove(buffer);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 || fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %s\n", path);
        fclose(file);
        return 1;
    }
    fclose(file);
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
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
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
            actual == NULL ? "NULL" : actual,
            needle == NULL ? "NULL" : needle
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
