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

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    mixed_column_count = 11,
    label_column_count = 4,
    parenthesized_column_count = 10,
    operand_column_count = 5,
    cast_binary_column_count = 7,
    cast_binary_label_column_count = 3,
    convert_binary_type_column_count = 7,
    convert_binary_type_label_column_count = 4,
    convert_using_binary_column_count = 2,
    convert_using_binary_label_column_count = 3,
    convert_using_charset_column_count = 7,
    convert_integer_boundary_column_count = 2,
    cast_convert_basic_char_column_count = 7,
    cast_convert_basic_integer_column_count = 7,
    cast_convert_basic_unsigned_column_count = 6,
    cast_convert_basic_boundary_column_count = 4,
    cast_convert_basic_label_column_count = 7,
    cast_convert_basic_status_column_count = 2,
    show_warning_column_count = 3,
    cast_convert_basic_unsigned_warning_count = 4,
    mysql_error_parse = 1064,
    mysql_error_incorrect_parameter_count = 1582,
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
    size_t warning_count;
    int64_t affected_rows;
    const char *context;
};

static int test_scalar_expression_projection_values_and_file_safety(void);
static int test_scalar_expression_projection_unsupported_forms(void);
static int test_scalar_expression_projection_independent_handles(void);
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

    failures += test_scalar_expression_projection_values_and_file_safety();
    failures += test_scalar_expression_projection_unsupported_forms();
    failures += test_scalar_expression_projection_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_expression_projection_values_and_file_safety(void) {
    static const char *const mixed_columns[] = {
        "1",
        "NULL",
        "TRUE",
        "FALSE",
        "2",
        "-3",
        "IF(1,4,5)",
        "IFNULL(NULL,6)",
        "COALESCE(NULL,7)",
        "NULLIF(8,8)",
        "ISNULL(NULL)",
    };
    static const char *const mixed_values[] = {
        "1",
        NULL,
        "1",
        "0",
        "2",
        "-3",
        "4",
        "6",
        "7",
        NULL,
        "1",
    };
    static const char *const label_columns[] = {"one", "if_result", "n", "bare_alias"};
    static const char *const label_values[] = {"1", "5", "9", "0"};
    static const char *const parenthesized_columns[] = {
        "1",
        "NULL",
        "(TRUE)",
        "2",
        "(-3)",
        "(IF(1,4,5))",
        "IFNULL((NULL),(6))",
        "COALESCE((NULL),(7))",
        "NULLIF((8),(8))",
        "ISNULL((NULL))",
    };
    static const char *const parenthesized_values[] = {
        "1",
        NULL,
        "1",
        "2",
        "-3",
        "4",
        "6",
        "7",
        NULL,
        "1",
    };
    static const char *const operand_columns[] = {
        "IF((1),(2),(3))",
        "IFNULL((NULL),(4))",
        "COALESCE((NULL),(NULL),(5))",
        "NULLIF((1),(1))",
        "ISNULL((NULL))",
    };
    static const char *const operand_values[] = {"2", "4", "5", NULL, "1"};
    static const char *const cast_binary_columns[] = {
        "binary",
        "CAST('' AS BINARY)",
        "CAST(NULL AS BINARY)",
        "CAST(123 AS BINARY)",
        "CAST(-7 AS BINARY)",
        "CAST(TRUE AS BINARY)",
        "CAST(FALSE AS BINARY)",
    };
    static const char *const cast_binary_values[] = {"ABC", "", NULL, "123", "-7", "1", "0"};
    static const char *const cast_binary_label_columns[] = {
        "CAST('ABC' AS BINARY)",
        "binary",
        "(CAST('x' AS BINARY))",
    };
    static const char *const cast_binary_label_values[] = {"ABC", "ABC", "x"};
    static const char *const convert_binary_type_columns[] = {
        "binary",
        "CONVERT('', BINARY)",
        "CONVERT(NULL, BINARY)",
        "CONVERT(123, BINARY)",
        "CONVERT(-7, BINARY)",
        "CONVERT(TRUE, BINARY)",
        "CONVERT(FALSE, BINARY)",
    };
    static const char *const convert_binary_type_values[] =
        {"ABC", "", NULL, "123", "-7", "1", "0"};
    static const char *const convert_binary_type_label_columns[] = {
        "binary_value",
        "(CONVERT('x', BINARY))",
        "converted",
        "(CONVERT('y' USING utf8mb4))",
    };
    static const char *const convert_binary_type_label_values[] = {"ABC", "x", "ABC", "y"};
    static const char *const convert_using_binary_columns[] = {
        "binary",
        "CONVERT('xyz' USING BINARY)",
    };
    static const char *const convert_using_binary_values[] = {"ABC", "xyz"};
    static const char *const convert_using_binary_label_columns[] = {
        "CONVERT('ABC' USING BINARY)",
        "binary",
        "(CONVERT('x' USING BINARY))",
    };
    static const char *const convert_using_binary_label_values[] = {"ABC", "ABC", "x"};
    static const char *const convert_using_charset_columns[] = {
        "converted",
        "CONVERT('' USING utf8mb4)",
        "CONVERT(NULL USING utf8mb4)",
        "CONVERT(123 USING utf8mb4)",
        "CONVERT(-7 USING utf8mb4)",
        "CONVERT(TRUE USING utf8mb4)",
        "CONVERT(FALSE USING utf8mb4)",
    };
    static const char *const convert_using_charset_values[] = {
        "ABC",
        "",
        NULL,
        "123",
        "-7",
        "1",
        "0",
    };
    static const char convert_integer_boundary_value[] =
        "123456789012345678901234567890123456789012345678901234567890123456789012345678901";
    static const char *const convert_integer_boundary_columns[] = {
        "binary_boundary",
        "charset_boundary",
    };
    static const char *const convert_integer_boundary_values[] = {
        convert_integer_boundary_value,
        convert_integer_boundary_value,
    };
    static const char *const cast_convert_basic_char_columns[] = {
        "CAST('ABC' AS CHAR)",
        "CONVERT('ABC', CHAR)",
        "CAST(123 AS CHAR)",
        "CONVERT(-7, CHAR)",
        "CAST(NULL AS CHAR)",
        "CAST(TRUE AS CHAR)",
        "CONVERT(FALSE, CHAR)",
    };
    static const char *const cast_convert_basic_char_values[] =
        {"ABC", "ABC", "123", "-7", NULL, "1", "0"};
    static const char *const cast_convert_basic_signed_columns[] = {
        "CAST('ABC' AS SIGNED)",
        "CONVERT('ABC', SIGNED)",
        "CAST('123abc' AS SIGNED)",
        "CAST('  -12x' AS SIGNED)",
        "CAST(NULL AS SIGNED)",
        "CAST(TRUE AS SIGNED)",
        "CAST(FALSE AS SIGNED)",
    };
    static const char *const cast_convert_basic_signed_values[] =
        {"0", "0", "123", "-12", NULL, "1", "0"};
    static const char *const cast_convert_basic_unsigned_columns[] = {
        "CAST('ABC' AS UNSIGNED)",
        "CONVERT('ABC', UNSIGNED)",
        "CAST('-1' AS UNSIGNED)",
        "CAST(-1 AS UNSIGNED)",
        "CAST('18446744073709551615' AS UNSIGNED)",
        "CAST('18446744073709551616' AS UNSIGNED)",
    };
    static const char *const cast_convert_basic_unsigned_values[] = {
        "0",
        "0",
        "18446744073709551615",
        "18446744073709551615",
        "18446744073709551615",
        "18446744073709551615",
    };
    static const char *const cast_convert_basic_boundary_columns[] = {
        "CAST('9223372036854775808' AS SIGNED)",
        "CAST('-9223372036854775809' AS SIGNED)",
        "CAST(18446744073709551616 AS SIGNED)",
        "CAST(-9223372036854775809 AS UNSIGNED)",
    };
    static const char *const cast_convert_basic_boundary_values[] = {
        "-9223372036854775808",
        "-9223372036854775808",
        "9223372036854775807",
        "9223372036854775808",
    };
    static const char *const cast_convert_basic_label_columns[] = {
        "CAST('1' AS SIGNED)",
        "signed_value",
        "(CAST('2' AS UNSIGNED))",
        "CAST(123 AS CHAR)",
        "CONVERT('3', SIGNED)",
        "CONVERT('4', UNSIGNED INTEGER)",
        "CONVERT('ABC', CHAR)",
    };
    static const char *const cast_convert_basic_label_values[] =
        {"1", "1", "2", "123", "3", "4", "ABC"};
    static const char *const cast_convert_basic_status_columns[] = {
        "ROW_COUNT()",
        "@@warning_count"
    };
    static const char *const cast_convert_basic_do_status_values[] = {"0", "2"};
    static const char *const show_warning_columns[] = {"Level", "Code", "Message"};
    static const char *const signed_warning_values[] = {
        "Warning",
        "1292",
        "Truncated incorrect INTEGER value: 'ABC'",
        "Warning",
        "1292",
        "Truncated incorrect INTEGER value: 'ABC'",
        "Warning",
        "1292",
        "Truncated incorrect INTEGER value: '123abc'",
        "Warning",
        "1292",
        "Truncated incorrect INTEGER value: '  -12x'",
    };
    static const char *const unsigned_warning_values[] = {
        "Warning",
        "1292",
        "Truncated incorrect INTEGER value: 'ABC'",
        "Warning",
        "1292",
        "Truncated incorrect INTEGER value: 'ABC'",
        "Warning",
        "1105",
        "Cast to unsigned converted negative integer to its positive complement",
        "Warning",
        "1292",
        "Truncated incorrect INTEGER value: '18446744073709551616'",
    };
    static const char *const boundary_warning_values[] = {
        "Warning",
        "1105",
        "Cast to signed converted positive out-of-range integer to its negative complement",
        "Warning",
        "1292",
        "Truncated incorrect INTEGER value: '-9223372036854775809'",
        "Warning",
        "1292",
        "Truncated incorrect DECIMAL value: '18446744073709551616'",
        "Warning",
        "1292",
        "Truncated incorrect DECIMAL value: '-9223372036854775809'",
    };
    static const char *const row_count_columns[] = {"ROW_COUNT()"};
    static const char *const row_count_values[] = {"-1"};
    static const char *const do_row_count_values[] = {"0"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1, NULL, TRUE, FALSE, +2, -3, IF(1,4,5), "
                   "IFNULL(NULL,6), COALESCE(NULL,7), NULLIF(8,8), ISNULL(NULL)",
            .columns = mixed_columns,
            .column_count = mixed_column_count,
            .values = mixed_values,
            .row_count = 1U,
            .context = "mixed scalar value projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ALL 1 AS one, IF(0,4,5) if_result, NULLIF(9,10) AS n, "
                   "ISNULL(1) bare_alias FROM DUAL",
            .columns = label_columns,
            .column_count = label_column_count,
            .values = label_values,
            .row_count = 1U,
            .context = "dual aliases and all",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT (1), (NULL), (TRUE), (+2), (-3), (IF(1,4,5)), "
                   "IFNULL((NULL),(6)), COALESCE((NULL),(7)), NULLIF((8),(8)), "
                   "ISNULL((NULL))",
            .columns = parenthesized_columns,
            .column_count = parenthesized_column_count,
            .values = parenthesized_values,
            .row_count = 1U,
            .context = "parenthesized scalar values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT IF((1),(2),(3)), IFNULL((NULL),(4)), "
                   "COALESCE((NULL),(NULL),(5)), NULLIF((1),(1)), ISNULL((NULL))",
            .columns = operand_columns,
            .column_count = operand_column_count,
            .values = operand_values,
            .row_count = 1U,
            .context = "parenthesized scalar operands",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CAST('ABC' AS BINARY) AS binary, CAST('' AS BINARY), "
                   "CAST(NULL AS BINARY), CAST(123 AS BINARY), CAST(-7 AS BINARY), "
                   "CAST(TRUE AS BINARY), CAST(FALSE AS BINARY)",
            .columns = cast_binary_columns,
            .column_count = cast_binary_column_count,
            .values = cast_binary_values,
            .row_count = 1U,
            .context = "cast binary scalar values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CAST('ABC' AS BINARY), CAST('ABC' AS BINARY) AS binary, "
                   "(CAST('x' AS BINARY)) FROM DUAL",
            .columns = cast_binary_label_columns,
            .column_count = cast_binary_label_column_count,
            .values = cast_binary_label_values,
            .row_count = 1U,
            .context = "cast binary labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONVERT('ABC', BINARY) AS binary, CONVERT('', BINARY), "
                   "CONVERT(NULL, BINARY), CONVERT(123, BINARY), "
                   "CONVERT(-7, BINARY), CONVERT(TRUE, BINARY), "
                   "CONVERT(FALSE, BINARY)",
            .columns = convert_binary_type_columns,
            .column_count = convert_binary_type_column_count,
            .values = convert_binary_type_values,
            .row_count = 1U,
            .context = "convert binary type scalar values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONVERT('ABC', BINARY) AS binary_value, "
                   "(CONVERT('x', BINARY)), "
                   "CONVERT('ABC' USING utf8mb4) AS converted, "
                   "(CONVERT('y' USING utf8mb4)) FROM DUAL",
            .columns = convert_binary_type_label_columns,
            .column_count = convert_binary_type_label_column_count,
            .values = convert_binary_type_label_values,
            .row_count = 1U,
            .context = "convert binary type and charset labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONVERT('ABC' USING BINARY) AS binary, "
                   "CONVERT('xyz' USING BINARY)",
            .columns = convert_using_binary_columns,
            .column_count = convert_using_binary_column_count,
            .values = convert_using_binary_values,
            .row_count = 1U,
            .context = "convert using binary scalar values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONVERT('ABC' USING BINARY), "
                   "CONVERT('ABC' USING BINARY) AS binary, "
                   "(CONVERT('x' USING BINARY)) FROM DUAL",
            .columns = convert_using_binary_label_columns,
            .column_count = convert_using_binary_label_column_count,
            .values = convert_using_binary_label_values,
            .row_count = 1U,
            .context = "convert using binary labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONVERT('ABC' USING utf8mb4) AS converted, "
                   "CONVERT('' USING utf8mb4), CONVERT(NULL USING utf8mb4), "
                   "CONVERT(123 USING utf8mb4), CONVERT(-7 USING utf8mb4), "
                   "CONVERT(TRUE USING utf8mb4), CONVERT(FALSE USING utf8mb4)",
            .columns = convert_using_charset_columns,
            .column_count = convert_using_charset_column_count,
            .values = convert_using_charset_values,
            .row_count = 1U,
            .context = "convert using utf8mb4 scalar values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "CONVERT("
                   "1234567890123456789012345678901234567890123456789012345678901234567890123456789"
                   "01, BINARY) "
                   "AS binary_boundary, "
                   "CONVERT("
                   "1234567890123456789012345678901234567890123456789012345678901234567890123456789"
                   "01 "
                   "USING UTF8MB4) AS charset_boundary FROM DUAL",
            .columns = convert_integer_boundary_columns,
            .column_count = convert_integer_boundary_column_count,
            .values = convert_integer_boundary_values,
            .row_count = 1U,
            .context = "convert integer boundary and mixed-case charset",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CAST('ABC' AS CHAR), CONVERT('ABC', CHAR), CAST(123 AS CHAR), "
                   "CONVERT(-7, CHAR), CAST(NULL AS CHAR), CAST(TRUE AS CHAR), "
                   "CONVERT(FALSE, CHAR)",
            .columns = cast_convert_basic_char_columns,
            .column_count = cast_convert_basic_char_column_count,
            .values = cast_convert_basic_char_values,
            .row_count = 1U,
            .context = "cast convert char target values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CAST('ABC' AS SIGNED), CONVERT('ABC', SIGNED), "
                   "CAST('123abc' AS SIGNED), CAST('  -12x' AS SIGNED), "
                   "CAST(NULL AS SIGNED), CAST(TRUE AS SIGNED), CAST(FALSE AS SIGNED)",
            .columns = cast_convert_basic_signed_columns,
            .column_count = cast_convert_basic_integer_column_count,
            .values = cast_convert_basic_signed_values,
            .row_count = 1U,
            .warning_count = 4U,
            .context = "cast convert signed target values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = show_warning_columns,
            .column_count = show_warning_column_count,
            .values = signed_warning_values,
            .row_count = 4U,
            .context = "cast convert signed warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CAST('ABC' AS UNSIGNED), CONVERT('ABC', UNSIGNED), "
                   "CAST('-1' AS UNSIGNED), CAST(-1 AS UNSIGNED), "
                   "CAST('18446744073709551615' AS UNSIGNED), "
                   "CAST('18446744073709551616' AS UNSIGNED)",
            .columns = cast_convert_basic_unsigned_columns,
            .column_count = cast_convert_basic_unsigned_column_count,
            .values = cast_convert_basic_unsigned_values,
            .row_count = 1U,
            .warning_count = cast_convert_basic_unsigned_warning_count,
            .context = "cast convert unsigned target values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = show_warning_columns,
            .column_count = show_warning_column_count,
            .values = unsigned_warning_values,
            .row_count = cast_convert_basic_unsigned_warning_count,
            .context = "cast convert unsigned warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CAST('9223372036854775808' AS SIGNED), "
                   "CAST('-9223372036854775809' AS SIGNED), "
                   "CAST(18446744073709551616 AS SIGNED), "
                   "CAST(-9223372036854775809 AS UNSIGNED)",
            .columns = cast_convert_basic_boundary_columns,
            .column_count = cast_convert_basic_boundary_column_count,
            .values = cast_convert_basic_boundary_values,
            .row_count = 1U,
            .warning_count = 4U,
            .context = "cast convert integer boundaries",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = show_warning_columns,
            .column_count = show_warning_column_count,
            .values = boundary_warning_values,
            .row_count = 4U,
            .context = "cast convert boundary warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CAST('1' AS SIGNED), CAST('1' AS SIGNED) AS signed_value, "
                   "(CAST('2' AS UNSIGNED)), CAST(123 AS CHAR), CONVERT('3', SIGNED), "
                   "CONVERT('4', UNSIGNED INTEGER), CONVERT('ABC', CHAR)",
            .columns = cast_convert_basic_label_columns,
            .column_count = cast_convert_basic_label_column_count,
            .values = cast_convert_basic_label_values,
            .row_count = 1U,
            .context = "cast convert labels and target synonyms",
        }
    );

    failures += execute_ok(database, "DO CAST('ABC' AS BINARY), CAST(NULL AS BINARY)", &result);
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, "cast binary do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "cast binary do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "cast binary do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "cast binary do warnings");
    }
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "DO CONVERT('ABC' USING BINARY)", &result);
    if (result != NULL) {
        failures +=
            expect_size(mylite_result_column_count(result), 0U, "convert using binary do columns");
        failures +=
            expect_size(mylite_result_row_count(result), 0U, "convert using binary do rows");
        failures += expect_int64(
            mylite_result_affected_rows(result),
            0,
            "convert using binary do affected"
        );
        failures += expect_size(
            mylite_result_warning_count(result),
            0U,
            "convert using binary do warnings"
        );
    }
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "DO CONVERT('ABC', BINARY), CONVERT(NULL, BINARY), "
        "CONVERT('ABC' USING utf8mb4)",
        &result
    );
    if (result != NULL) {
        failures += expect_size(
            mylite_result_column_count(result),
            0U,
            "convert syntax expansion do columns"
        );
        failures +=
            expect_size(mylite_result_row_count(result), 0U, "convert syntax expansion do rows");
        failures += expect_int64(
            mylite_result_affected_rows(result),
            0,
            "convert syntax expansion do affected"
        );
        failures += expect_size(
            mylite_result_warning_count(result),
            0U,
            "convert syntax expansion do warnings"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .columns = row_count_columns,
            .column_count = 1U,
            .values = do_row_count_values,
            .row_count = 1U,
            .context = "row count after scalar do",
        }
    );
    failures += execute_ok(
        database,
        "DO CONVERT('ABC', SIGNED), CONVERT('-1', UNSIGNED), CAST(NULL AS CHAR)",
        &result
    );
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, "cast convert do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "cast convert do rows");
        failures +=
            expect_int64(mylite_result_affected_rows(result), 0, "cast convert do affected");
        failures +=
            expect_size(mylite_result_warning_count(result), 2U, "cast convert do warnings");
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = cast_convert_basic_status_columns,
            .column_count = cast_convert_basic_status_column_count,
            .values = cast_convert_basic_do_status_values,
            .row_count = 1U,
            .context = "row count and warning count after cast convert do",
        }
    );

    failures += execute_ok(database, "SELECT 1, IF(1,2,3), ISNULL(NULL) FROM DUAL", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .columns = row_count_columns,
            .column_count = 1U,
            .values = row_count_values,
            .row_count = 1U,
            .context = "row count after mixed scalar select",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "mixed scalar select leaves catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "mixed scalar select leaves sqlite schema generation unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read mixed scalar preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "mixed scalar select leaves preamble unchanged"
    );

    remove_related_files(path);
    return failures;
}

