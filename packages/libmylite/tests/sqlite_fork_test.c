#include <mylite_fork/mylite_sqlite_fork.h>

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdio.h>
#include <string.h>

struct expected_text_row {
    const char *sql;
    const char *expected;
    const char *context;
};

struct expected_mylite_rows {
    const char *sql;
    const char *const *values;
    int column_count;
    int row_count;
    const char *context;
};

struct expected_sqlite_error {
    const char *sql;
    const char *message_fragment;
    const char *context;
};

struct expected_fork_condition {
    unsigned int mysql_errno;
    const char *sqlstate;
    const char *context;
};

static int test_registered_functions(void);

static int test_mysql_collations(void);

static int test_native_type_coercion(void);

static int test_native_binary_type_coercion(void);

static int test_native_decimal_type_coercion(void);

static int test_native_temporal_type_coercion(void);

static int test_wordpress_like_crud(void);

static int test_mylite_wordpress_like_crud(void);

static int test_mylite_basic_type_coercion(void);

static int test_mylite_binary_type_coercion(void);

static int test_mylite_decimal_type_coercion(void);

static int test_mylite_temporal_type_coercion(void);

static int test_mylite_collation_unique_semantics(void);

static int open_configured_database(sqlite3 **out_database);

static int exec_sql(sqlite3 *database, const char *sql, const char *context);

static int exec_mylite_sql(mylite_db *database, const char *sql, const char *context);

static int expect_sqlite_exec_error(sqlite3 *database, struct expected_sqlite_error expectation);

static int expect_fork_condition(sqlite3 *database, struct expected_fork_condition expectation);

static int expect_text(sqlite3 *database, struct expected_text_row expectation);

static int expect_mylite_rows(mylite_db *database, struct expected_mylite_rows expectation);

static int expect_mylite_error_condition(
    mylite_db *database,
    unsigned int mysql_errno,
    const char *context
);

static int expect_int64(
    sqlite3 *database,
    const char *sql,
    sqlite3_int64 expected,
    const char *context
);

static int prepare_single_column(
    sqlite3 *database,
    const char *sql,
    sqlite3_stmt **out_statement,
    const char *context
);

static int finish_single_row(sqlite3_stmt *statement, const char *context);

static int expect_sqlite_ok(int rc, sqlite3 *database, const char *context);

static int expect_mylite_ok(int status, mylite_db *database, const char *context);

static int expect_mylite_status(int status, int expected, mylite_db *database, const char *context);

static int expect_mylite_sql_status(
    mylite_db *database,
    const char *sql,
    int expected,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_registered_functions();
    failures += test_mysql_collations();
    failures += test_native_type_coercion();
    failures += test_native_binary_type_coercion();
    failures += test_native_decimal_type_coercion();
    failures += test_native_temporal_type_coercion();
    failures += test_wordpress_like_crud();
    failures += test_mylite_wordpress_like_crud();
    failures += test_mylite_basic_type_coercion();
    failures += test_mylite_binary_type_coercion();
    failures += test_mylite_decimal_type_coercion();
    failures += test_mylite_temporal_type_coercion();
    failures += test_mylite_collation_unique_semantics();

    return failures == 0 ? 0 : 1;
}

static int test_registered_functions(void) {
    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT CONCAT('wp_', 'posts')",
            .expected = "wp_posts",
            .context = "CONCAT joins non-null arguments",
        }
    );
    failures +=
        expect_int64(database, "SELECT LENGTH(CAST(X'C5BE' AS TEXT))", 2, "LENGTH counts bytes");
    failures += expect_int64(
        database,
        "SELECT CHAR_LENGTH(CAST(X'C5BE' AS TEXT))",
        1,
        "CHAR_LENGTH counts UTF-8 characters"
    );
    failures += expect_int64(
        database,
        "SELECT CONCAT('a', NULL) IS NULL",
        1,
        "CONCAT returns NULL when an argument is NULL"
    );

    sqlite3_close(database);
    return failures;
}

static int test_mysql_collations(void) {
    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += exec_sql(
        database,
        "CREATE TABLE collated_names("
        "name TEXT COLLATE utf8mb4_unicode_ci,"
        "binary_name TEXT COLLATE utf8mb4_bin"
        ")",
        "create collation fixture"
    );
    failures += exec_sql(
        database,
        "INSERT INTO collated_names VALUES ('Hello', 'Hello')",
        "insert collation fixture"
    );
    failures += expect_int64(
        database,
        "SELECT COUNT(*) FROM collated_names WHERE name = 'hello'",
        1,
        "case-insensitive MySQL collation is visible to SQLite predicates"
    );
    failures += expect_int64(
        database,
        "SELECT COUNT(*) FROM collated_names WHERE binary_name = 'hello'",
        0,
        "binary MySQL collation remains byte-sensitive"
    );
    failures += exec_sql(
        database,
        "CREATE TABLE collation_pad_unique(value TEXT COLLATE utf8mb4_unicode_ci UNIQUE)",
        "create direct PAD SPACE unique fixture"
    );
    failures += exec_sql(
        database,
        "INSERT INTO collation_pad_unique VALUES ('trail')",
        "insert direct PAD SPACE unique seed"
    );
    failures += exec_sql(
        database,
        "INSERT OR IGNORE INTO collation_pad_unique VALUES ('trail ')",
        "insert direct PAD SPACE trailing-space variant"
    );
    failures += expect_int64(
        database,
        "SELECT COUNT(*) FROM collation_pad_unique",
        1,
        "SQLite collation API preserves PAD SPACE uniqueness"
    );
    failures += exec_sql(
        database,
        "CREATE TABLE collation_no_pad_unique(value TEXT COLLATE utf8mb4_0900_ai_ci UNIQUE)",
        "create direct NO PAD unique fixture"
    );
    failures += exec_sql(
        database,
        "INSERT INTO collation_no_pad_unique VALUES ('trail')",
        "insert direct NO PAD unique seed"
    );
    failures += exec_sql(
        database,
        "INSERT OR IGNORE INTO collation_no_pad_unique VALUES ('trail ')",
        "insert direct NO PAD trailing-space variant"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT COUNT(*) || ':' || group_concat(value || ':' || length(value), '|') "
                   "FROM (SELECT value FROM collation_no_pad_unique ORDER BY rowid)",
            .expected = "2:trail:5|trail :6",
            .context = "SQLite collation API preserves NO PAD uniqueness",
        }
    );
    failures += exec_sql(
        database,
        "CREATE TABLE collation_prefix_ci(slug TEXT COLLATE utf8mb4_unicode_ci)",
        "create direct case-insensitive prefix fixture"
    );
    failures += exec_sql(
        database,
        "CREATE UNIQUE INDEX uq_direct_slug4_ci ON collation_prefix_ci("
        "substr(slug,1,4) COLLATE utf8mb4_unicode_ci)",
        "create direct case-insensitive prefix index"
    );
    failures += exec_sql(
        database,
        "INSERT INTO collation_prefix_ci VALUES ('Post Alpha')",
        "insert direct case-insensitive prefix seed"
    );
    failures += exec_sql(
        database,
        "INSERT OR IGNORE INTO collation_prefix_ci VALUES ('post Beta')",
        "insert direct case-insensitive prefix duplicate"
    );
    failures += expect_int64(
        database,
        "SELECT COUNT(*) FROM collation_prefix_ci",
        1,
        "SQLite expression indexes can preserve MySQL prefix collation"
    );
    failures += exec_sql(
        database,
        "CREATE TABLE collation_prefix_bin(slug TEXT COLLATE utf8mb4_bin)",
        "create direct binary prefix fixture"
    );
    failures += exec_sql(
        database,
        "CREATE UNIQUE INDEX uq_direct_slug4_bin ON collation_prefix_bin("
        "substr(slug,1,4) COLLATE utf8mb4_bin)",
        "create direct binary prefix index"
    );
    failures += exec_sql(
        database,
        "INSERT INTO collation_prefix_bin VALUES ('Post Alpha')",
        "insert direct binary prefix seed"
    );
    failures += exec_sql(
        database,
        "INSERT OR IGNORE INTO collation_prefix_bin VALUES ('post Beta')",
        "insert direct binary prefix distinct value"
    );
    failures += expect_int64(
        database,
        "SELECT COUNT(*) FROM collation_prefix_bin",
        2,
        "SQLite expression indexes keep binary prefix collation byte-sensitive"
    );

    sqlite3_close(database);
    return failures;
}

