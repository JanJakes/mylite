#include "parser_test_support.h"

#include <stddef.h>

static int test_predicate_introduced_literals(void);
static int test_scalar_introduced_literals(void);
static int test_ddl_and_dml_surfaces(void);
static int parse_ok(const char *sql);
static int parse_status(const char *sql, enum mylite_sql_parse_status expected);

int main(void) {
    int failures = 0;

    failures += test_predicate_introduced_literals();
    failures += test_scalar_introduced_literals();
    failures += test_ddl_and_dml_surfaces();

    return failures == 0 ? 0 : 1;
}

static int test_predicate_introduced_literals(void) {
    static const char *const forms[] = {
        "SELECT _latin1'B'",
        "SELECT _utf8mb4 0x4142",
        "SELECT _utf8mb4 b'01100001'",
        "SELECT * FROM t1 WHERE a > _latin1 'B' COLLATE latin1_bin",
        "SELECT * FROM t1 WHERE a <> _latin1 'B' COLLATE latin1_bin",
        "SELECT * FROM t1 WHERE c LIKE _ucs2 0x039C0025 COLLATE ucs2_unicode_ci",
        ("SELECT * FROM t1 WHERE c LIKE _utf16 0x039C0025 COLLATE utf16_general_ci "
         "ORDER BY c"),
        "SELECT * FROM t1 WHERE word LIKE _utf32 x'0000006300000025'",
        "SELECT * FROM t1 WHERE word = BINARY 0xDF",
        "SELECT * FROM t1 WHERE word BETWEEN BINARY 0xDF AND BINARY 0xDF",
        "SELECT * FROM t1 WHERE word LIKE BINARY 0xDF",
        "SELECT * FROM t1 WHERE word IN (_latin1'a', _latin1'b' COLLATE latin1_bin)",
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(forms) / sizeof(forms[0]); ++index) {
        failures += parse_ok(forms[index]);
    }

    return failures;
}

static int test_scalar_introduced_literals(void) {
    static const char *const forms[] = {
        "INSERT INTO t VALUES (_latin1'a' COLLATE latin1_bin)",
        "UPDATE t SET s = _utf8mb4 0x4142 COLLATE utf8mb4_0900_ai_ci",
        "CREATE TABLE t1(a CHAR CHARACTER SET cp1251 DEFAULT _koi8r 0xFF)",
        "CREATE TABLE t2(a CHAR DEFAULT _latin1'a' COLLATE latin1_bin)",
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(forms) / sizeof(forms[0]); ++index) {
        failures += parse_ok(forms[index]);
    }

    return failures;
}

static int test_ddl_and_dml_surfaces(void) {
    static const char *const forms[] = {
        "INSERT INTO t VALUES (_latin1'a' COLLATE latin1_bin), "
        "(_utf8mb4 0x4142 COLLATE utf8mb4_0900_ai_ci)",
        "INSERT INTO t VALUES (\"a\" COLLATE utf8mb4_bin)",
        "CREATE TABLE t1(c1 ENUM(0xc3a6,0xc3b8,b'01100001') CHARSET UTF8MB3)",
        "CREATE TABLE t1(c1 SET('b',0xc3a6,b'01100001') CHARSET UTF8MB3)",
        "ALTER TABLE t1 MODIFY COLUMN c1 SET('b',0xc3a6,0xc3b8) CHARSET UTF8MB4, "
        "ALGORITHM = INPLACE",
        "CREATE TABLE t1 (comment CHAR(32) ASCII NOT NULL, u CHAR(32) UNICODE NOT NULL)",
        "CREATE TABLE t1 (v VARCHAR(10) BINARY ASCII, w VARCHAR(10) UNICODE BINARY)",
        "ALTER TABLE t1 MODIFY v VARCHAR(10) BINARY ASCII",
        "CREATE TABLE t1(a CHAR CHARACTER SET cp1251 DEFAULT _koi8r 0xFF)",
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(forms) / sizeof(forms[0]); ++index) {
        failures += parse_ok(forms[index]);
    }
    failures += parse_status(
        "CREATE TABLE t1(c1 ENUM(_utf8mb3'abc') CHARSET UTF8MB3)",
        MYLITE_SQL_PARSE_SYNTAX_ERROR
    );
    failures += parse_status(
        "CREATE TABLE t1(c1 SET(_latin1'c') CHARSET UTF8MB3)",
        MYLITE_SQL_PARSE_SYNTAX_ERROR
    );

    return failures;
}

static int parse_ok(const char *sql) {
    struct mylite_sql_parse_result result;
    int failures = parser_test_parse_sql(sql, MYLITE_SQL_PARSE_OK, &result);

    if (failures != 0) {
        (void)parser_test_expect_true(0, sql);
    }
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int parse_status(const char *sql, enum mylite_sql_parse_status expected) {
    struct mylite_sql_parse_result result;
    int failures = parser_test_parse_sql(sql, expected, &result);

    mylite_sql_parse_result_deinit(&result);
    return failures;
}