static int test_scalar_expression_projection_unsupported_forms(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open unsupported file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE t(id INT NOT NULL)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1)", NULL);

    failures += execute_error(
        database,
        "SELECT IF(1,2), 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1, ISNULL()",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'ISNULL'",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1.0, IF(1,2,3)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT scalar projection supports only session scalar values",
        }
    );
    failures += execute_error(
        database,
        "SELECT CAST(1 + 2 AS BINARY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "CAST AS BINARY supports only string, integer, boolean, and NULL values",
        }
    );
    failures += execute_error(
        database,
        "SELECT CAST(X'41' AS BINARY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "CAST AS BINARY supports only string, integer, boolean, and NULL values",
        }
    );
    failures += execute_error(
        database,
        "SELECT CAST((SELECT 1) AS BINARY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "CAST AS BINARY supports only string, integer, boolean, and NULL values",
        }
    );
    failures += execute_error(
        database,
        "SELECT CAST('ABC' AS BINARY(5))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT CAST('ABC' AS CHAR(2))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT CAST('1' AS INT)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT CAST(1 + 2 AS SIGNED)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "CAST AS SIGNED supports only string, integer, boolean, and NULL values",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT(123 USING BINARY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CONVERT USING BINARY supports only string literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT(NULL USING BINARY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CONVERT USING BINARY supports only string literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT(TRUE USING BINARY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CONVERT USING BINARY supports only string literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT(X'41' USING BINARY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CONVERT USING BINARY supports only string literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT((SELECT 1) USING BINARY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CONVERT USING BINARY supports only string literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT('ABC', BINARY(5))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT('ABC', BINARY, 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT('ABC', CHAR(2))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT('1', INTEGER)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT(1 + 2, UNSIGNED)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "CONVERT(value, UNSIGNED) supports only string, integer, boolean, and NULL values",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT('ABC' USING latin1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CONVERT USING charset supports only utf8mb4",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT('ABC' USING 'utf8mb4')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CONVERT USING charset supports only utf8mb4",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT(1 + 2, BINARY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "CONVERT(value, BINARY) supports only string, integer, boolean, and NULL values",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT(1 + 2 USING utf8mb4)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "CONVERT USING utf8mb4 supports only string, integer, boolean, and NULL values",
        }
    );
    failures += execute_error(
        database,
        "SELECT "
        "CONVERT("
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012, "
        "BINARY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "SELECT literal projection supports at most 81 significant decimal digits",
        }
    );
    failures += execute_error(
        database,
        "SELECT "
        "CONVERT("
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012 "
        "USING utf8mb4)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "SELECT literal projection supports at most 81 significant decimal digits",
        }
    );
    failures += execute_error(
        database,
        "SELECT BINARY 'ABC'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT CAST(id AS BINARY) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT('ABC' USING BINARY) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT('ABC', BINARY) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT CAST(id AS SIGNED) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT('ABC', CHAR) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT('ABC' USING utf8mb4) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += execute_error(
        database,
        "DO CAST(1 + 2 AS BINARY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "CAST AS BINARY supports only string, integer, boolean, and NULL values",
        }
    );
    failures += execute_error(
        database,
        "DO CAST(1 + 2 AS SIGNED)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "CAST AS SIGNED supports only string, integer, boolean, and NULL values",
        }
    );
    failures += execute_error(
        database,
        "DO CONVERT(123 USING BINARY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CONVERT USING BINARY supports only string literals",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t SET id = CAST(1 AS BINARY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(1,(2+3),4)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(VERSION(),1,2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF('x',2,3) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "IF() row conditions support only integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1, IF(1,2,3) WHERE TRUE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'WHERE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1, IF(1,2,3) LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'LIMIT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1, IF(1,2,3) ORDER BY 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'ORDER'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_scalar_expression_projection_independent_handles(void) {
    static const char *const first_columns[] = {
        "first_result",
        "ISNULL(NULL)",
        "CAST('A' AS BINARY)",
        "CONVERT('first' USING BINARY)",
    };
    static const char *const first_values[] = {"2", "1", "A", "first"};
    static const char *const second_columns[] = {
        "second_result",
        "NULLIF(1,1)",
        "CAST('B' AS BINARY)",
        "CONVERT('second' USING BINARY)",
    };
    static const char *const second_values[] = {"4", NULL, "B", "second"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first memory handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second memory handle");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT IF(1,2,3) AS first_result, ISNULL(NULL), CAST('A' AS BINARY), "
                   "CONVERT('first' USING BINARY)",
            .columns = first_columns,
            .column_count = 4U,
            .values = first_values,
            .row_count = 1U,
            .context = "first handle mixed scalar",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT IFNULL(NULL,4) AS second_result, NULLIF(1,1), "
                   "CAST('B' AS BINARY), CONVERT('second' USING BINARY)",
            .columns = second_columns,
            .column_count = 4U,
            .values = second_values,
            .row_count = 1U,
            .context = "second handle mixed scalar",
        }
    );

    mylite_close(second);
    mylite_close(first);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
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

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
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
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);

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

    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-scalar-expression-projection-%s-%d.mylite",
        name,
        current_process_id()
    );

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

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_size = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    read_size = fread(buffer, 1U, size, file);
    fclose(file);

    return read_size == size ? 0 : 1;
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
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
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
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected %s, got %s\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing %s, got %s\n",
            context,
            needle == NULL ? "(null)" : needle,
            actual == NULL ? "(null)" : actual
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
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }
    return 0;
}