static int test_native_type_coercion(void) {
    enum {
        expected_unsigned_integer = 42,
        legacy_update_type_minimum = 0,
        legacy_update_type_maximum = 10,
        legacy_update_out_of_range_error = 1264,
    };

    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += expect_int64(
        database,
        "SELECT _mylite_coerce_signed_integer('3.5', -128, 127)",
        4,
        "signed integer coercion rounds positive half"
    );
    failures += expect_int64(
        database,
        "SELECT _mylite_coerce_signed_integer('-3.5', -128, 127)",
        -4,
        "signed integer coercion rounds negative half"
    );
    failures += expect_int64(
        database,
        "SELECT _mylite_coerce_unsigned_integer('42', 4294967295)",
        expected_unsigned_integer,
        "unsigned integer coercion accepts numeric text"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT _mylite_coerce_varchar(123, 4)",
            .expected = "123",
            .context = "varchar coercion converts numeric values to text",
        }
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT _mylite_coerce_varchar('éé', 2)",
            .expected = "éé",
            .context = "varchar coercion counts UTF-8 characters",
        }
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT _mylite_coerce_double('3.25') || ''",
            .expected = "3.25",
            .context = "double coercion accepts numeric text",
        }
    );

    failures += exec_sql(
        database,
        "CREATE TABLE legacy_update(id INTEGER PRIMARY KEY, legacy TEXT, changed INTEGER)",
        "create legacy update type-check fixture"
    );
    failures += exec_sql(
        database,
        "INSERT INTO legacy_update VALUES (1, 'bad', 1)",
        "insert legacy update fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "legacy_update",
            "legacy",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_SIGNED_INTEGER,
                .integer_minimum = legacy_update_type_minimum,
                .integer_maximum = legacy_update_type_maximum,
            }
        ),
        database,
        "set legacy descriptor"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "legacy_update",
            "changed",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_SIGNED_INTEGER,
                .integer_minimum = legacy_update_type_minimum,
                .integer_maximum = legacy_update_type_maximum,
            }
        ),
        database,
        "set changed descriptor"
    );
    failures += exec_sql(
        database,
        "UPDATE legacy_update SET changed = '2' WHERE id = 1",
        "update checks assigned MyLite descriptor only"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT legacy || ':' || changed FROM legacy_update",
            .expected = "bad:2",
            .context = "update preserves unchecked legacy value",
        }
    );

    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "SELECT _mylite_coerce_signed_integer(128, -128, 127)",
            .message_fragment = "integer value is out of range",
            .context = "signed integer coercion rejects out-of-range value",
        }
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "SELECT _mylite_coerce_unsigned_integer(-1, 4294967295)",
            .message_fragment = "unsigned integer value is out of range",
            .context = "unsigned integer coercion rejects negative value",
        }
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "SELECT _mylite_coerce_varchar('abcde', 4)",
            .message_fragment = "varchar value is too long",
            .context = "varchar coercion rejects over-length text",
        }
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "SELECT _mylite_coerce_double('bad')",
            .message_fragment = "invalid double value",
            .context = "double coercion rejects invalid text",
        }
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "UPDATE legacy_update SET legacy = 'bad' WHERE id = 1",
            .message_fragment = "integer value is out of range",
            .context = "update checks explicitly assigned legacy value",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = legacy_update_out_of_range_error,
            .sqlstate = "22003",
            .context = "assigned legacy value exposes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear fork condition"
    );

    sqlite3_close(database);
    return failures;
}

static int test_native_binary_type_coercion(void) {
    enum {
        binary_length = 3,
        varbinary_length = 4,
        binary_string_too_long_error = 1406,
    };

    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += exec_sql(
        database,
        "CREATE TABLE binary_direct(id INTEGER PRIMARY KEY, fixed BLOB, variable BLOB)",
        "create direct binary descriptor fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "binary_direct",
            "fixed",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_BINARY,
                .byte_maximum_length = binary_length,
            }
        ),
        database,
        "set direct fixed binary descriptor"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "binary_direct",
            "variable",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_VARBINARY,
                .byte_maximum_length = varbinary_length,
            }
        ),
        database,
        "set direct varbinary descriptor"
    );
    failures += exec_sql(
        database,
        "INSERT INTO binary_direct VALUES (1, 'a', X'C3A9'), (2, 65, 1234)",
        "insert direct binary descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat(id || ':' || hex(fixed) || ':' || "
                   "length(fixed) || ':' || hex(variable) || ':' || "
                   "length(variable), '|') FROM ("
                   "SELECT id, fixed, variable FROM binary_direct ORDER BY id)",
            .expected = "1:610000:3:C3A9:2|2:363500:3:31323334:4",
            .context = "direct binary descriptors coerce and pad stored bytes",
        }
    );
    failures += exec_sql(
        database,
        "UPDATE binary_direct SET fixed = 'xy', variable = 'z' WHERE id = 2",
        "update direct binary descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT hex(fixed) || ':' || length(fixed) || ':' || "
                   "hex(variable) || ':' || length(variable) "
                   "FROM binary_direct WHERE id = 2",
            .expected = "787900:3:7A:1",
            .context = "direct binary update coerces assigned values",
        }
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO binary_direct VALUES (3, X'61626364', X'6F6B')",
            .message_fragment = "binary value is too long",
            .context = "direct binary descriptor rejects over-length fixed value",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = binary_string_too_long_error,
            .sqlstate = "22001",
            .context = "over-length fixed binary exposes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear fixed binary fork condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO binary_direct VALUES (3, X'6F6B', X'6162636465')",
            .message_fragment = "binary value is too long",
            .context = "direct varbinary descriptor rejects over-length value",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = binary_string_too_long_error,
            .sqlstate = "22001",
            .context = "over-length varbinary exposes MySQL condition",
        }
    );

    sqlite3_close(database);
    return failures;
}

static int test_native_decimal_type_coercion(void) {
    enum {
        decimal_precision = 5,
        decimal_scale = 2,
        whole_precision = 4,
        whole_scale = 0,
        decimal_out_of_range_error = 1264,
        invalid_decimal_error = 1366,
    };

    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += exec_sql(
        database,
        "CREATE TABLE decimal_direct("
        "id INTEGER PRIMARY KEY, amount TEXT, whole TEXT, unsigned_amount TEXT)",
        "create direct decimal descriptor fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "decimal_direct",
            "amount",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_DECIMAL,
                .numeric_precision = decimal_precision,
                .numeric_scale = decimal_scale,
            }
        ),
        database,
        "set direct decimal descriptor"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "decimal_direct",
            "whole",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_DECIMAL,
                .numeric_precision = whole_precision,
                .numeric_scale = whole_scale,
            }
        ),
        database,
        "set direct whole decimal descriptor"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "decimal_direct",
            "unsigned_amount",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_DECIMAL,
                .numeric_precision = decimal_precision,
                .numeric_scale = decimal_scale,
                .flags = MYLITE_SQLITE_FORK_COLUMN_TYPE_UNSIGNED,
            }
        ),
        database,
        "set direct unsigned decimal descriptor"
    );
    failures += exec_sql(
        database,
        "INSERT INTO decimal_direct VALUES "
        "(1, '1.234', '12.5', '0'),"
        "(2, '-1.235', '-12.5', '9.995'),"
        "(3, '001.2', '9.5', '99.999')",
        "insert direct decimal descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat(id || ':' || amount || ':' || whole || ':' || "
                   "unsigned_amount, '|') FROM ("
                   "SELECT id, amount, whole, unsigned_amount FROM decimal_direct ORDER BY id)",
            .expected = "1:1.23:13:0.00|2:-1.24:-13:10.00|3:1.20:10:100.00",
            .context = "direct decimal descriptors round and normalize stored text",
        }
    );
    failures += exec_sql(
        database,
        "UPDATE decimal_direct SET amount = 999.994, whole = -0.5, "
        "unsigned_amount = 0.004 WHERE id = 1",
        "update direct decimal descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT amount || ':' || whole || ':' || unsigned_amount "
                   "FROM decimal_direct WHERE id = 1",
            .expected = "999.99:-1:0.00",
            .context = "direct decimal update coerces assigned values",
        }
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO decimal_direct VALUES (4, '999.995', '1', '1')",
            .message_fragment = "decimal value is out of range",
            .context = "direct decimal descriptor rejects post-round overflow",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = decimal_out_of_range_error,
            .sqlstate = "22003",
            .context = "post-round decimal overflow exposes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear decimal overflow fork condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO decimal_direct VALUES (4, '12abc', '1', '1')",
            .message_fragment = "invalid decimal value",
            .context = "direct decimal descriptor rejects invalid text",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = invalid_decimal_error,
            .sqlstate = "HY000",
            .context = "invalid decimal exposes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear invalid decimal fork condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO decimal_direct VALUES (4, '1', '1', '-0.01')",
            .message_fragment = "decimal value is out of range",
            .context = "direct decimal descriptor rejects unsigned negative value",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = decimal_out_of_range_error,
            .sqlstate = "22003",
            .context = "unsigned negative decimal exposes MySQL condition",
        }
    );

    sqlite3_close(database);
    return failures;
}

