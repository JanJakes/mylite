#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

#include <stdbool.h>
#include <stdint.h>
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
    show_columns_field_count = 6,
    bit_column_count = 11,
    information_schema_bit_column_count = 10,
    information_schema_bit_projection_count = 6,
    selected_bit_b64_column = 5,
    selected_bit_nn_column = 6,
    selected_bit_defi_column = 7,
    selected_bit_deft_column = 8,
    selected_bit_deff_column = 9,
    selected_bit_b64_no_default_column = 5,
    selected_bit_nn_no_default_column = 6,
    order_bit_inserted_row_count = 5,
    mysql_error_parse = 1064,
    mysql_error_bad_null = 1048,
    mysql_error_invalid_default = 1067,
    mysql_error_data_out_of_range = 1264,
    mysql_error_data_too_long = 1406,
    mysql_error_display_width_out_of_range = 1439,
    mysql_error_invalid_column_size = 3013,
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

struct expected_bytes {
    const unsigned char *bytes;
    size_t size;
    bool is_null;
};

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_bit_success_persistence_and_descriptor_copy(void);
static int test_bit_diagnostics(void);
static int test_bit_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_query_contains(
    mylite_db *database,
    const char *sql,
    size_t row,
    size_t column,
    const char *needle,
    const char *context
);
static int expect_bit_cell(
    const mylite_result *result,
    size_t row,
    size_t column,
    struct expected_bytes expected,
    const char *context
);
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

    failures += test_bit_success_persistence_and_descriptor_copy();
    failures += test_bit_diagnostics();
    failures += test_bit_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_bit_success_persistence_and_descriptor_copy(void) {
    static const unsigned char one_bit[] = {0x01U};
    static const unsigned char zero_bit[] = {0x00U};
    static const unsigned char bit_six_max_bit[] = {0x20U};
    static const unsigned char bit_six_one[] = {0x01U};
    static const unsigned char bit_six_two[] = {0x02U};
    static const unsigned char bit_six_three[] = {0x03U};
    static const unsigned char bit_six_five[] = {0x05U};
    static const unsigned char bit_six_seven[] = {0x07U};
    static const unsigned char bit_six_ten[] = {0x0AU};
    static const unsigned char bit_eight_a[] = {0x41U};
    static const unsigned char bit_nine_257[] = {0x01U, 0x01U};
    static const unsigned char bit_nine_one[] = {0x00U, 0x01U};
    static const unsigned char bit_sixty_four_max[] = {
        0xffU,
        0xffU,
        0xffU,
        0xffU,
        0xffU,
        0xffU,
        0xffU,
        0xffU,
    };
    static const unsigned char bit_sixty_four_zero[] = {
        0x00U,
        0x00U,
        0x00U,
        0x00U,
        0x00U,
        0x00U,
        0x00U,
        0x00U,
    };
    static const char *const show_columns_rows[] = {
        "id",   "int",     "NO",  "", NULL,     "", "b",    "bit(1)", "YES", "", NULL,     "",
        "b1",   "bit(1)",  "YES", "", NULL,     "", "b6",   "bit(6)", "YES", "", "b'101'", "",
        "b8",   "bit(8)",  "YES", "", NULL,     "", "b9",   "bit(9)", "YES", "", NULL,     "",
        "b64",  "bit(64)", "YES", "", NULL,     "", "nn",   "bit(6)", "NO",  "", NULL,     "",
        "defi", "bit(6)",  "YES", "", "b'101'", "", "deft", "bit(1)", "YES", "", "b'1'",   "",
        "deff", "bit(1)",  "YES", "", "b'0'",   "",
    };
    static const char *const information_schema_rows[] = {
        "b",    "bit", "bit(1)", "1", NULL, "YES", "b1",   "bit", "bit(1)",  "1",  NULL, "YES",
        "b6",   "bit", "bit(6)", "6", NULL, "YES", "b8",   "bit", "bit(8)",  "8",  NULL, "YES",
        "b9",   "bit", "bit(9)", "9", NULL, "YES", "b64",  "bit", "bit(64)", "64", NULL, "YES",
        "nn",   "bit", "bit(6)", "6", NULL, "NO",  "defi", "bit", "bit(6)",  "6",  NULL, "YES",
        "deft", "bit", "bit(1)", "1", NULL, "YES", "deff", "bit", "bit(1)",  "1",  NULL, "YES",
    };
    static const char *const expression_show_columns_rows[] = {
        "b",  "bit(6)", "YES", "", "(1 + 2)", "DEFAULT_GENERATED",
        "m",  "bit(6)", "YES", "", "(7 % 4)", "DEFAULT_GENERATED",
        "n",  "bit(6)", "YES", "", "NULL",    "DEFAULT_GENERATED",
        "nn", "bit(6)", "NO",  "", "NULL",    "DEFAULT_GENERATED",
    };
    static const char *const expression_information_schema_rows[] = {
        "b",
        "(1 + 2)",
        "DEFAULT_GENERATED",
        "m",
        "(7 % 4)",
        "DEFAULT_GENERATED",
        "n",
        "NULL",
        "DEFAULT_GENERATED",
        "nn",
        "NULL",
        "DEFAULT_GENERATED",
    };
    static const char *const expression_altered_default_rows[] = {
        "b",
        "bit(6)",
        "YES",
        "",
        "(2 * 5)",
        "DEFAULT_GENERATED",
    };
    static const char *const predicate_ids[] = {"1", "3"};
    static const char *const remaining_predicate_ids[] = {"3", "4"};
    static const char *const bit_multi_order_ids[] = {"1", "2", "4", "3", "5"};
    static const char *const ordered_limited_ids[] = {"1", "2"};
    static const char *const ordered_desc_id[] = {"5"};
    static const char *const tie_update_count[] = {"1"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_result *result = NULL;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open bit success file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE bits (id INT NOT NULL, b BIT, b1 BIT(1), b6 BIT(6) DEFAULT b'101', "
        "b8 BIT(8), b9 BIT(9), b64 BIT(64), nn BIT(6) NOT NULL, defi BIT(6) DEFAULT 5, "
        "deft BIT(1) DEFAULT TRUE, deff BIT(1) DEFAULT FALSE)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM bits",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = bit_column_count,
            .context = "BIT SHOW COLUMNS",
        }
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE bits",
        0U,
        1U,
        "`b6` bit(6) DEFAULT b'101'",
        "BIT SHOW CREATE default"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, NUMERIC_PRECISION, "
                   "NUMERIC_SCALE, IS_NULLABLE FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'bits' AND DATA_TYPE = 'bit' "
                   "ORDER BY ORDINAL_POSITION",
            .values = information_schema_rows,
            .column_count = information_schema_bit_projection_count,
            .row_count = information_schema_bit_column_count,
            .context = "BIT INFORMATION_SCHEMA.COLUMNS",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE bit_expr (id INT, b BIT(6) DEFAULT (1 + 2), "
        "m BIT(6) DEFAULT (MOD(7,4)), n BIT(6) DEFAULT (NULL), "
        "nn BIT(6) NOT NULL DEFAULT (NULL))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM bit_expr WHERE Field <> 'id'",
            .values = expression_show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 4U,
            .context = "BIT expression default SHOW COLUMNS",
        }
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE bit_expr",
        0U,
        1U,
        "`b` bit(6) DEFAULT ((1 + 2))",
        "BIT expression default SHOW CREATE b"
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE bit_expr",
        0U,
        1U,
        "`nn` bit(6) NOT NULL DEFAULT (NULL)",
        "BIT NULL expression default SHOW CREATE"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_DEFAULT, EXTRA "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'bit_expr' AND COLUMN_NAME <> 'id' "
                   "ORDER BY ORDINAL_POSITION",
            .values = expression_information_schema_rows,
            .column_count = 3U,
            .row_count = 4U,
            .context = "BIT expression default information schema",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO bit_expr (id, nn) VALUES (1, b'111')", 1);
    failures += execute_ok(database, "SELECT b, m, n, nn FROM bit_expr WHERE id = 1", &result);
    failures += expect_bit_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = bit_six_three, .size = sizeof(bit_six_three)},
        "BIT expression default b"
    );
    failures += expect_bit_cell(
        result,
        0U,
        1U,
        (struct expected_bytes){.bytes = bit_six_three, .size = sizeof(bit_six_three)},
        "BIT expression default MOD"
    );
    failures += expect_bit_cell(
        result,
        0U,
        2U,
        (struct expected_bytes){.is_null = true},
        "BIT NULL expression default"
    );
    failures += expect_bit_cell(
        result,
        0U,
        3U,
        (struct expected_bytes){.bytes = bit_six_seven, .size = sizeof(bit_six_seven)},
        "BIT NOT NULL explicit value with NULL expression default"
    );
    mylite_result_free(result);
    result = NULL;

    failures +=
        expect_statement_ok(database, "ALTER TABLE bit_expr ALTER COLUMN b SET DEFAULT (2 * 5)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM bit_expr LIKE 'b'",
            .values = expression_altered_default_rows,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "BIT expression altered default SHOW COLUMNS",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO bit_expr (id, nn) VALUES (2, b'001')", 1);
    failures += execute_ok(database, "SELECT b, nn FROM bit_expr WHERE id = 2", &result);
    failures += expect_bit_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = bit_six_ten, .size = sizeof(bit_six_ten)},
        "BIT expression altered default value"
    );
    failures += expect_bit_cell(
        result,
        0U,
        1U,
        (struct expected_bytes){.bytes = bit_six_one, .size = sizeof(bit_six_one)},
        "BIT expression altered default explicit NOT NULL value"
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_dml_ok(
        database,
        "UPDATE bit_expr SET b = DEFAULT, m = DEFAULT, n = DEFAULT WHERE id = 1",
        1
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO bit_expr (id, b, m, n, nn) "
        "VALUES (4, DEFAULT, DEFAULT, DEFAULT, b'010')",
        1
    );
    failures += expect_dml_ok(
        database,
        "REPLACE INTO bit_expr (id, b, m, n, nn) "
        "VALUES (5, DEFAULT, DEFAULT, DEFAULT, b'101')",
        1
    );
    failures += execute_ok(
        database,
        "SELECT b, m, n, nn FROM bit_expr WHERE id IN (1, 4, 5) ORDER BY id",
        &result
    );
    failures += expect_bit_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = bit_six_ten, .size = sizeof(bit_six_ten)},
        "BIT UPDATE DEFAULT expression b"
    );
    failures += expect_bit_cell(
        result,
        0U,
        1U,
        (struct expected_bytes){.bytes = bit_six_three, .size = sizeof(bit_six_three)},
        "BIT UPDATE DEFAULT expression m"
    );
    failures += expect_bit_cell(
        result,
        0U,
        2U,
        (struct expected_bytes){.is_null = true},
        "BIT UPDATE DEFAULT expression NULL"
    );
    failures += expect_bit_cell(
        result,
        0U,
        3U,
        (struct expected_bytes){.bytes = bit_six_seven, .size = sizeof(bit_six_seven)},
        "BIT UPDATE DEFAULT expression preserves explicit NOT NULL"
    );
    failures += expect_bit_cell(
        result,
        1U,
        0U,
        (struct expected_bytes){.bytes = bit_six_ten, .size = sizeof(bit_six_ten)},
        "BIT INSERT explicit DEFAULT expression b"
    );
    failures += expect_bit_cell(
        result,
        1U,
        1U,
        (struct expected_bytes){.bytes = bit_six_three, .size = sizeof(bit_six_three)},
        "BIT INSERT explicit DEFAULT expression m"
    );
    failures += expect_bit_cell(
        result,
        1U,
        2U,
        (struct expected_bytes){.is_null = true},
        "BIT INSERT explicit DEFAULT expression NULL"
    );
    failures += expect_bit_cell(
        result,
        1U,
        3U,
        (struct expected_bytes){.bytes = bit_six_two, .size = sizeof(bit_six_two)},
        "BIT INSERT explicit DEFAULT expression explicit NOT NULL"
    );
    failures += expect_bit_cell(
        result,
        2U,
        0U,
        (struct expected_bytes){.bytes = bit_six_ten, .size = sizeof(bit_six_ten)},
        "BIT REPLACE explicit DEFAULT expression b"
    );
    failures += expect_bit_cell(
        result,
        2U,
        1U,
        (struct expected_bytes){.bytes = bit_six_three, .size = sizeof(bit_six_three)},
        "BIT REPLACE explicit DEFAULT expression m"
    );
    failures += expect_bit_cell(
        result,
        2U,
        2U,
        (struct expected_bytes){.is_null = true},
        "BIT REPLACE explicit DEFAULT expression NULL"
    );
    failures += expect_bit_cell(
        result,
        2U,
        3U,
        (struct expected_bytes){.bytes = bit_six_five, .size = sizeof(bit_six_five)},
        "BIT REPLACE explicit DEFAULT expression explicit NOT NULL"
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_statement_ok(database, "CREATE TABLE bit_expr_like LIKE bit_expr");
    failures += expect_dml_ok(database, "INSERT INTO bit_expr_like (id, nn) VALUES (3, b'011')", 1);
    failures += execute_ok(database, "SELECT b, m, n, nn FROM bit_expr_like WHERE id = 3", &result);
    failures += expect_bit_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = bit_six_ten, .size = sizeof(bit_six_ten)},
        "BIT expression default CREATE LIKE b"
    );
    failures += expect_bit_cell(
        result,
        0U,
        1U,
        (struct expected_bytes){.bytes = bit_six_three, .size = sizeof(bit_six_three)},
        "BIT expression default CREATE LIKE m"
    );
    failures += expect_bit_cell(
        result,
        0U,
        2U,
        (struct expected_bytes){.is_null = true},
        "BIT expression default CREATE LIKE NULL"
    );
    failures += expect_bit_cell(
        result,
        0U,
        3U,
        (struct expected_bytes){.bytes = bit_six_three, .size = sizeof(bit_six_three)},
        "BIT expression default CREATE LIKE explicit NOT NULL value"
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_dml_ok(
        database,
        "INSERT INTO bits VALUES "
        "(1, 1, b'1', b'100000', 'A', X'0101', "
        "b'1111111111111111111111111111111111111111111111111111111111111111', "
        "b'101', DEFAULT, DEFAULT, DEFAULT)",
        1
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO bits VALUES "
        "(2, NULL, NULL, NULL, X'00', X'0001', 0, 0, DEFAULT, DEFAULT, DEFAULT)",
        1
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO bits VALUES "
        "(3, b'', FALSE, +5, '1', X'0000', 5, 5, DEFAULT, DEFAULT, DEFAULT)",
        1
    );

    failures += execute_ok(
        database,
        "SELECT b, b1, b6, b8, b9, b64, nn, defi, deft, deff "
        "FROM bits WHERE id = 1",
        &result
    );
    failures += expect_bit_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = one_bit, .size = sizeof(one_bit)},
        "BIT omitted width integer"
    );
    failures += expect_bit_cell(
        result,
        0U,
        1U,
        (struct expected_bytes){.bytes = one_bit, .size = sizeof(one_bit)},
        "BIT(1) bit literal"
    );
    failures += expect_bit_cell(
        result,
        0U,
        2U,
        (struct expected_bytes){.bytes = bit_six_max_bit, .size = sizeof(bit_six_max_bit)},
        "BIT(6) high bit"
    );
    failures += expect_bit_cell(
        result,
        0U,
        3U,
        (struct expected_bytes){.bytes = bit_eight_a, .size = sizeof(bit_eight_a)},
        "BIT(8) string literal"
    );
    failures += expect_bit_cell(
        result,
        0U,
        4U,
        (struct expected_bytes){.bytes = bit_nine_257, .size = sizeof(bit_nine_257)},
        "BIT(9) hex literal"
    );
    failures += expect_bit_cell(
        result,
        0U,
        selected_bit_b64_column,
        (struct expected_bytes){.bytes = bit_sixty_four_max, .size = sizeof(bit_sixty_four_max)},
        "BIT(64) maximum"
    );
    failures += expect_bit_cell(
        result,
        0U,
        selected_bit_nn_column,
        (struct expected_bytes){.bytes = bit_six_five, .size = sizeof(bit_six_five)},
        "BIT NOT NULL literal"
    );
    failures += expect_bit_cell(
        result,
        0U,
        selected_bit_defi_column,
        (struct expected_bytes){.bytes = bit_six_five, .size = sizeof(bit_six_five)},
        "BIT integer default"
    );
    failures += expect_bit_cell(
        result,
        0U,
        selected_bit_deft_column,
        (struct expected_bytes){.bytes = one_bit, .size = sizeof(one_bit)},
        "BIT TRUE default"
    );
    failures += expect_bit_cell(
        result,
        0U,
        selected_bit_deff_column,
        (struct expected_bytes){.bytes = zero_bit, .size = sizeof(zero_bit)},
        "BIT FALSE default"
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT b, b1, b6, b8, b9, b64, nn "
        "FROM bits WHERE id = 2",
        &result
    );
    failures +=
        expect_bit_cell(result, 0U, 0U, (struct expected_bytes){.is_null = true}, "BIT NULL");
    failures +=
        expect_bit_cell(result, 0U, 1U, (struct expected_bytes){.is_null = true}, "BIT(1) NULL");
    failures +=
        expect_bit_cell(result, 0U, 2U, (struct expected_bytes){.is_null = true}, "BIT(6) NULL");
    failures += expect_bit_cell(
        result,
        0U,
        3U,
        (struct expected_bytes){.bytes = zero_bit, .size = sizeof(zero_bit)},
        "BIT(8) zero"
    );
    failures += expect_bit_cell(
        result,
        0U,
        4U,
        (struct expected_bytes){.bytes = bit_nine_one, .size = sizeof(bit_nine_one)},
        "BIT(9) one"
    );
    failures += expect_bit_cell(
        result,
        0U,
        selected_bit_b64_no_default_column,
        (struct expected_bytes){.bytes = bit_sixty_four_zero, .size = sizeof(bit_sixty_four_zero)},
        "BIT(64) zero"
    );
    failures += expect_bit_cell(
        result,
        0U,
        selected_bit_nn_no_default_column,
        (struct expected_bytes){.bytes = zero_bit, .size = sizeof(zero_bit)},
        "BIT NOT NULL zero"
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_dml_ok(database, "UPDATE bits SET b6 = 7 WHERE id = 2", 1);
    failures += expect_dml_ok(database, "UPDATE bits SET b6 = b'111' WHERE id = 2", 0);
    failures += execute_ok(database, "SELECT b6 FROM bits WHERE id = 2", &result);
    failures += expect_bit_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = bit_six_seven, .size = sizeof(bit_six_seven)},
        "BIT UPDATE changed value"
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_statement_ok(database, "CREATE TABLE scalar_source (b6 BIT(6))");
    failures += expect_dml_ok(database, "INSERT INTO scalar_source VALUES (b'101')", 1);
    failures += expect_dml_ok(
        database,
        "UPDATE bits SET b6 = (SELECT b6 FROM scalar_source) WHERE id = 2",
        1
    );
    failures += execute_ok(database, "SELECT b6 FROM bits WHERE id = 2", &result);
    failures += expect_bit_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = bit_six_five, .size = sizeof(bit_six_five)},
        "BIT scalar subquery update"
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_statement_ok(
        database,
        "ALTER TABLE bits ADD COLUMN added BIT(6) NOT NULL "
        "DEFAULT b'101'"
    );
    failures += execute_ok(database, "SELECT added FROM bits WHERE id = 1", &result);
    failures += expect_bit_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = bit_six_five, .size = sizeof(bit_six_five)},
        "ALTER ADD BIT default backfill"
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_statement_ok(
        database,
        "ALTER TABLE bits ADD COLUMN expr_added BIT(6) DEFAULT (1 + 2)"
    );
    failures += execute_ok(database, "SELECT expr_added FROM bits WHERE id = 1", &result);
    failures += expect_bit_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = bit_six_three, .size = sizeof(bit_six_three)},
        "ALTER ADD BIT expression default backfill"
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_statement_ok(database, "CREATE TABLE bit_like LIKE bits");
    failures += expect_dml_ok(database, "INSERT INTO bit_like SELECT * FROM bits WHERE id = 1", 1);
    failures += execute_ok(database, "SELECT b6 FROM bit_like WHERE id = 1", &result);
    failures += expect_bit_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = bit_six_max_bit, .size = sizeof(bit_six_max_bit)},
        "BIT CREATE TABLE LIKE copy"
    );
    mylite_result_free(result);
    result = NULL;
    failures +=
        expect_dml_ok(database, "CREATE TABLE bit_ctas AS SELECT id, b6 FROM bits WHERE id = 1", 1);
    failures += execute_ok(database, "SELECT b6 FROM bit_ctas WHERE id = 1", &result);
    failures += expect_bit_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = bit_six_max_bit, .size = sizeof(bit_six_max_bit)},
        "BIT CREATE TABLE SELECT copy"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_statement_ok(database, "CREATE TABLE bit_insert_select (id INT, b6 BIT(6))");
    failures += expect_dml_ok(
        database,
        "INSERT INTO bit_insert_select SELECT id, b6 FROM bits WHERE id = 1",
        1
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE bit_replace_select (id INT, b6 BIT(6))");
    failures += expect_dml_ok(
        database,
        "REPLACE INTO bit_replace_select SELECT id, b6 FROM bits WHERE id = 1",
        1
    );

    failures += expect_statement_ok(database, "CREATE TABLE pred_bits (id INT, b BIT(3))");
    failures += expect_dml_ok(
        database,
        "INSERT INTO pred_bits VALUES (1,b'001'),(2,b'010'),(3,b'011'),(4,NULL)",
        4
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM pred_bits WHERE b BETWEEN b'001' AND b'011' "
                   "AND b IN (b'001', b'011') ORDER BY id",
            .values = predicate_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "BIT predicate BETWEEN and IN",
        }
    );
    failures += expect_dml_ok(database, "UPDATE pred_bits SET b = b'111' WHERE b <=> b'010'", 1);
    failures += expect_dml_ok(database, "UPDATE pred_bits SET b = b'000' WHERE b IS NULL", 1);
    failures += expect_dml_ok(database, "DELETE FROM pred_bits WHERE b IN (b'001', b'111')", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM pred_bits ORDER BY id",
            .values = remaining_predicate_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "BIT predicate remaining rows",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE order_bits (id INT, b BIT(3), marker BIT(3))");
    failures += expect_dml_ok(
        database,
        "INSERT INTO order_bits VALUES "
        "(1,NULL,0),(2,b'001',0),(3,b'010',0),(4,b'010',0),(5,b'111',0)",
        order_bit_inserted_row_count
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM order_bits ORDER BY b, id DESC",
            .values = bit_multi_order_ids,
            .column_count = 1U,
            .row_count = (size_t)order_bit_inserted_row_count,
            .context = "BIT multi-key order",
        }
    );
    failures +=
        expect_dml_ok(database, "UPDATE order_bits SET marker = b'111' ORDER BY b LIMIT 2", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM order_bits WHERE marker = b'111' ORDER BY id",
            .values = ordered_limited_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "BIT ordered limited update ASC NULL first",
        }
    );
    failures += expect_dml_ok(database, "UPDATE order_bits SET marker = b'000'", 2);
    failures +=
        expect_dml_ok(database, "UPDATE order_bits SET marker = b'111' ORDER BY b DESC LIMIT 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM order_bits WHERE marker = b'111'",
            .values = ordered_desc_id,
            .column_count = 1U,
            .row_count = 1U,
            .context = "BIT ordered limited update DESC NULL last",
        }
    );
    failures += expect_dml_ok(database, "UPDATE order_bits SET marker = b'000'", 1);
    failures += expect_dml_ok(
        database,
        "UPDATE order_bits SET marker = b'111' WHERE b = b'010' ORDER BY b LIMIT 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM order_bits WHERE marker = b'111'",
            .values = tie_update_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "BIT ordered limited duplicate tie count",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read bit preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "BIT file preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen bit file");
    failures += expect_statement_ok(database, "USE app");
    failures += execute_ok(database, "SELECT b6 FROM bits WHERE id = 1", &result);
    failures += expect_bit_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = bit_six_max_bit, .size = sizeof(bit_six_max_bit)},
        "BIT reopen persistence"
    );
    mylite_result_free(result);
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_bit_diagnostics(void) {
    static const unsigned char clipped_bit[] = {0x01U};
    static const unsigned char implicit_bit[] = {0x00U};
    char path[test_path_capacity];
    mylite_result *result = NULL;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open bit diagnostics file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "CREATE TABLE bad_zero (b BIT(0))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_column_size,
            .sqlstate = "HY000",
            .message_part = "Invalid size for column 'b'.",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_wide (b BIT(65))",
        (struct expected_sql_error){
            .code = mysql_error_display_width_out_of_range,
            .sqlstate = "42000",
            .message_part = "Display width out of range for column 'b' (max = 64)",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_empty (b BIT())",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near ')'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_default (b BIT(6) DEFAULT b'1000000')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_expression_default (b BIT(6) DEFAULT (64))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_expression_negative_default (b BIT(6) DEFAULT (-1))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_expression_div_zero (b BIT(6) DEFAULT (1 DIV 0))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_expression_string (b BIT(6) DEFAULT ('1'))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE null_expr_bits (b BIT(1) NOT NULL DEFAULT (NULL))"
    );
    failures += execute_error(
        database,
        "INSERT INTO null_expr_bits () VALUES ()",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "cannot be null",
        }
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE strict_bits (id INT, b1 BIT(1) NOT NULL)");
    failures += execute_error(
        database,
        "INSERT INTO strict_bits VALUES (1, b'10')",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO strict_bits VALUES (1, 18446744073709551616)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO strict_bits VALUES (1, NULL)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "cannot be null",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO strict_bits VALUES (1, b'10')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO strict_bits VALUES (2, NULL)",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
    );
    failures += execute_ok(database, "SELECT b1 FROM strict_bits ORDER BY id", &result);
    failures += expect_bit_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = clipped_bit, .size = sizeof(clipped_bit)},
        "BIT INSERT IGNORE clipped value"
    );
    failures += expect_bit_cell(
        result,
        1U,
        0U,
        (struct expected_bytes){.bytes = implicit_bit, .size = sizeof(implicit_bit)},
        "BIT INSERT IGNORE implicit value"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_statement_ok(database, "CREATE TABLE key_bits (b BIT(6), KEY k_b (b))");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'key_bits' "
                   "AND INDEX_NAME = 'k_b'",
            .values = (const char *const[]){"1"},
            .column_count = 1U,
            .row_count = 1U,
            .context = "BIT secondary index metadata",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE alter_bits (id INT)");
    failures += execute_error(
        database,
        "ALTER TABLE alter_bits MODIFY id BIT(6)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "baseline integer",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_bit_independent_handles(void) {
    static const unsigned char first_value[] = {0x01U};
    static const unsigned char second_value[] = {0x00U};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);
    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first bit handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second bit handle");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (id INT, b BIT(1))");
    failures += expect_dml_ok(first, "INSERT INTO t VALUES (1, b'1')", 1);
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(second, "CREATE TABLE t (id INT, b BIT(1))");
    failures += expect_dml_ok(second, "INSERT INTO t VALUES (1, b'0')", 1);

    failures += execute_ok(first, "SELECT b FROM t WHERE id = 1", &result);
    failures += expect_bit_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = first_value, .size = sizeof(first_value)},
        "first independent BIT state"
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "SELECT b FROM t WHERE id = 1", &result);
    failures += expect_bit_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = second_value, .size = sizeof(second_value)},
        "second independent BIT state"
    );
    mylite_result_free(result);

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s failed: %s\n", sql, mylite_errmsg(database));
        return 1;
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s unexpectedly succeeded\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    return expect_dml_result(
        database,
        sql,
        (struct expected_dml_result){.affected_rows = affected_rows, .warning_count = 0U}
    );
}

