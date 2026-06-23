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
    mysql_collation_big5_chinese_ci_id = 1,
    mysql_collation_binary_id = 63,
    mysql_collation_utf8mb3_general_ci_id = 33,
    mysql_collation_cp1251_general_ci_id = 51,
    mysql_collation_latin1_swedish_ci_id = 8,
    mysql_collation_utf8mb4_0900_ai_ci_id = 255,
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
    mysql_error_unknown_character_set = 1115,
    row_cast_convert_signed_warning_count = 5,
    row_cast_convert_complement_warning_count = 2,
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
    const enum mylite_result_column_type *types;
    const uint32_t *collation_ids;
    size_t row_count;
    size_t warning_count;
    const char *context;
};

static int test_row_cast_convert_values_metadata_reopen_and_file_safety(void);
static int test_row_cast_convert_warnings(void);
static int test_row_cast_convert_diagnostics(void);
static int open_populated_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
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
static int expect_result_bytes(
    const mylite_result *result,
    size_t row,
    size_t column,
    const void *expected,
    size_t expected_size,
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
static int expect_uint32(uint32_t actual, uint32_t expected, const char *context);
static int expect_column_type(
    enum mylite_result_column_type actual,
    enum mylite_result_column_type expected,
    const char *context
);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_row_cast_convert_values_metadata_reopen_and_file_safety();
    failures += test_row_cast_convert_warnings();
    failures += test_row_cast_convert_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_row_cast_convert_values_metadata_reopen_and_file_safety(void) {
    static const char *const binary_columns[] = {"sb", "nb", "xb", "cb", "ub"};
    static const char *const binary_values[] = {
        "ABC",
        "123",
        NULL,
        "ABC",
        "ABC",
        "3.9",
        "-7",
        "x",
        "3.9",
        "3.9",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };

    static const enum mylite_result_column_type binary_types[] = {
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
    };

    static const uint32_t binary_collations[] = {
        mysql_collation_binary_id,
        mysql_collation_binary_id,
        mysql_collation_binary_id,
        mysql_collation_binary_id,
        mysql_collation_binary_id,
    };
    static const char *const char_columns[] = {"n_char", "s_char", "u_char", "nullable_char"};
    static const char *const char_values[] = {
        "123",
        "ABC",
        "7",
        NULL,
        "-7",
        "3.9",
        "9",
        "x",
        NULL,
        NULL,
        NULL,
        NULL,
    };

    static const enum mylite_result_column_type char_types[] = {
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
    };

    static const uint32_t char_collations[] = {
        mysql_collation_utf8mb4_0900_ai_ci_id,
        mysql_collation_utf8mb4_0900_ai_ci_id,
        mysql_collation_utf8mb4_0900_ai_ci_id,
        mysql_collation_utf8mb4_0900_ai_ci_id,
    };
    static const char *const unsigned_columns[] = {"n_unsigned", "n_unsigned_again"};
    static const char *const unsigned_values[] = {
        "123",
        "123",
        "18446744073709551609",
        "18446744073709551609",
        NULL,
        NULL,
    };

    static const enum mylite_result_column_type unsigned_types[] = {
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
    };

    static const uint32_t unsigned_collations[] = {
        mysql_collation_binary_id,
        mysql_collation_binary_id,
    };
    static const char *const charset_columns[] = {
        "u4",
        "u8",
        "u83",
        "l1",
        "n_l1",
        "null_u4",
    };
    static const char *const charset_values[] = {
        "ABC",
        "ABC",
        "ABC",
        "ABC",
        "123",
        NULL,
        "3.9",
        "3.9",
        "3.9",
        "3.9",
        "-7",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };

    static const enum mylite_result_column_type charset_types[] = {
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
    };

    static const uint32_t charset_collations[] = {
        mysql_collation_utf8mb4_0900_ai_ci_id,
        mysql_collation_utf8mb3_general_ci_id,
        mysql_collation_utf8mb3_general_ci_id,
        mysql_collation_latin1_swedish_ci_id,
        mysql_collation_latin1_swedish_ci_id,
        mysql_collation_utf8mb4_0900_ai_ci_id,
    };
    static const char *const constant_columns[] = {"bin", "chr", "si", "ui"};
    static const char *const constant_values[] = {
        "ABC",
        "ABC",
        "4",
        "1",
        "ABC",
        "ABC",
        "4",
        "1",
        "ABC",
        "ABC",
        "4",
        "1",
    };
    static const char *const nested_columns[] = {"nested"};
    static const char *const nested_values[] = {"ABC", "3.9", NULL};
    static const char *const nested_charset_columns[] = {"cp", "u4", "b5"};
    static const char *const nested_charset_values[] = {
        "abcd",
        "safe",
        "abc",
        "abcd",
        "safe",
        "abc",
        "abcd",
        "safe",
        "abc",
    };

    static const enum mylite_result_column_type nested_charset_types[] = {
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
    };

    static const uint32_t nested_charset_collations[] = {
        mysql_collation_cp1251_general_ci_id,
        mysql_collation_utf8mb4_0900_ai_ci_id,
        mysql_collation_big5_chinese_ci_id,
    };
    static const char *const predicate_order_columns[] = {"v1", "v2"};
    static const char *const predicate_order_values[] = {"alpha", "alpha", "hello", "hello"};

    static const enum mylite_result_column_type predicate_order_types[] = {
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
    };

    static const uint32_t predicate_order_collations[] = {
        mysql_collation_binary_id,
        mysql_collation_utf8mb4_0900_ai_ci_id,
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    mylite_result *byte_result = NULL;
    int failures = 0;

    failures += open_populated_database(&database, "values", path, sizeof(path));
    if (failures != 0) {
        return failures;
    }
    failures +=
        execute_ok(database, "CREATE TABLE wp_convert(val VARCHAR(255), num VARCHAR(255))", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO wp_convert VALUES ('hello', '-42'), ('zero', '0'), ('alpha', '-5')",
        NULL
    );
    mylite_file_preamble_init(expected_preamble);
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CAST(s AS BINARY) AS sb, CAST(n AS BINARY) AS nb, "
                   "CAST(nullable AS BINARY) AS xb, CONVERT(s, BINARY) AS cb, "
                   "CONVERT(s USING BINARY) AS ub FROM t ORDER BY id",
            .columns = binary_columns,
            .column_count = sizeof(binary_columns) / sizeof(binary_columns[0]),
            .values = binary_values,
            .types = binary_types,
            .collation_ids = binary_collations,
            .row_count = 3U,
            .context = "row binary conversions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CAST(n AS CHAR) AS n_char, CAST(s AS CHAR) AS s_char, "
                   "CONVERT(u, CHAR) AS u_char, CAST(nullable AS CHAR) AS nullable_char "
                   "FROM t ORDER BY id",
            .columns = char_columns,
            .column_count = sizeof(char_columns) / sizeof(char_columns[0]),
            .values = char_values,
            .types = char_types,
            .collation_ids = char_collations,
            .row_count = 3U,
            .context = "row char conversions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CAST(n AS UNSIGNED) AS n_unsigned, "
                   "CONVERT(n, UNSIGNED) AS n_unsigned_again FROM t ORDER BY id",
            .columns = unsigned_columns,
            .column_count = sizeof(unsigned_columns) / sizeof(unsigned_columns[0]),
            .values = unsigned_values,
            .types = unsigned_types,
            .collation_ids = unsigned_collations,
            .row_count = 3U,
            .context = "row numeric signed to unsigned conversions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONVERT(s USING utf8mb4) AS u4, CONVERT(s USING utf8) AS u8, "
                   "CONVERT(s USING utf8mb3) AS u83, CONVERT(s USING latin1) AS l1, "
                   "CONVERT(n USING 'latin1') AS n_l1, CONVERT(NULL USING 'utf8mb4') "
                   "AS null_u4 FROM t ORDER BY id",
            .columns = charset_columns,
            .column_count = sizeof(charset_columns) / sizeof(charset_columns[0]),
            .values = charset_values,
            .types = charset_types,
            .collation_ids = charset_collations,
            .row_count = 3U,
            .warning_count = 2U,
            .context = "row charset conversions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONVERT('ABC' USING BINARY) AS bin, CONVERT('ABC', CHAR) AS chr, "
                   "CAST('4' AS SIGNED) AS si, CAST(TRUE AS UNSIGNED) AS ui "
                   "FROM t ORDER BY id",
            .columns = constant_columns,
            .column_count = sizeof(constant_columns) / sizeof(constant_columns[0]),
            .values = constant_values,
            .row_count = 3U,
            .context = "row-backed conversion constants",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CAST(CONVERT(s USING BINARY) AS CHAR) AS nested FROM t ORDER BY id",
            .columns = nested_columns,
            .column_count = sizeof(nested_columns) / sizeof(nested_columns[0]),
            .values = nested_values,
            .types = char_types,
            .collation_ids = char_collations,
            .row_count = 3U,
            .context = "nested row conversion",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONVERT(LEFT(CONVERT('abcdef' USING cp1251), 4) USING cp1251) AS cp, "
                   "CONVERT(CONVERT('safe' USING cp1251) USING utf8mb4) AS u4, "
                   "CONVERT(LEFT(CONVERT('abcdef' USING BINARY), 3) USING big5) AS b5 "
                   "FROM t ORDER BY id",
            .columns = nested_charset_columns,
            .column_count = sizeof(nested_charset_columns) / sizeof(nested_charset_columns[0]),
            .values = nested_charset_values,
            .types = nested_charset_types,
            .collation_ids = nested_charset_collations,
            .row_count = 3U,
            .context = "nested row charset conversion with string slice",
        }
    );
    failures += execute_ok(
        database,
        "SELECT CONVERT(LEFT(CONVERT('\375ord\362ress' USING koi8r), 4) USING koi8r) AS k "
        "FROM t WHERE id = 1",
        &byte_result
    );
    if (byte_result != NULL) {
        static const unsigned char expected_koi8r_bytes[] = {0xfd, 'o', 'r', 'd'};

        failures += expect_result_bytes(
            byte_result,
            0U,
            0U,
            expected_koi8r_bytes,
            sizeof(expected_koi8r_bytes),
            "nested row charset conversion preserves invalid single-byte text"
        );
        mylite_result_free(byte_result);
        byte_result = NULL;
    }
    failures += execute_ok(
        database,
        "SELECT CONVERT(CONVERT('\330ord\320ress' USING cp1251) USING cp1251) AS cp",
        &byte_result
    );
    if (byte_result != NULL) {
        static const unsigned char expected_cp1251_bytes[] = {
            0xd8,
            'o',
            'r',
            'd',
            0xd0,
            'r',
            'e',
            's',
            's',
        };

        failures += expect_result_bytes(
            byte_result,
            0U,
            0U,
            expected_cp1251_bytes,
            sizeof(expected_cp1251_bytes),
            "nested scalar charset conversion preserves invalid single-byte text"
        );
        mylite_result_free(byte_result);
        byte_result = NULL;
    }
    failures += execute_ok(
        database,
        "SELECT CONVERT(LEFT(CONVERT('a\252@ba\252@ba\252@ba\252@b' USING big5), 10) "
        "USING big5) AS b FROM t WHERE id = 1",
        &byte_result
    );
    if (byte_result != NULL) {
        static const unsigned char expected_big5_char_bytes[] = {
            'a',
            0xaa,
            '@',
            'b',
            'a',
            0xaa,
            '@',
            'b',
            'a',
            0xaa,
            '@',
            'b',
            'a',
        };

        failures += expect_result_bytes(
            byte_result,
            0U,
            0U,
            expected_big5_char_bytes,
            sizeof(expected_big5_char_bytes),
            "nested row Big5 charset conversion truncates by characters"
        );
        mylite_result_free(byte_result);
        byte_result = NULL;
    }
    failures += execute_ok(
        database,
        "SELECT CONVERT(LEFT(CONVERT('a\252@ba\252@ba\252@ba\252@b' USING binary), 10) "
        "USING big5) AS b FROM t WHERE id = 1",
        &byte_result
    );
    if (byte_result != NULL) {
        static const unsigned char expected_big5_byte_bytes[] = {
            'a',
            0xaa,
            '@',
            'b',
            'a',
            0xaa,
            '@',
            'b',
            'a',
        };

        failures += expect_result_bytes(
            byte_result,
            0U,
            0U,
            expected_big5_byte_bytes,
            sizeof(expected_big5_byte_bytes),
            "nested row Big5 charset conversion trims partial byte character"
        );
        mylite_result_free(byte_result);
        byte_result = NULL;
    }
    failures += execute_ok(
        database,
        "SELECT CONVERT(LEFT(CONVERT('\375ord\362ress' USING koi8r), 100) USING koi8r) AS k, "
        "CONVERT(LEFT(CONVERT('\330ord\320ress' USING cp1251), 4) USING cp1251) AS c, "
        "CONVERT(LEFT(CONVERT('\340abc' USING hebrew), 2) USING hebrew) AS h, "
        "CONVERT(LEFT(CONVERT('\360abc' USING tis620), 2) USING tis620) AS t",
        &byte_result
    );
    if (byte_result != NULL) {
        static const unsigned char expected_koi8r_scalar_bytes[] = {
            0xfd,
            'o',
            'r',
            'd',
            0xf2,
            'r',
            'e',
            's',
            's',
        };
        static const unsigned char expected_cp1251_scalar_bytes[] = {0xd8, 'o', 'r', 'd'};
        static const unsigned char expected_hebrew_scalar_bytes[] = {0xe0, 'a'};
        static const unsigned char expected_tis620_scalar_bytes[] = {0xf0, 'a'};

        failures += expect_result_bytes(
            byte_result,
            0U,
            0U,
            expected_koi8r_scalar_bytes,
            sizeof(expected_koi8r_scalar_bytes),
            "nested scalar Koi8r charset slice preserves invalid bytes"
        );
        failures += expect_result_bytes(
            byte_result,
            0U,
            1U,
            expected_cp1251_scalar_bytes,
            sizeof(expected_cp1251_scalar_bytes),
            "nested scalar cp1251 charset slice preserves invalid bytes"
        );
        failures += expect_result_bytes(
            byte_result,
            0U,
            2U,
            expected_hebrew_scalar_bytes,
            sizeof(expected_hebrew_scalar_bytes),
            "nested scalar Hebrew charset slice preserves invalid bytes"
        );
        failures += expect_result_bytes(
            byte_result,
            0U,
            3U,
            expected_tis620_scalar_bytes,
            sizeof(expected_tis620_scalar_bytes),
            "nested scalar TIS-620 charset slice preserves invalid bytes"
        );
        mylite_result_free(byte_result);
        byte_result = NULL;
    }
    failures += execute_ok(
        database,
        "SELECT CONVERT(LEFT(CONVERT('a\252@ba\252@ba\252@ba\252@b' USING big5), 10) "
        "USING big5) AS b",
        &byte_result
    );
    if (byte_result != NULL) {
        static const unsigned char expected_big5_scalar_char_bytes[] = {
            'a',
            0xaa,
            '@',
            'b',
            'a',
            0xaa,
            '@',
            'b',
            'a',
            0xaa,
            '@',
            'b',
            'a',
        };

        failures += expect_result_bytes(
            byte_result,
            0U,
            0U,
            expected_big5_scalar_char_bytes,
            sizeof(expected_big5_scalar_char_bytes),
            "nested scalar Big5 charset conversion truncates by characters"
        );
        mylite_result_free(byte_result);
        byte_result = NULL;
    }
    failures += execute_ok(
        database,
        "SELECT CONVERT(LEFT(CONVERT('a\252@ba\252@ba\252@ba\252@b' USING binary), 10) "
        "USING big5) AS b",
        &byte_result
    );
    if (byte_result != NULL) {
        static const unsigned char expected_big5_scalar_byte_bytes[] = {
            'a',
            0xaa,
            '@',
            'b',
            'a',
            0xaa,
            '@',
            'b',
            'a',
        };

        failures += expect_result_bytes(
            byte_result,
            0U,
            0U,
            expected_big5_scalar_byte_bytes,
            sizeof(expected_big5_scalar_byte_bytes),
            "nested scalar Big5 charset conversion trims partial byte character"
        );
        mylite_result_free(byte_result);
        byte_result = NULL;
    }
    failures += execute_ok(
        database,
        "SELECT CONVERT(LEFT(CONVERT('\350\207\252\345\213\225\344\270\213\346\233\270"
        "\343\201\215' USING ujis), 4) USING utf8) AS u",
        &byte_result
    );
    if (byte_result != NULL) {
        static const unsigned char expected_ujis_scalar_bytes[] = {
            0xe8,
            0x87,
            0xaa,
            0xe5,
            0x8b,
            0x95,
            0xe4,
            0xb8,
            0x8b,
            0xe6,
            0x9b,
            0xb8,
        };

        failures += expect_result_bytes(
            byte_result,
            0U,
            0U,
            expected_ujis_scalar_bytes,
            sizeof(expected_ujis_scalar_bytes),
            "nested scalar ujis charset conversion truncates UTF-8 fixture by characters"
        );
        mylite_result_free(byte_result);
        byte_result = NULL;
    }
    failures += execute_ok(
        database,
        "SELECT CONVERT(LEFT(CONVERT('\350\207\252\345\213\225' USING binary), 6) "
        "USING utf8) AS u",
        &byte_result
    );
    if (byte_result != NULL) {
        static const unsigned char expected_utf8_byte_scalar_bytes[] = {
            0xe8,
            0x87,
            0xaa,
            0xe5,
            0x8b,
            0x95,
        };

        failures += expect_result_bytes(
            byte_result,
            0U,
            0U,
            expected_utf8_byte_scalar_bytes,
            sizeof(expected_utf8_byte_scalar_bytes),
            "nested scalar binary conversion preserves UTF-8 byte slice"
        );
        mylite_result_free(byte_result);
        byte_result = NULL;
    }
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONVERT(val, BINARY) AS v1, CONVERT(val USING utf8mb4) AS v2 "
                   "FROM wp_convert WHERE CONVERT(num, SIGNED) < 0 "
                   "ORDER BY CONVERT(val USING utf8mb4)",
            .columns = predicate_order_columns,
            .column_count = sizeof(predicate_order_columns) / sizeof(predicate_order_columns[0]),
            .values = predicate_order_values,
            .types = predicate_order_types,
            .collation_ids = predicate_order_collations,
            .row_count = 2U,
            .context = "row conversion predicate and order",
        }
    );

    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "row conversions leave catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "row conversions leave sqlite schema generation unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read row conversion preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "row conversions leave preamble unchanged"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen row conversion file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CAST(s AS BINARY) AS sb, CONVERT(n, CHAR) AS nc "
                   "FROM t WHERE id = 2",
            .columns = (const char *const[]){"sb", "nc"},
            .column_count = 2U,
            .values = (const char *const[]){"3.9", "-7"},
            .row_count = 1U,
            .context = "row conversions after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_row_cast_convert_warnings(void) {
    static const char *const signed_columns[] = {
        "s_signed",
        "txt_signed",
        "n_signed",
        "u_unsigned",
        "nullable_signed",
    };
    static const char *const signed_values[] = {
        "0",
        "123",
        "123",
        "7",
        NULL,
        "3",
        "0",
        "-7",
        "9",
        "0",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };

    static const enum mylite_result_column_type signed_types[] = {
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
    };

    static const uint32_t signed_collations[] = {
        mysql_collation_binary_id,
        mysql_collation_binary_id,
        mysql_collation_binary_id,
        mysql_collation_binary_id,
        mysql_collation_binary_id,
    };
    static const char *const warning_columns[] = {"Level", "Code", "Message"};
    static const char *const warning_values[] = {
        "Warning",
        "1292",
        "Truncated incorrect INTEGER value: 'ABC'",
        "Warning",
        "1292",
        "Truncated incorrect INTEGER value: '123abc'",
        "Warning",
        "1292",
        "Truncated incorrect INTEGER value: '3.9'",
        "Warning",
        "1292",
        "Truncated incorrect INTEGER value: 'abc'",
        "Warning",
        "1292",
        "Truncated incorrect INTEGER value: 'x'",
    };
    static const char *const count_columns[] = {"@@warning_count"};
    static const char *const count_values[] = {"5"};
    static const char *const complement_columns[] = {"s_signed", "u_unsigned"};
    static const char *const complement_values[] = {
        "-9223372036854775808",
        "18446744073709551615",
    };
    static const char *const complement_warning_values[] = {
        "Warning",
        "1105",
        "Cast to signed converted positive out-of-range integer to its negative complement",
        "Warning",
        "1105",
        "Cast to unsigned converted negative integer to its positive complement",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_populated_database(&database, "warnings", path, sizeof(path));
    if (failures != 0) {
        return failures;
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CAST(s AS SIGNED) AS s_signed, "
                   "CONVERT(txt, SIGNED) AS txt_signed, "
                   "CAST(n AS SIGNED INTEGER) AS n_signed, "
                   "CONVERT(u, UNSIGNED INTEGER) AS u_unsigned, "
                   "CAST(nullable AS SIGNED) AS nullable_signed FROM t ORDER BY id",
            .columns = signed_columns,
            .column_count = sizeof(signed_columns) / sizeof(signed_columns[0]),
            .values = signed_values,
            .types = signed_types,
            .collation_ids = signed_collations,
            .row_count = 3U,
            .warning_count = row_cast_convert_signed_warning_count,
            .context = "row signed conversions and warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = warning_columns,
            .column_count = sizeof(warning_columns) / sizeof(warning_columns[0]),
            .values = warning_values,
            .row_count = row_cast_convert_signed_warning_count,
            .context = "row signed conversion warning rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count",
            .columns = count_columns,
            .column_count = 1U,
            .values = count_values,
            .row_count = 1U,
            .context = "row signed conversion warning count",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE complement_t(id INT PRIMARY KEY, s VARCHAR(32), u VARCHAR(32))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO complement_t VALUES(1, '9223372036854775808', '-1')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CAST(s AS SIGNED) AS s_signed, CAST(u AS UNSIGNED) AS u_unsigned "
                   "FROM complement_t ORDER BY id",
            .columns = complement_columns,
            .column_count = sizeof(complement_columns) / sizeof(complement_columns[0]),
            .values = complement_values,
            .row_count = 1U,
            .warning_count = row_cast_convert_complement_warning_count,
            .context = "row string complement warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = warning_columns,
            .column_count = sizeof(warning_columns) / sizeof(warning_columns[0]),
            .values = complement_warning_values,
            .row_count = row_cast_convert_complement_warning_count,
            .context = "row string complement warning rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_row_cast_convert_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_populated_database(&database, "diagnostics", path, sizeof(path));
    if (failures != 0) {
        return failures;
    }

    failures += execute_error(
        database,
        "SELECT CONVERT(s USING nosuch_charset) AS converted FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_character_set,
            .sqlstate = "42000",
            .message_part = "Unknown character set: 'nosuch_charset'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CAST(n + 1 AS SIGNED) AS converted FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "table-backed arithmetic expression supports only descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT CAST(CAST(s AS SIGNED) AS CHAR) AS converted FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "nested numeric conversions are unsupported",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT(missing, BINARY) AS converted FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing'",
        }
    );
    failures += execute_ok(database, "CREATE TABLE binary_t(v BINARY(2))", NULL);
    failures += execute_error(
        database,
        "SELECT CAST(v AS CHAR) AS converted FROM binary_t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "does not support binary string or BIT columns",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int open_populated_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
) {
    int failures = 0;

    if (make_test_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(
            *out_database,
            "CREATE TABLE t(id INT PRIMARY KEY, s VARCHAR(20), txt TEXT, n INT, "
            "u BIGINT UNSIGNED, nullable VARCHAR(20) NULL)",
            NULL
        );
    }
    if (failures == 0) {
        failures += execute_ok(
            *out_database,
            "INSERT INTO t VALUES (1, 'ABC', '123abc', 123, 7, NULL), "
            "(2, '3.9', 'abc', -7, 9, 'x'), "
            "(3, NULL, NULL, NULL, NULL, NULL)",
            NULL
        );
    }
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
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
        if (expected.types != NULL) {
            failures += expect_column_type(
                mylite_result_column_type(result, column),
                expected.types[column],
                expected.context
            );
        }
        if (expected.collation_ids != NULL) {
            failures += expect_uint32(
                mylite_result_column_charset_id(result, column),
                expected.collation_ids[column],
                expected.context
            );
            failures += expect_uint32(
                mylite_result_column_collation_id(result, column),
                expected.collation_ids[column],
                expected.context
            );
        }
    }
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

    return expect_text(actual, expected, context);
}

static int expect_result_bytes(
    const mylite_result *result,
    size_t row,
    size_t column,
    const void *expected,
    size_t expected_size,
    const char *context
) {
    const void *actual = mylite_result_value_bytes(result, row, column);
    int failures = 0;

    failures += expect_size(mylite_result_value_size(result, row, column), expected_size, context);
    if (failures == 0) {
        failures += expect_bytes(actual, expected, expected_size, context);
    }
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "runtime_row_cast_convert_projection_%s_%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "%s: failed to build path\n", name);
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
    char related[test_path_capacity + path_suffix_capacity];
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

static int expect_uint32(uint32_t actual, uint32_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %u, got %u\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_column_type(
    enum mylite_result_column_type actual,
    enum mylite_result_column_type expected,
    const char *context
) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected column type %d, got %d\n", context, expected, actual);
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