static int test_native_temporal_type_coercion(void) {
    enum {
        invalid_temporal_error = 1292,
        datetime_overflow_error = 1441,
        datetime_millisecond_precision = 3,
        datetime_microsecond_precision = 6,
    };

    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += exec_sql(
        database,
        "CREATE TABLE temporal_direct("
        "id INTEGER PRIMARY KEY, d TEXT, dt TEXT, dt3 TEXT, dt6 TEXT)",
        "create direct temporal descriptor fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "temporal_direct",
            "d",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_DATE,
            }
        ),
        database,
        "set direct date descriptor"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "temporal_direct",
            "dt",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_DATETIME,
            }
        ),
        database,
        "set direct datetime descriptor"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "temporal_direct",
            "dt3",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_DATETIME,
                .datetime_precision = datetime_millisecond_precision,
            }
        ),
        database,
        "set direct datetime(3) descriptor"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "temporal_direct",
            "dt6",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_DATETIME,
                .datetime_precision = datetime_microsecond_precision,
            }
        ),
        database,
        "set direct datetime(6) descriptor"
    );
    failures += exec_sql(
        database,
        "INSERT INTO temporal_direct VALUES "
        "(1, '2024-02-29', '2024-02-29 12:34:56', "
        "'2024-02-29 12:34:56.7896', '2024-02-29 12:34:56.1234567'),"
        "(2, '20240229', '20240229123456', '20240229123456.1', "
        "'20240229123456.123'),"
        "(3, 20240229, 20240229123456, '20240229123456.789', "
        "'20240229123456.123456')",
        "insert direct temporal descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat(id || ':' || d || ':' || dt || ':' || dt3 || ':' || "
                   "dt6, '|') FROM ("
                   "SELECT id, d, dt, dt3, dt6 FROM temporal_direct ORDER BY id)",
            .expected = "1:2024-02-29:2024-02-29 12:34:56:"
                        "2024-02-29 12:34:56.790:2024-02-29 12:34:56.123457|"
                        "2:2024-02-29:2024-02-29 12:34:56:"
                        "2024-02-29 12:34:56.100:2024-02-29 12:34:56.123000|"
                        "3:2024-02-29:2024-02-29 12:34:56:"
                        "2024-02-29 12:34:56.789:2024-02-29 12:34:56.123456",
            .context = "direct temporal descriptors normalize stored text",
        }
    );
    failures += exec_sql(
        database,
        "UPDATE temporal_direct SET d = '2024-03-01', dt = '2024-03-01', "
        "dt3 = '2024-03-01 01:02:03.9999', "
        "dt6 = '2024-03-01 01:02:03.0000019' WHERE id = 1",
        "update direct temporal descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT d || ':' || dt || ':' || dt3 || ':' || dt6 "
                   "FROM temporal_direct WHERE id = 1",
            .expected = "2024-03-01:2024-03-01 00:00:00:"
                        "2024-03-01 01:02:04.000:2024-03-01 01:02:03.000002",
            .context = "direct temporal update coerces assigned values",
        }
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO temporal_direct VALUES "
                   "(4, '2024-02-30', '2024-01-01', '2024-01-01', '2024-01-01')",
            .message_fragment = "invalid date value",
            .context = "direct temporal descriptor rejects invalid dates",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = invalid_temporal_error,
            .sqlstate = "22007",
            .context = "invalid date exposes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear invalid date fork condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO temporal_direct VALUES "
                   "(4, '0000-00-00', '2024-01-01', '2024-01-01', '2024-01-01')",
            .message_fragment = "invalid date value",
            .context = "direct temporal descriptor rejects zero date without allow flag",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = invalid_temporal_error,
            .sqlstate = "22007",
            .context = "zero date exposes MySQL condition without allow flag",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear zero date fork condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO temporal_direct VALUES "
                   "(4, '2024-01-01', '2024-01-01 24:00:00', "
                   "'2024-01-01', '2024-01-01')",
            .message_fragment = "invalid datetime value",
            .context = "direct temporal descriptor rejects invalid datetimes",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = invalid_temporal_error,
            .sqlstate = "22007",
            .context = "invalid datetime exposes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear invalid datetime fork condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO temporal_direct VALUES "
                   "(4, '2024-01-01', '9999-12-31 23:59:59.5', "
                   "'2024-01-01', '2024-01-01')",
            .message_fragment = "datetime field overflow",
            .context = "direct temporal descriptor rejects post-round overflow",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = datetime_overflow_error,
            .sqlstate = "22008",
            .context = "datetime overflow exposes MySQL condition",
        }
    );
    failures += exec_sql(
        database,
        "CREATE TABLE temporal_zero_direct(id INTEGER PRIMARY KEY, d TEXT, dt TEXT)",
        "create direct temporal zero descriptor fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "temporal_zero_direct",
            "d",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_DATE,
                .flags = MYLITE_SQLITE_FORK_COLUMN_TYPE_ALLOW_ZERO_TEMPORAL,
            }
        ),
        database,
        "set direct zero-date descriptor"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "temporal_zero_direct",
            "dt",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_DATETIME,
                .flags = MYLITE_SQLITE_FORK_COLUMN_TYPE_ALLOW_ZERO_TEMPORAL,
            }
        ),
        database,
        "set direct zero-datetime descriptor"
    );
    failures += exec_sql(
        database,
        "INSERT INTO temporal_zero_direct VALUES "
        "(1, '0000-00-00', '0000-00-00 00:00:00')",
        "insert direct temporal zero descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT d || ':' || dt FROM temporal_zero_direct WHERE id = 1",
            .expected = "0000-00-00:0000-00-00 00:00:00",
            .context = "direct temporal descriptors can allow zero sentinels",
        }
    );

    sqlite3_close(database);
    return failures;
}

static int test_wordpress_like_crud(void) {
    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += exec_sql(
        database,
        "CREATE TABLE wp_posts_like ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
        "post_author INTEGER NOT NULL DEFAULT 0 CHECK(post_author >= 0),"
        "post_date TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "post_title TEXT NOT NULL COLLATE utf8mb4_unicode_ci,"
        "post_name TEXT NOT NULL DEFAULT '' COLLATE utf8mb4_unicode_ci "
        "CHECK(CHAR_LENGTH(post_name) <= 200),"
        "post_status TEXT NOT NULL DEFAULT 'publish' COLLATE utf8mb4_unicode_ci "
        "CHECK(CHAR_LENGTH(post_status) <= 20),"
        "comment_count INTEGER NOT NULL DEFAULT 0"
        ")",
        "create wp_posts_like physical table"
    );
    failures += exec_sql(
        database,
        "CREATE INDEX post_name ON wp_posts_like(post_name)",
        "create post_name index"
    );
    failures += exec_sql(
        database,
        "CREATE INDEX post_status_date ON wp_posts_like(post_status, post_date)",
        "create post_status_date index"
    );
    failures += exec_sql(
        database,
        "CREATE TABLE wp_postmeta_like ("
        "meta_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "post_id INTEGER NOT NULL DEFAULT 0 CHECK(post_id >= 0),"
        "meta_key TEXT DEFAULT NULL COLLATE utf8mb4_unicode_ci "
        "CHECK(meta_key IS NULL OR CHAR_LENGTH(meta_key) <= 255),"
        "meta_value TEXT COLLATE utf8mb4_unicode_ci"
        ")",
        "create wp_postmeta_like physical table"
    );
    failures += exec_sql(
        database,
        "CREATE INDEX post_id ON wp_postmeta_like(post_id)",
        "create post_id index"
    );
    failures += exec_sql(
        database,
        "CREATE INDEX meta_key ON wp_postmeta_like(meta_key)",
        "create meta_key index"
    );

    failures += exec_sql(
        database,
        "INSERT INTO wp_posts_like "
        "(post_author, post_date, post_title, post_name, post_status, comment_count) VALUES "
        "(1, '2026-05-06 09:15:00', 'Hello MyLite', 'hello-mylite', 'publish', 2),"
        "(2, '2026-05-06 10:00:00', 'Draft Notes', 'draft-notes', 'draft', 0),"
        "(1, '2026-05-07 08:30:00', 'SQLite Fork Plan', 'sqlite-fork-plan', "
        "'publish', 1)",
        "insert WordPress-like posts"
    );
    failures += exec_sql(
        database,
        "INSERT INTO wp_postmeta_like (post_id, meta_key, meta_value) VALUES "
        "(1, '_edit_lock', '1714994100:1'),"
        "(1, '_thumbnail_id', '99'),"
        "(3, '_wp_page_template', 'default')",
        "insert WordPress-like metadata"
    );
    failures += exec_sql(
        database,
        "UPDATE wp_posts_like "
        "SET post_status = 'publish', comment_count = comment_count + 1 "
        "WHERE post_name = 'draft-notes'",
        "publish draft post"
    );
    failures += exec_sql(
        database,
        "DELETE FROM wp_postmeta_like WHERE meta_key = '_edit_lock'",
        "delete edit lock metadata"
    );

    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT COUNT(*) || ':' || MIN(ID) || ':' || "
                   "MAX(ID) || ':' || SUM(comment_count) "
                   "FROM wp_posts_like",
            .expected = "3:1:3:4",
            .context = "post summary matches MySQL fixture",
        }
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat(ID || ':' || post_name || "
                   "':' || post_status || ':' || "
                   "comment_count, '|') FROM ("
                   "SELECT ID, post_name, post_status, "
                   "comment_count FROM wp_posts_like "
                   "WHERE post_status = 'publish' ORDER BY ID"
                   ")",
            .expected = "1:hello-mylite:publish:2|2:draft-notes:"
                        "publish:1|3:sqlite-fork-plan:publish:1",
            .context = "published rows match MySQL fixture",
        }
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT COUNT(*) || ':' || group_concat(post_id "
                   "|| ':' || meta_key || '=' || "
                   "meta_value, '|') FROM ("
                   "SELECT post_id, meta_key, meta_value FROM "
                   "wp_postmeta_like ORDER BY meta_id"
                   ")",
            .expected = "2:1:_thumbnail_id=99|3:_wp_page_template=default",
            .context = "metadata rows match MySQL fixture before "
                       "truncate",
        }
    );

    failures += expect_sqlite_ok(
        mylite_sqlite_fork_truncate_table(database, "wp_postmeta_like"),
        database,
        "truncate wp_postmeta_like"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT COUNT(*) || ':' || "
                   "COALESCE(MAX(meta_id), 0) FROM "
                   "wp_postmeta_like",
            .expected = "0:0",
            .context = "truncate empties metadata table",
        }
    );
    failures += exec_sql(
        database,
        "INSERT INTO wp_postmeta_like (post_id, meta_key, meta_value) "
        "VALUES (2, '_restored', 'yes')",
        "insert metadata after truncate"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT meta_id || ':' || post_id || ':' || "
                   "meta_key || ':' || meta_value "
                   "FROM wp_postmeta_like",
            .expected = "1:2:_restored:yes",
            .context = "truncate resets auto-increment sequence",
        }
    );
    failures += exec_sql(database, "DROP TABLE wp_postmeta_like", "drop metadata table");
    failures += expect_int64(
        database,
        "SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' "
        "AND name IN ('wp_posts_like', 'wp_postmeta_like')",
        1,
        "drop table leaves only posts table"
    );

    sqlite3_close(database);
    return failures;
}