static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
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
    size_t value_count = query.column_count * query.row_count;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t value_index = 0U; value_index < value_count; ++value_index) {
        size_t row = value_index / query.column_count;
        size_t column = value_index % query.column_count;

        failures +=
            expect_result_value(result, row, column, query.values[value_index], query.context);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_contains(
    mylite_db *database,
    const char *sql,
    size_t row,
    size_t column,
    const char *needle,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_contains(mylite_result_value_text(result, row, column), needle, context);
    mylite_result_free(result);
    return failures;
}

static int expect_bit_cell(
    const mylite_result *result,
    size_t row,
    size_t column,
    struct expected_bytes expected,
    const char *context
) {
    const unsigned char *actual =
        (const unsigned char *)mylite_result_value_bytes(result, row, column);
    size_t actual_size = mylite_result_value_size(result, row, column);
    int failures = 0;

    if (expected.is_null) {
        failures += expect_true(actual == NULL, context);
        failures += expect_size(actual_size, 0U, context);
        return failures;
    }
    failures += expect_true(actual != NULL, context);
    failures += expect_size(actual_size, expected.size, context);
    if (actual != NULL && actual_size == expected.size) {
        failures += expect_bytes(actual, expected.bytes, expected.size, context);
    }
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
    int written =
        snprintf(path, path_size, "/tmp/mylite_bit_type_%d_%s.mylite", current_process_id(), name);

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
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
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

static int expect_true(int condition, const char *context) {
    if (condition) {
        return 0;
    }
    fprintf(stderr, "%s: expected condition to be true\n", context);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected \"%s\", got \"%s\"\n",
        context,
        expected == NULL ? "(null)" : expected,
        actual == NULL ? "(null)" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected \"%s\" to contain \"%s\"\n",
        context,
        actual == NULL ? "(null)" : actual,
        needle == NULL ? "(null)" : needle
    );
    return 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (size == 0U && expected != NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && memcmp(actual, expected, size) == 0) {
        return 0;
    }
    fprintf(stderr, "%s: byte sequence mismatch\n", context);
    return 1;
}
