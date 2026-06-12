#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_collation_utf8mb4_0900_ai_ci_id = 255,
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
    mysql_error_native_function_arity = 1582,
    mysql_string_decimals = 31,
    mysql_utf8mb4_bytes_per_character = 4,
    md5_hex_character_count = 32,
    sha1_hex_character_count = 40,
    sha512_hex_character_count = 128,
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

struct expected_column_metadata {
    enum mylite_result_column_type type;
    uint32_t flags;
    uint32_t charset_id;
    uint32_t collation_id;
    uint64_t display_length;
    uint16_t decimals;
    int nullable;
    const char *context;
};

static int test_no_source_dual_do_and_warnings(void);
static int test_dml_constant_digest_values(void);
static int test_table_backed_digest_projection(void);
static int test_digest_diagnostics(void);
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
static int expect_column_metadata(
    const mylite_result *result,
    size_t column,
    struct expected_column_metadata expected
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_no_source_dual_do_and_warnings();
    failures += test_dml_constant_digest_values();
    failures += test_table_backed_digest_projection();
    failures += test_digest_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_do_and_warnings(void) {
    static const char *const scalar_columns[] = {
        "md5_empty",
        "md5_mysql",
        "md5_null",
        "md5_int",
        "md5_true",
        "md5_false",
        "md5_neg",
        "md5_hex",
        "sha1_empty",
        "sha_alias",
        "sha1_int",
    };
    static const char *const scalar_values[] = {
        "d41d8cd98f00b204e9800998ecf8427e",
        "62a004b95946bb97541afa471dcca73a",
        NULL,
        "202cb962ac59075b964b07152d234b70",
        "c4ca4238a0b923820dcc509a6f75849b",
        "cfcd208495d565ef66e7dff9f98764da",
        "6bb61e3b7bce0931da574d19d1d82c88",
        "900150983cd24fb0d6963f7d28e17f72",
        "da39a3ee5e6b4b0d3255bfef95601890afd80709",
        "a9993e364706816aba3e25717850c26c9cd0d89d",
        "40bd001563085fc35165329ea1ff5c5ecbdbbeef",
    };
    static const char *const sha2_columns[] = {
        "s224",
        "s256",
        "s384",
        "s512",
        "s0",
        "snull",
        "null_input",
        "false_len",
    };
    static const char *const sha2_values[] = {
        "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f",
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f148"
        "98b95b",
        "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d287"
        "7eec2f63b931bd47417a81a538327af927da3e",
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        NULL,
        NULL,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
    };
    static const char *const dual_columns[] = {"md5_value", "sha2_value"};
    static const char *const dual_values[] = {
        "0cc175b9c0f1b6a831c399e269772661",
        "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb",
    };
    static const char *const warning_columns[] = {"bad"};
    static const char *const warning_values[] = {NULL};
    static const char *const status_columns[] = {"warnings", "row_count"};
    static const char *const select_status_values[] = {"1", "-1"};
    static const char *const do_status_values[] = {"1", "0"};
    static const char *const metadata_values[] = {
        "900150983cd24fb0d6963f7d28e17f72",
        "a9993e364706816aba3e25717850c26c9cd0d89d",
        ("ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1"
         "a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"),
    };
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open scalar db");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE guard(id INT)", NULL);
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT MD5('') AS md5_empty, MD5('MySQL') AS md5_mysql, "
                   "MD5(NULL) AS md5_null, MD5(123) AS md5_int, "
                   "MD5(TRUE) AS md5_true, MD5(FALSE) AS md5_false, "
                   "MD5(-1) AS md5_neg, MD5(X'616263') AS md5_hex, "
                   "SHA1('') AS sha1_empty, SHA('abc') AS sha_alias, "
                   "SHA1(123) AS sha1_int",
            .columns = scalar_columns,
            .column_count = sizeof(scalar_columns) / sizeof(scalar_columns[0]),
            .values = scalar_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "digest scalar values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SHA2('',224) AS s224, SHA2('',256) AS s256, "
                   "SHA2('',384) AS s384, SHA2('',512) AS s512, "
                   "SHA2('',0) AS s0, SHA2('',NULL) AS snull, "
                   "SHA2(NULL,256) AS null_input, SHA2('abc',FALSE) AS false_len",
            .columns = sha2_columns,
            .column_count = sizeof(sha2_columns) / sizeof(sha2_columns[0]),
            .values = sha2_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "sha2 scalar values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT MD5 ('a') AS md5_value, SHA2(('a'), 256) AS sha2_value FROM DUAL",
            .columns = dual_columns,
            .column_count = sizeof(dual_columns) / sizeof(dual_columns[0]),
            .values = dual_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "digest dual values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SHA2('',1) AS bad",
            .columns = warning_columns,
            .column_count = sizeof(warning_columns) / sizeof(warning_columns[0]),
            .values = warning_values,
            .row_count = 1U,
            .warning_count = 1U,
            .affected_rows = 0,
            .context = "sha2 invalid length warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count AS warnings, ROW_COUNT() AS row_count",
            .columns = status_columns,
            .column_count = sizeof(status_columns) / sizeof(status_columns[0]),
            .values = select_status_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "sha2 warning status after select",
        }
    );

    failures += execute_ok(database, "DO MD5('abc'), SHA(NULL), SHA2('abc',1)", &result);
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, "digest do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "digest do rows");
        failures += expect_size(mylite_result_warning_count(result), 1U, "digest do warnings");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "digest do affected");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count AS warnings, ROW_COUNT() AS row_count",
            .columns = status_columns,
            .column_count = sizeof(status_columns) / sizeof(status_columns[0]),
            .values = do_status_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "sha2 warning status after do",
        }
    );
    failures += execute_ok(
        database,
        "SELECT MD5('abc') AS md5_value, SHA('abc') AS sha_value, SHA2('abc',512) AS sha2_value",
        &result
    );
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 3U, "digest metadata columns");
        failures += expect_size(mylite_result_row_count(result), 1U, "digest metadata rows");
        failures += expect_result_value(result, 0U, 0U, metadata_values[0], "md5 metadata value");
        failures += expect_result_value(result, 0U, 1U, metadata_values[1], "sha metadata value");
        failures += expect_result_value(result, 0U, 2U, metadata_values[2], "sha2 metadata value");
        failures += expect_column_metadata(
            result,
            0U,
            (struct expected_column_metadata){
                .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
                .flags = 0U,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length =
                    (uint64_t)md5_hex_character_count * mysql_utf8mb4_bytes_per_character,
                .decimals = mysql_string_decimals,
                .nullable = 1,
                .context = "md5 result metadata",
            }
        );
        failures += expect_column_metadata(
            result,
            1U,
            (struct expected_column_metadata){
                .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
                .flags = 0U,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length =
                    (uint64_t)sha1_hex_character_count * mysql_utf8mb4_bytes_per_character,
                .decimals = mysql_string_decimals,
                .nullable = 1,
                .context = "sha result metadata",
            }
        );
        failures += expect_column_metadata(
            result,
            2U,
            (struct expected_column_metadata){
                .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
                .flags = 0U,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length =
                    (uint64_t)sha512_hex_character_count * mysql_utf8mb4_bytes_per_character,
                .decimals = mysql_string_decimals,
                .nullable = 1,
                .context = "sha2 result metadata",
            }
        );
    }
    mylite_result_free(result);
    result = NULL;

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "digest catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "digest sqlite schema generation unchanged"
    );

    mylite_close(database);
    return failures;
}