static int test_mylite_wordpress_like_crud(void) {
    static const char *const post_summary[] = {"3", "1", "3", "4"};
    static const char *const published_rows[] = {
        "1",
        "hello-mylite",
        "publish",
        "2",
        "2",
        "draft-notes",
        "publish",
        "1",
        "3",
        "sqlite-fork-plan",
        "publish",
        "1",
    };
    static const char *const meta_before_truncate[] = {
        "2",
        "1:_thumbnail_id=99|3:_wp_page_template=default",
    };
    static const char *const meta_after_truncate[] = {"0", "0"};
    static const char *const meta_after_reinsert[] = {"1", "2", "_restored", "yes"};
    static const char *const remaining_tables[] = {"1"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_mylite_ok(mylite_open_memory(&database), database, "open MyLite fork CRUD");
    if (failures != 0) {
        return failures;
    }

    failures += exec_mylite_sql(
        database,
        "CREATE DATABASE mylite_fork_crud CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci",
        "create MyLite fork CRUD schema"
    );
    failures += exec_mylite_sql(database, "USE mylite_fork_crud", "use MyLite fork CRUD schema");
    failures += exec_mylite_sql(
        database,
        "CREATE TABLE wp_posts_like ("
        "ID BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        "post_author BIGINT UNSIGNED NOT NULL DEFAULT 0,"
        "post_date DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "post_title TEXT NOT NULL,"
        "post_name VARCHAR(200) NOT NULL DEFAULT '',"
        "post_status VARCHAR(20) NOT NULL DEFAULT 'publish',"
        "comment_count BIGINT NOT NULL DEFAULT 0,"
        "PRIMARY KEY (ID),"
        "KEY post_name (post_name),"
        "KEY post_status_date (post_status, post_date)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
        "create MyLite wp_posts_like"
    );
    failures += exec_mylite_sql(
        database,
        "CREATE TABLE wp_postmeta_like ("
        "meta_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        "post_id BIGINT UNSIGNED NOT NULL DEFAULT 0,"
        "meta_key VARCHAR(255) DEFAULT NULL,"
        "meta_value LONGTEXT,"
        "PRIMARY KEY (meta_id),"
        "KEY post_id (post_id),"
        "KEY meta_key (meta_key)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
        "create MyLite wp_postmeta_like"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO wp_posts_like "
        "(post_author, post_date, post_title, post_name, post_status, comment_count) VALUES "
        "(1, '2026-05-06 09:15:00', 'Hello MyLite', 'hello-mylite', 'publish', 2),"
        "(2, '2026-05-06 10:00:00', 'Draft Notes', 'draft-notes', 'draft', 0),"
        "(1, '2026-05-07 08:30:00', 'SQLite Fork Plan', 'sqlite-fork-plan', 'publish', 1)",
        "insert MyLite posts"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO wp_postmeta_like (post_id, meta_key, meta_value) VALUES "
        "(1, '_edit_lock', '1714994100:1'),"
        "(1, '_thumbnail_id', '99'),"
        "(3, '_wp_page_template', 'default')",
        "insert MyLite postmeta"
    );
    failures += exec_mylite_sql(
        database,
        "UPDATE wp_posts_like "
        "SET post_status = 'publish', comment_count = comment_count + 1 "
        "WHERE post_name = 'draft-notes'",
        "publish MyLite draft post"
    );
    failures += exec_mylite_sql(
        database,
        "DELETE FROM wp_postmeta_like WHERE meta_key = '_edit_lock'",
        "delete MyLite edit lock"
    );

    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT COUNT(*), MIN(ID), MAX(ID), SUM(comment_count) FROM wp_posts_like",
            .values = post_summary,
            .column_count = 4,
            .row_count = 1,
            .context = "MyLite post summary matches MySQL fixture",
        }
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT ID, post_name, post_status, comment_count "
                   "FROM wp_posts_like WHERE post_status = 'publish' ORDER BY ID",
            .values = published_rows,
            .column_count = 4,
            .row_count = 3,
            .context = "MyLite published rows match MySQL fixture",
        }
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT COUNT(*), GROUP_CONCAT(CONCAT(post_id, ':', meta_key, '=', "
                   "meta_value) ORDER BY meta_id SEPARATOR '|') FROM wp_postmeta_like",
            .values = meta_before_truncate,
            .column_count = 2,
            .row_count = 1,
            .context = "MyLite metadata rows match MySQL fixture before truncate",
        }
    );

    failures +=
        exec_mylite_sql(database, "TRUNCATE TABLE wp_postmeta_like", "truncate MyLite metadata");
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT COUNT(*), COALESCE(MAX(meta_id), 0) FROM wp_postmeta_like",
            .values = meta_after_truncate,
            .column_count = 2,
            .row_count = 1,
            .context = "MyLite truncate empties metadata table",
        }
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO wp_postmeta_like (post_id, meta_key, meta_value) "
        "VALUES (2, '_restored', 'yes')",
        "insert MyLite metadata after truncate"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT meta_id, post_id, meta_key, meta_value FROM wp_postmeta_like",
            .values = meta_after_reinsert,
            .column_count = 4,
            .row_count = 1,
            .context = "MyLite truncate resets auto-increment sequence",
        }
    );
    failures += exec_mylite_sql(database, "DROP TABLE wp_postmeta_like", "drop MyLite metadata");
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT COUNT(*) FROM information_schema.tables "
                   "WHERE table_schema = DATABASE() "
                   "AND table_name IN ('wp_posts_like', 'wp_postmeta_like')",
            .values = remaining_tables,
            .column_count = 1,
            .row_count = 1,
            .context = "MyLite remaining tables match MySQL fixture",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_mylite_basic_type_coercion(void) {
    enum {
        type_coercion_column_count = 6,
        type_coercion_out_of_range_error = 1264,
        type_coercion_truncated_error = 1265,
        type_coercion_too_long_error = 1406,
    };

    static const char *const after_insert[] = {
        "1",
        "42",
        "7",
        "123",
        "3.2500",
        "1",
        "2",
        "-5",
        "0",
        "éé",
        "4.0000",
        "0",
    };
    static const char *const after_update[] = {
        "1",
        "42",
        "7",
        "123",
        "3.2500",
        "1",
        "2",
        "12",
        "8",
        "99",
        "6.5000",
        "0",
    };
    static const char *const after_duplicate_update[] = {
        "1",
        "42",
        "7",
        "123",
        "3.2500",
        "1",
        "2",
        "9",
        "10",
        "77",
        "8.7500",
        "1",
    };
    static const char *const after_replace[] = {
        "1",
        "42",
        "7",
        "123",
        "3.2500",
        "1",
        "2",
        "9",
        "10",
        "77",
        "8.7500",
        "1",
        "3",
        "11",
        "12",
        "456",
        "9.2500",
        "0",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_mylite_ok(mylite_open_memory(&database), database, "open MyLite type coercion");
    if (failures != 0) {
        return failures;
    }

    failures += exec_mylite_sql(
        database,
        "CREATE DATABASE mylite_type_coercion CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci",
        "create MyLite type coercion schema"
    );
    failures += exec_mylite_sql(database, "USE mylite_type_coercion", "use type coercion schema");
    failures += exec_mylite_sql(
        database,
        "CREATE TABLE coercion_basic ("
        "id INT PRIMARY KEY,"
        "tiny TINYINT NOT NULL,"
        "unsigned_id INT UNSIGNED NOT NULL,"
        "label VARCHAR(4) NOT NULL,"
        "score DOUBLE NOT NULL,"
        "optional VARCHAR(4) NULL"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
        "create MyLite type coercion table"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO coercion_basic VALUES "
        "(1, '42', '7', 123, '3.25', NULL),"
        "('2', -5, 0, 'éé', 4, 'ok')",
        "insert MyLite type coercion rows"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, tiny, unsigned_id, label, score + 0, optional IS NULL "
                   "FROM coercion_basic ORDER BY id",
            .values = after_insert,
            .column_count = type_coercion_column_count,
            .row_count = 2,
            .context = "MyLite insert coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "UPDATE coercion_basic "
        "SET tiny = '12', unsigned_id = '8', label = 99, score = '6.5' "
        "WHERE id = '2'",
        "update MyLite type coercion row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, tiny, unsigned_id, label, score + 0, optional IS NULL "
                   "FROM coercion_basic ORDER BY id",
            .values = after_update,
            .column_count = type_coercion_column_count,
            .row_count = 2,
            .context = "MyLite update coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO coercion_basic VALUES (2, '9', '10', 77, '8.75', NULL) "
        "ON DUPLICATE KEY UPDATE tiny = VALUES(tiny), unsigned_id = VALUES(unsigned_id), "
        "label = VALUES(label), score = VALUES(score), optional = VALUES(optional)",
        "insert duplicate update MyLite type coercion row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, tiny, unsigned_id, label, score + 0, optional IS NULL "
                   "FROM coercion_basic ORDER BY id",
            .values = after_duplicate_update,
            .column_count = type_coercion_column_count,
            .row_count = 2,
            .context = "MyLite duplicate update coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "REPLACE INTO coercion_basic VALUES (3, '11', '12', 456, '9.25', 'zz')",
        "replace MyLite type coercion row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, tiny, unsigned_id, label, score + 0, optional IS NULL "
                   "FROM coercion_basic ORDER BY id",
            .values = after_replace,
            .column_count = type_coercion_column_count,
            .row_count = 3,
            .context = "MyLite replace coercion matches MySQL fixture",
        }
    );

    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO coercion_basic VALUES (4, 128, 1, 'ok', 1, NULL)",
        MYLITE_SQLITE_ERROR,
        "MyLite insert coercion rejects out-of-range tinyint"
    );
    failures += expect_mylite_error_condition(
        database,
        type_coercion_out_of_range_error,
        "MyLite out-of-range tinyint condition"
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO coercion_basic VALUES (4, 1, -1, 'ok', 1, NULL)",
        MYLITE_SQLITE_ERROR,
        "MyLite insert coercion rejects negative unsigned integer"
    );
    failures += expect_mylite_error_condition(
        database,
        type_coercion_out_of_range_error,
        "MyLite negative unsigned integer condition"
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO coercion_basic VALUES (4, 1, 1, 'abcde', 1, NULL)",
        MYLITE_SQLITE_ERROR,
        "MyLite insert coercion rejects over-length varchar"
    );
    failures += expect_mylite_error_condition(
        database,
        type_coercion_too_long_error,
        "MyLite over-length varchar condition"
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO coercion_basic VALUES (4, 1, 1, 'ok', 'bad', NULL)",
        MYLITE_SQLITE_ERROR,
        "MyLite insert coercion rejects invalid double"
    );
    failures += expect_mylite_error_condition(
        database,
        type_coercion_truncated_error,
        "MyLite invalid double condition"
    );

    mylite_close(database);
    return failures;
}

static int test_mylite_binary_type_coercion(void) {
    enum {
        binary_column_count = 5,
        binary_string_too_long_error = 1406,
    };

    static const char *const after_insert[] = {
        "1",
        "610000",
        "3",
        "C3A9",
        "2",
        "2",
        "363500",
        "3",
        "31323334",
        "4",
    };
    static const char *const after_update[] = {
        "1",
        "610000",
        "3",
        "C3A9",
        "2",
        "2",
        "787900",
        "3",
        "7A",
        "1",
    };
    static const char *const after_duplicate_update[] = {
        "1",
        "610000",
        "3",
        "C3A9",
        "2",
        "2",
        "757600",
        "3",
        "7778",
        "2",
    };
    static const char *const after_replace[] = {
        "1",
        "610000",
        "3",
        "C3A9",
        "2",
        "2",
        "757600",
        "3",
        "7778",
        "2",
        "3",
        "720000",
        "3",
        "7374",
        "2",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_mylite_ok(mylite_open_memory(&database), database, "open MyLite binary coercion");
    if (failures != 0) {
        return failures;
    }

    failures += exec_mylite_sql(
        database,
        "CREATE DATABASE mylite_binary_coercion CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci",
        "create MyLite binary coercion schema"
    );
    failures +=
        exec_mylite_sql(database, "USE mylite_binary_coercion", "use binary coercion schema");
    failures += exec_mylite_sql(
        database,
        "CREATE TABLE binary_basic ("
        "id INT PRIMARY KEY,"
        "fixed BINARY(3) NOT NULL,"
        "variable VARBINARY(4) NOT NULL"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
        "create MyLite binary coercion table"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO binary_basic VALUES (1, 'a', 'é'), (2, 65, 1234)",
        "insert MyLite binary coercion rows"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, HEX(fixed), LENGTH(fixed), HEX(variable), LENGTH(variable) "
                   "FROM binary_basic ORDER BY id",
            .values = after_insert,
            .column_count = binary_column_count,
            .row_count = 2,
            .context = "MyLite binary insert coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "UPDATE binary_basic SET fixed = 'xy', variable = 'z' WHERE id = 2",
        "update MyLite binary coercion row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, HEX(fixed), LENGTH(fixed), HEX(variable), LENGTH(variable) "
                   "FROM binary_basic ORDER BY id",
            .values = after_update,
            .column_count = binary_column_count,
            .row_count = 2,
            .context = "MyLite binary update coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO binary_basic VALUES (2, 'uv', 'wx') "
        "ON DUPLICATE KEY UPDATE fixed = VALUES(fixed), variable = VALUES(variable)",
        "insert duplicate update MyLite binary coercion row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, HEX(fixed), LENGTH(fixed), HEX(variable), LENGTH(variable) "
                   "FROM binary_basic ORDER BY id",
            .values = after_duplicate_update,
            .column_count = binary_column_count,
            .row_count = 2,
            .context = "MyLite binary duplicate update coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "REPLACE INTO binary_basic VALUES (3, 'r', 'st')",
        "replace MyLite binary coercion row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, HEX(fixed), LENGTH(fixed), HEX(variable), LENGTH(variable) "
                   "FROM binary_basic ORDER BY id",
            .values = after_replace,
            .column_count = binary_column_count,
            .row_count = 3,
            .context = "MyLite binary replace coercion matches MySQL fixture",
        }
    );

    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO binary_basic VALUES (4, 'abcd', 'ok')",
        MYLITE_SQLITE_ERROR,
        "MyLite binary coercion rejects over-length fixed value"
    );
    failures += expect_mylite_error_condition(
        database,
        binary_string_too_long_error,
        "MyLite over-length fixed binary condition"
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO binary_basic VALUES (4, 'ok', 'abcde')",
        MYLITE_SQLITE_ERROR,
        "MyLite binary coercion rejects over-length varbinary value"
    );
    failures += expect_mylite_error_condition(
        database,
        binary_string_too_long_error,
        "MyLite over-length varbinary condition"
    );

    mylite_close(database);
    return failures;
}