static int test_dml_constant_digest_values(void) {
    static const char *const columns[] = {"id", "value"};
    static const char *const values[] = {
        "1",
        "900150983cd24fb0d6963f7d28e17f72",
        "2",
        "a9993e364706816aba3e25717850c26c9cd0d89d",
        "3",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "4",
        NULL,
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open dml db");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures +=
        execute_ok(database, "CREATE TABLE digest_values(id INT, value VARCHAR(128))", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO digest_values VALUES "
        "(1, MD5('abc')), (2, SHA('abc')), (3, SHA2('abc',256)), (4, SHA2('abc',NULL))",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, value FROM digest_values ORDER BY id",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "digest dml values",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_digest_projection(void) {
    static const char *const columns[] = {
        "id",
        "md5_v",
        "sha_c",
        "sha1_txt",
        "sha2_b",
        "sha2_vb",
        "sha2_bl",
        "sha2_bi",
        "sha2_null_len",
    };
    static const char *const values[] = {
        "1",
        "900150983cd24fb0d6963f7d28e17f72",
        "a9993e364706816aba3e25717850c26c9cd0d89d",
        "a9993e364706816aba3e25717850c26c9cd0d89d",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7",
        // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
        "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134"
        "c825a7",
        // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
        "3c9909afec25354d551dae21590bb26e38d53f2173b8d3dc3eee4c047e7ab1c1eb8b85103e3be7ba613b31bb5c"
        "9c36214dc9f14a42fd7a2fdb84856bca5c44c2",
        NULL,
        "2",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open table db");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, v VARCHAR(10), c CHAR(3), txt TEXT, b BINARY(3), vb VARBINARY(3), "
        "bl BLOB, bi BIGINT, decv DECIMAL(5,2)"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 'abc', 'abc', 'abc', 'abc', X'616263', X'616263', 123, 12.30), "
        "(2, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, MD5(v) AS md5_v, SHA(c) AS sha_c, SHA1(txt) AS sha1_txt, "
                   "SHA2(b,256) AS sha2_b, SHA2(vb,224) AS sha2_vb, "
                   "SHA2(bl,384) AS sha2_bl, SHA2(bi,512) AS sha2_bi, "
                   "SHA2(v,NULL) AS sha2_null_len FROM t ORDER BY id",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "digest table projection",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_digest_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open diagnostics db");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures +=
        execute_ok(database, "CREATE TABLE t(id INT, v VARCHAR(10), decv DECIMAL(5,2))", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 'abc', 12.30)", NULL);
    failures += execute_error(
        database,
        "SELECT MD5()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'MD5'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SHA('a','b')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'SHA'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SHA1()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'SHA1'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SHA2('abc')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'SHA2'",
        }
    );
    failures += execute_error(
        database,
        "DO SHA2('abc', 256, 512)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'SHA2'",
        }
    );
    failures += execute_error(
        database,
        "SELECT MD5(missing)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT MD5(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT MD5(decv) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "digest functions support only integer, nonbinary string, and binary "
                            "string columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT SHA2(v, TRUE) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "row-scalar SELECT SHA2() supports only hash lengths",
        }
    );

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", sql, mylite_errmsg(database));
        return failures;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return failures;
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
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, expected.context);
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[(row * expected.column_count) + column],
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

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(stderr, "%s: expected NULL at %zu,%zu, got %s\n", context, row, column, actual);
            return 1;
        }
        return 0;
    }
    return expect_text(actual, expected, context);
}

static int expect_column_metadata(
    const mylite_result *result,
    size_t column,
    struct expected_column_metadata expected
) {
    int failures = 0;

    failures +=
        expect_int(mylite_result_column_type(result, column), expected.type, expected.context);
    failures +=
        expect_int64(mylite_result_column_flags(result, column), expected.flags, expected.context);
    failures += expect_int64(
        mylite_result_column_charset_id(result, column),
        expected.charset_id,
        expected.context
    );
    failures += expect_int64(
        mylite_result_column_collation_id(result, column),
        expected.collation_id,
        expected.context
    );
    failures += expect_int64(
        (int64_t)mylite_result_column_display_length(result, column),
        (int64_t)expected.display_length,
        expected.context
    );
    failures += expect_int(
        mylite_result_column_decimals(result, column),
        expected.decimals,
        expected.context
    );
    failures += expect_int(
        mylite_result_column_nullable(result, column),
        expected.nullable,
        expected.context
    );
    return failures;
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