static int test_mylite_decimal_type_coercion(void) {
    enum {
        decimal_column_count = 4,
        decimal_out_of_range_error = 1264,
        invalid_decimal_error = 1366,
    };

    static const char *const after_insert[] = {
        "1",
        "1.23",
        "13",
        "0.00",
        "2",
        "-1.24",
        "-13",
        "10.00",
        "3",
        "1.20",
        "10",
        "100.00",
    };
    static const char *const after_update[] = {
        "1",
        "999.99",
        "-1",
        "0.00",
        "2",
        "-1.24",
        "-13",
        "10.00",
        "3",
        "1.20",
        "10",
        "100.00",
    };
    static const char *const after_duplicate_update[] = {
        "1",
        "999.99",
        "-1",
        "0.00",
        "2",
        "2.23",
        "2",
        "1.56",
        "3",
        "1.20",
        "10",
        "100.00",
    };
    static const char *const after_replace[] = {
        "1",
        "999.99",
        "-1",
        "0.00",
        "2",
        "2.23",
        "2",
        "1.56",
        "3",
        "1.20",
        "10",
        "100.00",
        "4",
        "3.34",
        "-3",
        "2.23",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_mylite_ok(mylite_open_memory(&database), database, "open MyLite decimal coercion");
    if (failures != 0) {
        return failures;
    }

    failures += exec_mylite_sql(
        database,
        "CREATE DATABASE mylite_decimal_coercion CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci",
        "create MyLite decimal coercion schema"
    );
    failures +=
        exec_mylite_sql(database, "USE mylite_decimal_coercion", "use decimal coercion schema");
    failures += exec_mylite_sql(
        database,
        "CREATE TABLE decimal_basic ("
        "id INT PRIMARY KEY,"
        "amount DECIMAL(5,2) NOT NULL,"
        "whole DECIMAL(4,0) NOT NULL,"
        "unsigned_amount DECIMAL(5,2) UNSIGNED NOT NULL"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
        "create MyLite decimal coercion table"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO decimal_basic VALUES "
        "(1, 1.234, 12.5, 0),"
        "(2, -1.235, -12.5, 9.995),"
        "(3, '001.2', '9.5', '99.999')",
        "insert MyLite decimal coercion rows"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, amount, whole, unsigned_amount "
                   "FROM decimal_basic ORDER BY id",
            .values = after_insert,
            .column_count = decimal_column_count,
            .row_count = 3,
            .context = "MyLite decimal insert coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "UPDATE decimal_basic SET amount = 999.994, whole = -0.5, "
        "unsigned_amount = 0.004 WHERE id = 1",
        "update MyLite decimal coercion row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, amount, whole, unsigned_amount "
                   "FROM decimal_basic ORDER BY id",
            .values = after_update,
            .column_count = decimal_column_count,
            .row_count = 3,
            .context = "MyLite decimal update coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO decimal_basic VALUES (2, 2.225, 1.5, 1.555) "
        "ON DUPLICATE KEY UPDATE amount = VALUES(amount), whole = VALUES(whole), "
        "unsigned_amount = VALUES(unsigned_amount)",
        "insert duplicate update MyLite decimal coercion row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, amount, whole, unsigned_amount "
                   "FROM decimal_basic ORDER BY id",
            .values = after_duplicate_update,
            .column_count = decimal_column_count,
            .row_count = 3,
            .context = "MyLite decimal duplicate update coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "REPLACE INTO decimal_basic VALUES (4, 3.335, -2.5, 2.225)",
        "replace MyLite decimal coercion row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, amount, whole, unsigned_amount "
                   "FROM decimal_basic ORDER BY id",
            .values = after_replace,
            .column_count = decimal_column_count,
            .row_count = 4,
            .context = "MyLite decimal replace coercion matches MySQL fixture",
        }
    );

    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO decimal_basic VALUES (5, 999.995, 1, 1)",
        MYLITE_SQLITE_ERROR,
        "MyLite decimal coercion rejects post-round overflow"
    );
    failures += expect_mylite_error_condition(
        database,
        decimal_out_of_range_error,
        "MyLite decimal post-round overflow condition"
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO decimal_basic VALUES (5, '12abc', 1, 1)",
        MYLITE_SQLITE_ERROR,
        "MyLite decimal coercion rejects invalid text"
    );
    failures += expect_mylite_error_condition(
        database,
        invalid_decimal_error,
        "MyLite invalid decimal condition"
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO decimal_basic VALUES (5, 1, 1, -0.01)",
        MYLITE_SQLITE_ERROR,
        "MyLite decimal coercion rejects unsigned negative value"
    );
    failures += expect_mylite_error_condition(
        database,
        decimal_out_of_range_error,
        "MyLite unsigned negative decimal condition"
    );

    mylite_close(database);
    return failures;
}

static int test_mylite_temporal_type_coercion(void) {
    enum {
        temporal_column_count = 5,
        invalid_temporal_error = 1292,
        datetime_overflow_error = 1441,
    };

    static const char *const after_insert[] = {
        "1",
        "2024-02-29",
        "2024-02-29 12:34:56",
        "2024-02-29 12:34:56.790",
        "2024-02-29 12:34:56.123457",
        "2",
        "2024-02-29",
        "2024-02-29 12:34:56",
        "2024-02-29 12:34:56.100",
        "2024-02-29 12:34:56.123000",
        "3",
        "2024-02-29",
        "2024-02-29 12:34:56",
        "2024-02-29 12:34:56.789",
        "2024-02-29 12:34:56.123456",
    };
    static const char *const after_update[] = {
        "1",
        "2024-03-01",
        "2024-03-01 00:00:00",
        "2024-03-01 01:02:04.000",
        "2024-03-01 01:02:03.000002",
        "2",
        "2024-02-29",
        "2024-02-29 12:34:56",
        "2024-02-29 12:34:56.100",
        "2024-02-29 12:34:56.123000",
        "3",
        "2024-02-29",
        "2024-02-29 12:34:56",
        "2024-02-29 12:34:56.789",
        "2024-02-29 12:34:56.123456",
    };
    static const char *const after_duplicate_update[] = {
        "1",
        "2024-03-01",
        "2024-03-01 00:00:00",
        "2024-03-01 01:02:04.000",
        "2024-03-01 01:02:03.000002",
        "2",
        "2024-04-05",
        "2024-04-05 06:07:09",
        "2024-04-05 06:07:08.988",
        "2024-04-05 06:07:08.987654",
        "3",
        "2024-02-29",
        "2024-02-29 12:34:56",
        "2024-02-29 12:34:56.789",
        "2024-02-29 12:34:56.123456",
    };
    static const char *const after_replace[] = {
        "1",
        "2024-03-01",
        "2024-03-01 00:00:00",
        "2024-03-01 01:02:04.000",
        "2024-03-01 01:02:03.000002",
        "2",
        "2024-04-05",
        "2024-04-05 06:07:09",
        "2024-04-05 06:07:08.988",
        "2024-04-05 06:07:08.987654",
        "3",
        "2024-02-29",
        "2024-02-29 12:34:56",
        "2024-02-29 12:34:56.789",
        "2024-02-29 12:34:56.123456",
        "4",
        "1999-12-31",
        "2000-01-01 00:00:00",
        "1999-12-31 23:59:59.877",
        "1999-12-31 23:59:59.876543",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_mylite_ok(mylite_open_memory(&database), database, "open MyLite temporal coercion");
    if (failures != 0) {
        return failures;
    }

    failures += exec_mylite_sql(
        database,
        "CREATE DATABASE mylite_temporal_coercion CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci",
        "create MyLite temporal coercion schema"
    );
    failures +=
        exec_mylite_sql(database, "USE mylite_temporal_coercion", "use temporal coercion schema");
    failures += exec_mylite_sql(
        database,
        "CREATE TABLE temporal_basic ("
        "id INT PRIMARY KEY,"
        "d DATE NOT NULL,"
        "dt DATETIME NOT NULL,"
        "dt3 DATETIME(3) NOT NULL,"
        "dt6 DATETIME(6) NOT NULL"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
        "create MyLite temporal coercion table"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO temporal_basic VALUES "
        "(1, '2024-02-29', '2024-02-29 12:34:56', "
        "'2024-02-29 12:34:56.7896', '2024-02-29 12:34:56.1234567'),"
        "(2, '20240229', '20240229123456', '20240229123456.1', "
        "'20240229123456.123'),"
        "(3, 20240229, 20240229123456, '20240229123456.789', "
        "'20240229123456.123456')",
        "insert MyLite temporal coercion rows"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, d, dt, dt3, dt6 FROM temporal_basic ORDER BY id",
            .values = after_insert,
            .column_count = temporal_column_count,
            .row_count = 3,
            .context = "MyLite temporal insert coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "UPDATE temporal_basic SET d = '2024-03-01', dt = '2024-03-01', "
        "dt3 = '2024-03-01 01:02:03.9999', "
        "dt6 = '2024-03-01 01:02:03.0000019' WHERE id = 1",
        "update MyLite temporal coercion row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, d, dt, dt3, dt6 FROM temporal_basic ORDER BY id",
            .values = after_update,
            .column_count = temporal_column_count,
            .row_count = 3,
            .context = "MyLite temporal update coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO temporal_basic VALUES "
        "(2, '2024-04-05 06:07:08', '2024-04-05 06:07:08.9', "
        "'2024-04-05 06:07:08.9876', '2024-04-05 06:07:08.987654') "
        "ON DUPLICATE KEY UPDATE d = VALUES(d), dt = VALUES(dt), "
        "dt3 = VALUES(dt3), dt6 = VALUES(dt6)",
        "insert duplicate update MyLite temporal coercion row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, d, dt, dt3, dt6 FROM temporal_basic ORDER BY id",
            .values = after_duplicate_update,
            .column_count = temporal_column_count,
            .row_count = 3,
            .context = "MyLite temporal duplicate update coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "REPLACE INTO temporal_basic VALUES "
        "(4, '1999-12-31', '1999-12-31 23:59:59.8', "
        "'1999-12-31 23:59:59.8765', '1999-12-31 23:59:59.876543')",
        "replace MyLite temporal coercion row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, d, dt, dt3, dt6 FROM temporal_basic ORDER BY id",
            .values = after_replace,
            .column_count = temporal_column_count,
            .row_count = 4,
            .context = "MyLite temporal replace coercion matches MySQL fixture",
        }
    );

    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO temporal_basic VALUES "
        "(5, '2024-02-30', '2024-01-01', '2024-01-01', '2024-01-01')",
        MYLITE_SQLITE_ERROR,
        "MyLite temporal coercion rejects invalid dates"
    );
    failures += expect_mylite_error_condition(
        database,
        invalid_temporal_error,
        "MyLite invalid date condition"
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO temporal_basic VALUES "
        "(5, '2024-01-01', '2024-01-01 24:00:00', '2024-01-01', '2024-01-01')",
        MYLITE_SQLITE_ERROR,
        "MyLite temporal coercion rejects invalid datetimes"
    );
    failures += expect_mylite_error_condition(
        database,
        invalid_temporal_error,
        "MyLite invalid datetime condition"
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO temporal_basic VALUES "
        "(5, '2024-01-01', '9999-12-31 23:59:59.5', "
        "'2024-01-01', '2024-01-01')",
        MYLITE_SQLITE_ERROR,
        "MyLite temporal coercion rejects post-round overflow"
    );
    failures += expect_mylite_error_condition(
        database,
        datetime_overflow_error,
        "MyLite datetime overflow condition"
    );

    mylite_close(database);
    return failures;
}

static int test_mylite_collation_unique_semantics(void) {
    enum {
        single_column = 1,
    };

    static const char *const prefix_ci_rows[] = {"Post Alpha"};
    static const char *const prefix_bin_rows[] = {"Post Alpha", "post Beta"};
    static const char *const pad_space_rows[] = {"trail:5"};
    static const char *const no_pad_rows[] = {"trail:5", "trail :6"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_mylite_ok(
        mylite_open_memory(&database),
        database,
        "open MyLite collation unique semantics"
    );
    if (failures != 0) {
        return failures;
    }

    failures += exec_mylite_sql(
        database,
        "CREATE DATABASE mylite_collation_unique CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci",
        "create MyLite collation unique schema"
    );
    failures +=
        exec_mylite_sql(database, "USE mylite_collation_unique", "use collation unique schema");

    failures += exec_mylite_sql(
        database,
        "CREATE TABLE prefix_ci ("
        "id INT AUTO_INCREMENT PRIMARY KEY,"
        "slug VARCHAR(20) COLLATE utf8mb4_unicode_ci NOT NULL,"
        "UNIQUE KEY uq_slug4 (slug(4))"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
        "create MyLite case-insensitive prefix unique table"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO prefix_ci(slug) VALUES ('Post Alpha')",
        "insert MyLite case-insensitive prefix seed"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT IGNORE INTO prefix_ci(slug) VALUES ('post Beta')",
        "ignore MyLite case-insensitive prefix duplicate"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT slug FROM prefix_ci ORDER BY id",
            .values = prefix_ci_rows,
            .column_count = single_column,
            .row_count = 1,
            .context = "MyLite prefix unique uses case-insensitive SQLite collation",
        }
    );

    failures += exec_mylite_sql(
        database,
        "CREATE TABLE prefix_bin ("
        "id INT AUTO_INCREMENT PRIMARY KEY,"
        "slug VARCHAR(20) COLLATE utf8mb4_bin NOT NULL,"
        "UNIQUE KEY uq_slug4 (slug(4))"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
        "create MyLite binary prefix unique table"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO prefix_bin(slug) VALUES ('Post Alpha')",
        "insert MyLite binary prefix seed"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT IGNORE INTO prefix_bin(slug) VALUES ('post Beta')",
        "insert MyLite binary prefix distinct value"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT slug FROM prefix_bin ORDER BY id",
            .values = prefix_bin_rows,
            .column_count = single_column,
            .row_count = 2,
            .context = "MyLite prefix unique keeps binary SQLite collation byte-sensitive",
        }
    );

    failures += exec_mylite_sql(
        database,
        "CREATE TABLE pad_space_unique ("
        "id INT AUTO_INCREMENT PRIMARY KEY,"
        "value VARCHAR(20) COLLATE utf8mb4_unicode_ci NOT NULL UNIQUE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
        "create MyLite PAD SPACE unique table"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO pad_space_unique(value) VALUES ('trail')",
        "insert MyLite PAD SPACE unique seed"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT IGNORE INTO pad_space_unique(value) VALUES ('trail ')",
        "ignore MyLite PAD SPACE duplicate"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT CONCAT(value, ':', LENGTH(value)) FROM pad_space_unique ORDER BY id",
            .values = pad_space_rows,
            .column_count = single_column,
            .row_count = 1,
            .context = "MyLite unique checks honor PAD SPACE SQLite collation",
        }
    );

    failures += exec_mylite_sql(
        database,
        "CREATE TABLE no_pad_unique ("
        "id INT AUTO_INCREMENT PRIMARY KEY,"
        "value VARCHAR(20) COLLATE utf8mb4_0900_ai_ci NOT NULL UNIQUE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
        "create MyLite NO PAD unique table"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO no_pad_unique(value) VALUES ('trail')",
        "insert MyLite NO PAD unique seed"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT IGNORE INTO no_pad_unique(value) VALUES ('trail ')",
        "insert MyLite NO PAD trailing-space value"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT CONCAT(value, ':', LENGTH(value)) FROM no_pad_unique ORDER BY id",
            .values = no_pad_rows,
            .column_count = single_column,
            .row_count = 2,
            .context = "MyLite unique checks honor NO PAD SQLite collation",
        }
    );

    failures += exec_mylite_sql(
        database,
        "CREATE TABLE prefix_existing ("
        "id INT PRIMARY KEY,"
        "slug VARCHAR(20) COLLATE utf8mb4_unicode_ci NOT NULL"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
        "create MyLite existing prefix duplicate table"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO prefix_existing VALUES (1, 'Post Alpha'), (2, 'post Beta')",
        "insert MyLite existing prefix duplicates"
    );
    failures += expect_mylite_sql_status(
        database,
        "CREATE UNIQUE INDEX uq_existing_slug4 ON prefix_existing(slug(4))",
        MYLITE_EXEC_ERROR,
        "MyLite CREATE UNIQUE INDEX detects case-insensitive prefix duplicates"
    );
    failures += expect_mylite_sql_status(
        database,
        "ALTER TABLE prefix_existing ADD UNIQUE KEY uq_existing_slug4_alt (slug(4))",
        MYLITE_EXEC_ERROR,
        "MyLite ALTER TABLE ADD UNIQUE detects case-insensitive prefix duplicates"
    );

    mylite_close(database);
    return failures;
}

static int open_configured_database(sqlite3 **out_database) {
    sqlite3 *database = NULL;
    int rc = SQLITE_OK;

    *out_database = NULL;
    rc = sqlite3_open_v2(
        ":memory:",
        &database,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY,
        NULL
    );
    if (rc != SQLITE_OK) {
        fprintf(stderr, "sqlite open failed: %s\n", sqlite3_errmsg(database));
        sqlite3_close(database);
        return 1;
    }
    rc = mylite_sqlite_fork_configure(database);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "fork configure failed: %s\n", sqlite3_errmsg(database));
        sqlite3_close(database);
        return 1;
    }

    *out_database = database;
    return 0;
}

static int exec_sql(sqlite3 *database, const char *sql, const char *context) {
    return expect_sqlite_ok(sqlite3_exec(database, sql, NULL, NULL, NULL), database, context);
}

static int exec_mylite_sql(mylite_db *database, const char *sql, const char *context) {
    mylite_stmt *statement = NULL;
    int failures =
        expect_mylite_ok(mylite_prepare(database, sql, strlen(sql), &statement), database, context);

    if (failures == 0) {
        failures += expect_mylite_status(mylite_step(statement), MYLITE_DONE, database, context);
    }
    if (statement != NULL) {
        mylite_finalize(statement);
    }
    return failures;
}

static int expect_sqlite_exec_error(sqlite3 *database, struct expected_sqlite_error expectation) {
    char *message = NULL;
    int rc = sqlite3_exec(database, expectation.sql, NULL, NULL, &message);
    int failures = 0;

    if (rc == SQLITE_OK) {
        fprintf(stderr, "%s: expected sqlite execution error\n", expectation.context);
        sqlite3_free(message);
        return 1;
    }
    if (expectation.message_fragment != NULL &&
        (message == NULL || strstr(message, expectation.message_fragment) == NULL)) {
        fprintf(
            stderr,
            "%s: expected sqlite error containing \"%s\", got \"%s\"\n",
            expectation.context,
            expectation.message_fragment,
            message == NULL ? "(null)" : message
        );
        ++failures;
    }
    sqlite3_free(message);
    return failures;
}

static int expect_fork_condition(sqlite3 *database, struct expected_fork_condition expectation) {
    struct mylite_sqlite_fork_condition condition = {0};
    int failures = expect_sqlite_ok(
        mylite_sqlite_fork_last_condition(database, &condition),
        database,
        expectation.context
    );

    if (failures != 0) {
        return failures;
    }
    if (condition.level != MYLITE_SQLITE_FORK_CONDITION_ERROR) {
        fprintf(stderr, "%s: expected fork error condition\n", expectation.context);
        ++failures;
    }
    if (condition.mysql_errno != expectation.mysql_errno) {
        fprintf(
            stderr,
            "%s: expected MySQL errno %u, got %u\n",
            expectation.context,
            expectation.mysql_errno,
            condition.mysql_errno
        );
        ++failures;
    }
    if (strcmp(condition.sqlstate, expectation.sqlstate) != 0) {
        fprintf(
            stderr,
            "%s: expected SQLSTATE %s, got %s\n",
            expectation.context,
            expectation.sqlstate,
            condition.sqlstate
        );
        ++failures;
    }
    return failures;
}

static int expect_text(sqlite3 *database, struct expected_text_row expectation) {
    sqlite3_stmt *statement = NULL;
    const unsigned char *actual = NULL;
    int failures =
        prepare_single_column(database, expectation.sql, &statement, expectation.context);

    if (failures != 0) {
        return failures;
    }

    actual = sqlite3_column_text(statement, 0);
    if ((actual == NULL && expectation.expected != NULL) ||
        (actual != NULL && expectation.expected == NULL) ||
        (actual != NULL && strcmp((const char *)actual, expectation.expected) != 0)) {
        fprintf(
            stderr,
            "%s: expected \"%s\", got \"%s\"\n",
            expectation.context,
            expectation.expected == NULL ? "(null)" : expectation.expected,
            actual == NULL ? "(null)" : (const char *)actual
        );
        ++failures;
    }

    failures += finish_single_row(statement, expectation.context);
    return failures;
}

static int expect_mylite_rows(mylite_db *database, struct expected_mylite_rows expectation) {
    mylite_stmt *statement = NULL;
    int failures = expect_mylite_ok(
        mylite_prepare(database, expectation.sql, strlen(expectation.sql), &statement),
        database,
        expectation.context
    );

    if (failures != 0) {
        mylite_finalize(statement);
        return failures;
    }
    for (int row = 0; row < expectation.row_count; ++row) {
        failures +=
            expect_mylite_status(mylite_step(statement), MYLITE_ROW, database, expectation.context);
        if (failures != 0) {
            break;
        }
        for (int column = 0; column < expectation.column_count; ++column) {
            const char *expected = expectation.values[(row * expectation.column_count) + column];
            const char *actual = mylite_column_text(statement, column);

            if ((actual == NULL && expected != NULL) || (actual != NULL && expected == NULL) ||
                (actual != NULL && strcmp(actual, expected) != 0)) {
                fprintf(
                    stderr,
                    "%s: row %d column %d expected \"%s\", got \"%s\"\n",
                    expectation.context,
                    row,
                    column,
                    expected == NULL ? "(null)" : expected,
                    actual == NULL ? "(null)" : actual
                );
                ++failures;
            }
        }
    }
    if (failures == 0) {
        failures += expect_mylite_status(
            mylite_step(statement),
            MYLITE_DONE,
            database,
            expectation.context
        );
    }

    mylite_finalize(statement);
    return failures;
}

static int expect_mylite_error_condition(
    mylite_db *database,
    unsigned int mysql_errno,
    const char *context
) {
    if (mylite_warning_count(database) != 1) {
        fprintf(
            stderr,
            "%s: expected one MyLite condition, got %d\n",
            context,
            mylite_warning_count(database)
        );
        return 1;
    }
    if (mylite_warning_code(database, 0) != mysql_errno) {
        fprintf(
            stderr,
            "%s: expected MySQL errno %u, got %u\n",
            context,
            mysql_errno,
            mylite_warning_code(database, 0)
        );
        return 1;
    }
    return 0;
}

static int expect_int64(
    sqlite3 *database,
    const char *sql,
    sqlite3_int64 expected,
    const char *context
) {
    sqlite3_stmt *statement = NULL;
    sqlite3_int64 actual = 0;
    int failures = prepare_single_column(database, sql, &statement, context);

    if (failures != 0) {
        return failures;
    }

    actual = sqlite3_column_int64(statement, 0);
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
        );
        ++failures;
    }

    failures += finish_single_row(statement, context);
    return failures;
}

static int prepare_single_column(
    sqlite3 *database,
    const char *sql,
    sqlite3_stmt **out_statement,
    const char *context
) {
    sqlite3_stmt *statement = NULL;
    int rc = sqlite3_prepare_v3(database, sql, -1, SQLITE_PREPARE_PERSISTENT, &statement, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s: prepare failed: %s\n", context, sqlite3_errmsg(database));
        return 1;
    }
    rc = sqlite3_step(statement);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "%s: expected one row, got rc=%d\n", context, rc);
        sqlite3_finalize(statement);
        return 1;
    }
    if (sqlite3_column_count(statement) != 1) {
        fprintf(
            stderr,
            "%s: expected one column, got %d\n",
            context,
            sqlite3_column_count(statement)
        );
        sqlite3_finalize(statement);
        return 1;
    }

    *out_statement = statement;
    return 0;
}

static int finish_single_row(sqlite3_stmt *statement, const char *context) {
    int rc = sqlite3_step(statement);
    sqlite3 *database = sqlite3_db_handle(statement);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "%s: expected end of results, got rc=%d\n", context, rc);
        sqlite3_finalize(statement);
        return 1;
    }
    return expect_sqlite_ok(sqlite3_finalize(statement), database, context);
}

static int expect_sqlite_ok(int rc, sqlite3 *database, const char *context) {
    if (rc == SQLITE_OK) {
        return 0;
    }
    fprintf(stderr, "%s: sqlite rc=%d: %s\n", context, rc, sqlite3_errmsg(database));
    return 1;
}

static int expect_mylite_ok(int status, mylite_db *database, const char *context) {
    if (status == MYLITE_OK) {
        return 0;
    }
    return expect_mylite_status(status, MYLITE_OK, database, context);
}

static int expect_mylite_status(
    int status,
    int expected,
    mylite_db *database,
    const char *context
) {
    if (status == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected mylite status=%s, got %s: %s\n",
        context,
        mylite_status_name(expected),
        mylite_status_name(status),
        database == NULL ? "(no database)" : mylite_error_message(database)
    );
    return 1;
}

static int expect_mylite_sql_status(
    mylite_db *database,
    const char *sql,
    int expected,
    const char *context
) {
    mylite_stmt *statement = NULL;
    int failures =
        expect_mylite_ok(mylite_prepare(database, sql, strlen(sql), &statement), database, context);

    if (failures == 0) {
        failures += expect_mylite_status(mylite_step(statement), expected, database, context);
    }
    if (statement != NULL) {
        mylite_finalize(statement);
    }
    return failures;
}
