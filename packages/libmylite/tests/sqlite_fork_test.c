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

static int test_native_constraint_diagnostics(void);

static int test_native_type_coercion(void);

static int test_native_binary_type_coercion(void);

static int test_native_text_blob_family_coercion(void);

static int test_native_decimal_type_coercion(void);

static int test_native_temporal_type_coercion(void);

static int test_native_timestamp_type_coercion(void);

static int test_native_time_type_coercion(void);

static int test_native_year_type_coercion(void);

static int test_native_bit_type_coercion(void);

static int test_native_json_type_coercion(void);

static int test_native_enum_type_coercion(void);

static int test_native_set_type_coercion(void);

static int test_wordpress_like_crud(void);

static int test_mylite_wordpress_like_crud(void);

static int test_mylite_basic_type_coercion(void);

static int test_mylite_binary_type_coercion(void);

static int test_mylite_text_blob_family_coercion(void);

static int test_mylite_decimal_type_coercion(void);

static int test_mylite_temporal_type_coercion(void);

static int test_mylite_timestamp_type_coercion(void);

static int test_mylite_time_type_coercion(void);

static int test_mylite_year_type_coercion(void);

static int test_mylite_bit_type_coercion(void);

static int test_mylite_select_descriptor_hydration(void);

static int test_mylite_json_type_coercion(void);

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

static void remove_sqlite_fork_test_files(void);

int main(void) {
    int failures = 0;

    failures += test_registered_functions();
    failures += test_mysql_collations();
    failures += test_native_constraint_diagnostics();
    failures += test_native_type_coercion();
    failures += test_native_binary_type_coercion();
    failures += test_native_text_blob_family_coercion();
    failures += test_native_decimal_type_coercion();
    failures += test_native_temporal_type_coercion();
    failures += test_native_timestamp_type_coercion();
    failures += test_native_time_type_coercion();
    failures += test_native_year_type_coercion();
    failures += test_native_bit_type_coercion();
    failures += test_native_json_type_coercion();
    failures += test_native_enum_type_coercion();
    failures += test_native_set_type_coercion();
    failures += test_wordpress_like_crud();
    failures += test_mylite_wordpress_like_crud();
    failures += test_mylite_basic_type_coercion();
    failures += test_mylite_binary_type_coercion();
    failures += test_mylite_text_blob_family_coercion();
    failures += test_mylite_decimal_type_coercion();
    failures += test_mylite_temporal_type_coercion();
    failures += test_mylite_timestamp_type_coercion();
    failures += test_mylite_time_type_coercion();
    failures += test_mylite_year_type_coercion();
    failures += test_mylite_bit_type_coercion();
    failures += test_mylite_select_descriptor_hydration();
    failures += test_mylite_json_type_coercion();
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
    failures += expect_int64(
        database,
        "PRAGMA foreign_keys",
        1,
        "configured fork connections enforce foreign keys"
    );

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

static int test_native_constraint_diagnostics(void) {
    enum {
        not_null_error = 1048,
        duplicate_error = 1062,
        fk_parent_error = 1451,
        fk_child_error = 1452,
        check_error = 3819,
    };

    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += exec_sql(
        database,
        "CREATE TABLE constraint_diag("
        "id INT NOT NULL,"
        "slug TEXT NOT NULL UNIQUE,"
        "title TEXT NOT NULL,"
        "PRIMARY KEY(id)"
        ")",
        "create direct constraint diagnostics fixture"
    );
    failures += exec_sql(
        database,
        "INSERT INTO constraint_diag VALUES (1, 'home', 'Home')",
        "insert direct constraint diagnostics seed"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO constraint_diag VALUES (2, NULL, 'Missing slug')",
            .message_fragment = "NOT NULL constraint failed",
            .context = "direct NOT NULL constraint fails",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = not_null_error,
            .sqlstate = "23000",
            .context = "direct NOT NULL constraint publishes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear NOT NULL constraint condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO constraint_diag VALUES (1, 'about', 'Duplicate primary')",
            .message_fragment = "UNIQUE constraint failed",
            .context = "direct primary key constraint fails",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = duplicate_error,
            .sqlstate = "23000",
            .context = "direct primary key constraint publishes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear primary key constraint condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO constraint_diag VALUES (2, 'home', 'Duplicate slug')",
            .message_fragment = "UNIQUE constraint failed",
            .context = "direct unique constraint fails",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = duplicate_error,
            .sqlstate = "23000",
            .context = "direct unique constraint publishes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear unique constraint condition"
    );
    failures += exec_sql(
        database,
        "INSERT INTO constraint_diag VALUES (2, 'about', 'About')",
        "insert direct constraint diagnostics update seed"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "UPDATE constraint_diag SET slug = NULL WHERE id = 2",
            .message_fragment = "NOT NULL constraint failed",
            .context = "direct update NOT NULL constraint fails",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = not_null_error,
            .sqlstate = "23000",
            .context = "direct update NOT NULL publishes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear update NOT NULL constraint condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "UPDATE constraint_diag SET slug = 'home' WHERE id = 2",
            .message_fragment = "UNIQUE constraint failed",
            .context = "direct update unique constraint fails",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = duplicate_error,
            .sqlstate = "23000",
            .context = "direct update unique publishes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear update unique constraint condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "UPDATE constraint_diag SET id = 1 WHERE slug = 'about'",
            .message_fragment = "UNIQUE constraint failed",
            .context = "direct update primary key constraint fails",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = duplicate_error,
            .sqlstate = "23000",
            .context = "direct update primary key publishes MySQL condition",
        }
    );
    failures += expect_int64(
        database,
        "SELECT COUNT(*) FROM constraint_diag",
        2,
        "failed native constraint writes preserve stored rows"
    );
    failures += exec_sql(
        database,
        "CREATE TABLE check_diag("
        "id INT PRIMARY KEY,"
        "qty INT,"
        "CONSTRAINT qty_positive CHECK(qty > 0)"
        ")",
        "create direct CHECK diagnostics fixture"
    );
    failures +=
        exec_sql(database, "INSERT INTO check_diag VALUES (1, 1)", "insert direct CHECK seed");
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO check_diag VALUES (2, 0)",
            .message_fragment = "CHECK constraint failed",
            .context = "direct CHECK constraint fails",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = check_error,
            .sqlstate = "HY000",
            .context = "direct CHECK constraint publishes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear CHECK constraint condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "UPDATE check_diag SET qty = -1 WHERE id = 1",
            .message_fragment = "CHECK constraint failed",
            .context = "direct update CHECK constraint fails",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = check_error,
            .sqlstate = "HY000",
            .context = "direct update CHECK publishes MySQL condition",
        }
    );
    failures += expect_int64(
        database,
        "SELECT COUNT(*) FROM check_diag WHERE qty = 1",
        1,
        "failed native CHECK writes preserve stored rows"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear CHECK condition before foreign key diagnostics"
    );
    failures += exec_sql(
        database,
        "CREATE TABLE fk_parent_diag(id INT PRIMARY KEY)",
        "create direct foreign key parent fixture"
    );
    failures += exec_sql(
        database,
        "CREATE TABLE fk_child_diag("
        "id INT PRIMARY KEY,"
        "parent_id INT,"
        "CONSTRAINT fk_parent FOREIGN KEY(parent_id) REFERENCES fk_parent_diag(id)"
        ")",
        "create direct foreign key child fixture"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO fk_child_diag VALUES (1, 9)",
            .message_fragment = "FOREIGN KEY constraint failed",
            .context = "direct child foreign key insert fails",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = fk_child_error,
            .sqlstate = "23000",
            .context = "direct child foreign key insert publishes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear child insert foreign key condition"
    );
    failures += exec_sql(
        database,
        "INSERT INTO fk_parent_diag VALUES (1), (2)",
        "insert direct foreign key parent seed"
    );
    failures += exec_sql(
        database,
        "INSERT INTO fk_child_diag VALUES (2, 1)",
        "insert direct foreign key child seed"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "UPDATE fk_child_diag SET parent_id = 8 WHERE id = 2",
            .message_fragment = "FOREIGN KEY constraint failed",
            .context = "direct child foreign key update fails",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = fk_child_error,
            .sqlstate = "23000",
            .context = "direct child foreign key update publishes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear child update foreign key condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "DELETE FROM fk_parent_diag WHERE id = 1",
            .message_fragment = "FOREIGN KEY constraint failed",
            .context = "direct parent foreign key delete fails",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = fk_parent_error,
            .sqlstate = "23000",
            .context = "direct parent foreign key delete publishes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear parent delete foreign key condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "UPDATE fk_parent_diag SET id = 3 WHERE id = 1",
            .message_fragment = "FOREIGN KEY constraint failed",
            .context = "direct parent foreign key update fails",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = fk_parent_error,
            .sqlstate = "23000",
            .context = "direct parent foreign key update publishes MySQL condition",
        }
    );
    failures += expect_int64(
        database,
        "SELECT COUNT(*) FROM fk_child_diag WHERE parent_id = 1",
        1,
        "failed native foreign key writes preserve child rows"
    );
    failures += expect_int64(
        database,
        "SELECT COUNT(*) FROM fk_parent_diag",
        2,
        "failed native foreign key writes preserve parent rows"
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

static int test_native_text_blob_family_coercion(void) {
    enum {
        text_byte_length = 5,
        blob_byte_length = 4,
        text_blob_too_long_error = 1406,
    };

    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += exec_sql(
        database,
        "CREATE TABLE text_blob_direct(id INTEGER PRIMARY KEY, short_text TEXT, short_blob BLOB)",
        "create direct text/blob descriptor fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "text_blob_direct",
            "short_text",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_TEXT,
                .byte_maximum_length = text_byte_length,
            }
        ),
        database,
        "set direct text-family descriptor"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "text_blob_direct",
            "short_blob",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_BLOB,
                .byte_maximum_length = blob_byte_length,
            }
        ),
        database,
        "set direct blob-family descriptor"
    );
    failures += exec_sql(
        database,
        "INSERT INTO text_blob_direct VALUES (1, 'éa', X'00FF'), (2, 65, 1234)",
        "insert direct text/blob descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat(id || ':' || hex(CAST(short_text AS BLOB)) || ':' || "
                   "length(CAST(short_text AS BLOB)) || ':' || char_length(short_text) || ':' || "
                   "hex(short_blob) || ':' || length(short_blob), '|') FROM ("
                   "SELECT id, short_text, short_blob FROM text_blob_direct ORDER BY id)",
            .expected = "1:C3A961:3:2:00FF:2|2:3635:2:2:31323334:4",
            .context = "direct text/blob descriptors coerce stored values",
        }
    );
    failures += exec_sql(
        database,
        "UPDATE text_blob_direct SET short_text = 'éé', short_blob = 'xy' WHERE id = 2",
        "update direct text/blob descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT hex(CAST(short_text AS BLOB)) || ':' || "
                   "length(CAST(short_text AS BLOB)) || ':' || char_length(short_text) || ':' || "
                   "hex(short_blob) || ':' || length(short_blob) "
                   "FROM text_blob_direct WHERE id = 2",
            .expected = "C3A9C3A9:4:2:7879:2",
            .context = "direct text/blob update coerces assigned values",
        }
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO text_blob_direct VALUES (3, 'ééé', X'6F6B')",
            .message_fragment = "text value is too long",
            .context = "direct text-family descriptor rejects over-byte-length value",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = text_blob_too_long_error,
            .sqlstate = "22001",
            .context = "over-length text family exposes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear text-family fork condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO text_blob_direct VALUES (3, 'ok', X'0102030405')",
            .message_fragment = "blob value is too long",
            .context = "direct blob-family descriptor rejects over-length value",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = text_blob_too_long_error,
            .sqlstate = "22001",
            .context = "over-length blob family exposes MySQL condition",
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

static int test_native_timestamp_type_coercion(void) {
    enum {
        invalid_timestamp_error = 1292,
        timestamp_millisecond_precision = 3,
        timestamp_microsecond_precision = 6,
    };

    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += exec_sql(
        database,
        "CREATE TABLE timestamp_direct("
        "id INTEGER PRIMARY KEY, ts TEXT, ts3 TEXT, ts6 TEXT)",
        "create direct timestamp descriptor fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "timestamp_direct",
            "ts",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_TIMESTAMP,
            }
        ),
        database,
        "set direct timestamp descriptor"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "timestamp_direct",
            "ts3",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_TIMESTAMP,
                .datetime_precision = timestamp_millisecond_precision,
            }
        ),
        database,
        "set direct timestamp(3) descriptor"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "timestamp_direct",
            "ts6",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_TIMESTAMP,
                .datetime_precision = timestamp_microsecond_precision,
            }
        ),
        database,
        "set direct timestamp(6) descriptor"
    );
    failures += exec_sql(
        database,
        "INSERT INTO timestamp_direct VALUES "
        "(1, '1970-01-01 00:00:01', '1970-01-01 00:00:01.1234', "
        "'1970-01-01 00:00:01.1234567'),"
        "(2, '2038-01-19 03:14:07', '2038-01-19 03:14:07.9994', "
        "'2038-01-19 03:14:07.999999'),"
        "(3, 20240229123456, '2024-02-29 12:34:56.7896', NULL)",
        "insert direct timestamp descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat(id || ':' || ts || ':' || ts3 || ':' || "
                   "COALESCE(ts6, 'NULL'), '|') FROM ("
                   "SELECT id, ts, ts3, ts6 FROM timestamp_direct ORDER BY id)",
            .expected = "1:1970-01-01 00:00:01:1970-01-01 00:00:01.123:"
                        "1970-01-01 00:00:01.123457|"
                        "2:2038-01-19 03:14:07:2038-01-19 03:14:07.999:"
                        "2038-01-19 03:14:07.999999|"
                        "3:2024-02-29 12:34:56:2024-02-29 12:34:56.790:NULL",
            .context = "direct timestamp descriptors normalize stored text",
        }
    );
    failures += exec_sql(
        database,
        "UPDATE timestamp_direct SET ts = '2038-01-19 03:14:07', "
        "ts3 = '2038-01-19 03:14:07.9994', "
        "ts6 = '2038-01-19 03:14:07.999999' WHERE id = 3",
        "update direct timestamp descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT ts || ':' || ts3 || ':' || ts6 FROM timestamp_direct WHERE id = 3",
            .expected = "2038-01-19 03:14:07:2038-01-19 03:14:07.999:"
                        "2038-01-19 03:14:07.999999",
            .context = "direct timestamp update coerces assigned values",
        }
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO timestamp_direct VALUES "
                   "(4, '1970-01-01 00:00:00', '1970-01-01 00:00:01', "
                   "'1970-01-01 00:00:01')",
            .message_fragment = "invalid timestamp value",
            .context = "direct timestamp descriptor rejects lower bound",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = invalid_timestamp_error,
            .sqlstate = "22007",
            .context = "lower-bound timestamp exposes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear lower-bound timestamp fork condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO timestamp_direct VALUES "
                   "(4, '2038-01-19 03:14:08', '2038-01-19 03:14:07', "
                   "'2038-01-19 03:14:07')",
            .message_fragment = "invalid timestamp value",
            .context = "direct timestamp descriptor rejects upper bound",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = invalid_timestamp_error,
            .sqlstate = "22007",
            .context = "upper-bound timestamp exposes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear upper-bound timestamp fork condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO timestamp_direct VALUES "
                   "(4, '2038-01-19 03:14:07.5', '2038-01-19 03:14:07', "
                   "'2038-01-19 03:14:07')",
            .message_fragment = "invalid timestamp value",
            .context = "direct timestamp descriptor rejects rounded overflow",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = invalid_timestamp_error,
            .sqlstate = "22007",
            .context = "rounded timestamp overflow exposes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear rounded timestamp overflow fork condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO timestamp_direct VALUES "
                   "(4, '2038-01-19 03:14:07', '2038-01-19 03:14:07.9995', "
                   "'2038-01-19 03:14:07.999999')",
            .message_fragment = "invalid timestamp value",
            .context = "direct timestamp(3) descriptor rejects rounded overflow",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = invalid_timestamp_error,
            .sqlstate = "22007",
            .context = "rounded timestamp(3) overflow exposes MySQL condition",
        }
    );

    sqlite3_close(database);
    return failures;
}

static int test_native_time_type_coercion(void) {
    enum {
        invalid_time_error = 1292,
        time_millisecond_precision = 3,
        time_microsecond_precision = 6,
    };

    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += exec_sql(
        database,
        "CREATE TABLE time_direct(id INTEGER PRIMARY KEY, t TEXT, t3 TEXT, t6 TEXT)",
        "create direct time descriptor fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "time_direct",
            "t",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_TIME,
            }
        ),
        database,
        "set direct time descriptor"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "time_direct",
            "t3",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_TIME,
                .datetime_precision = time_millisecond_precision,
            }
        ),
        database,
        "set direct time(3) descriptor"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "time_direct",
            "t6",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_TIME,
                .datetime_precision = time_microsecond_precision,
            }
        ),
        database,
        "set direct time(6) descriptor"
    );
    failures += exec_sql(
        database,
        "INSERT INTO time_direct VALUES "
        "(1, '12:34:56', '12:34:56.7896', '12:34:56.1234567'),"
        "(2, '-12:34:56', '-12:34:56.7896', '-12:34:56.1234567'),"
        "(3, '1 02:03:04', '1 02:03:04.5678', '1 02:03:04.123456'),"
        "(4, 123456, 123456.789, 123456.123456),"
        "(5, 1234, 1234.5, 1234.123456),"
        "(6, 12, 12.9, 12.123456),"
        "(7, '34:56', '34:56.7894', '34:56.789456'),"
        "(8, ':12', '1:2:3.4567', '-00:00:00.0000004'),"
        "(9, '838:59:58.5', '838:59:58.9995', '838:59:58.9999995')",
        "insert direct time descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat(id || ':' || t || ':' || t3 || ':' || t6, '|') "
                   "FROM (SELECT id, t, t3, t6 FROM time_direct ORDER BY id)",
            .expected = "1:12:34:56:12:34:56.790:12:34:56.123457|"
                        "2:-12:34:56:-12:34:56.790:-12:34:56.123457|"
                        "3:26:03:04:26:03:04.568:26:03:04.123456|"
                        "4:12:34:56:12:34:56.789:12:34:56.123456|"
                        "5:00:12:34:00:12:34.500:00:12:34.123456|"
                        "6:00:00:12:00:00:12.900:00:00:12.123456|"
                        "7:34:56:00:34:56:00.789:34:56:00.789456|"
                        "8:00:12:00:01:02:03.457:00:00:00.000000|"
                        "9:838:59:59:838:59:59.000:838:59:59.000000",
            .context = "direct time descriptors normalize stored text",
        }
    );
    failures += exec_sql(
        database,
        "UPDATE time_direct SET t = '23:59:59.9', "
        "t3 = '-23:59:59.9994', t6 = '838:59:58.999999' WHERE id = 1",
        "update direct time descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT t || ':' || t3 || ':' || t6 FROM time_direct WHERE id = 1",
            .expected = "24:00:00:-23:59:59.999:838:59:58.999999",
            .context = "direct time update coerces assigned values",
        }
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO time_direct VALUES (10, '838:59:59.000001', "
                   "'00:00:00', '00:00:00')",
            .message_fragment = "invalid time value",
            .context = "direct time descriptor rejects out-of-range values",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = invalid_time_error,
            .sqlstate = "22007",
            .context = "out-of-range time exposes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear out-of-range time fork condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO time_direct VALUES (10, '12:60:00', "
                   "'00:00:00', '00:00:00')",
            .message_fragment = "invalid time value",
            .context = "direct time descriptor rejects malformed values",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = invalid_time_error,
            .sqlstate = "22007",
            .context = "malformed time exposes MySQL condition",
        }
    );

    sqlite3_close(database);
    return failures;
}

static int test_native_year_type_coercion(void) {
    enum {
        year_out_of_range_error = 1264,
        invalid_year_error = 1366,
    };

    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += exec_sql(
        database,
        "CREATE TABLE year_direct(id INTEGER PRIMARY KEY, y TEXT)",
        "create direct year descriptor fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "year_direct",
            "y",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_YEAR,
            }
        ),
        database,
        "set direct year descriptor"
    );
    failures += exec_sql(
        database,
        "INSERT INTO year_direct VALUES "
        "(1, 0), (2, '0'), (3, '00'), (4, '0000'), (5, 1), "
        "(6, '69'), (7, 70), (8, 1901.5), (9, 2155)",
        "insert direct year descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat(id || ':' || y, '|') "
                   "FROM (SELECT id, y FROM year_direct ORDER BY id)",
            .expected = "1:0000|2:2000|3:2000|4:0000|5:2001|6:2069|7:1970|8:1902|9:2155",
            .context = "direct year descriptor normalizes stored text",
        }
    );
    failures += exec_sql(
        database,
        "UPDATE year_direct SET y = '2' WHERE id = 1",
        "update direct quoted year"
    );
    failures += exec_sql(
        database,
        "UPDATE year_direct SET y = 98 WHERE id = 2",
        "update direct numeric year"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat(id || ':' || y, '|') "
                   "FROM (SELECT id, y FROM year_direct WHERE id IN (1, 2) ORDER BY id)",
            .expected = "1:2002|2:1998",
            .context = "direct year update coerces assigned values",
        }
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO year_direct VALUES (10, 100)",
            .message_fragment = "year value is out of range",
            .context = "direct year descriptor rejects out-of-range value",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = year_out_of_range_error,
            .sqlstate = "22003",
            .context = "out-of-range year exposes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear out-of-range year fork condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO year_direct VALUES (10, 'bad')",
            .message_fragment = "invalid year value",
            .context = "direct year descriptor rejects invalid text",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = invalid_year_error,
            .sqlstate = "HY000",
            .context = "invalid year text exposes MySQL condition",
        }
    );

    sqlite3_close(database);
    return failures;
}

static int test_native_bit_type_coercion(void) {
    enum {
        bit1_precision = 1,
        bit4_precision = 4,
        bit9_precision = 9,
        bit64_precision = 64,
        bit_out_of_range_error = 1264,
        bit_too_long_error = 1406,
    };

    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += exec_sql(
        database,
        "CREATE TABLE bit_direct("
        "id INTEGER PRIMARY KEY, b1 INTEGER, b4 INTEGER, b9 INTEGER, b64 INTEGER)",
        "create direct bit descriptor fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "bit_direct",
            "b1",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_BIT,
                .numeric_precision = bit1_precision,
            }
        ),
        database,
        "set direct bit(1) descriptor"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "bit_direct",
            "b4",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_BIT,
                .numeric_precision = bit4_precision,
            }
        ),
        database,
        "set direct bit(4) descriptor"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "bit_direct",
            "b9",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_BIT,
                .numeric_precision = bit9_precision,
            }
        ),
        database,
        "set direct bit(9) descriptor"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "bit_direct",
            "b64",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_BIT,
                .numeric_precision = bit64_precision,
            }
        ),
        database,
        "set direct bit(64) descriptor"
    );
    failures += exec_sql(
        database,
        "INSERT INTO bit_direct VALUES "
        "(1, 1, x'05', x'0155', x'ffffffffffffffff'),"
        "(2, 0, 15.4, x'0001', x'0000000000000001')",
        "insert direct bit descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat(id || ':' || hex(b1) || ':' || length(b1) || ':' || "
                   "(b1 + 0) || ':' || hex(b4) || ':' || length(b4) || ':' || (b4 + 0) || ':' || "
                   "hex(b9) || ':' || length(b9) || ':' || (b9 + 0) || ':' || "
                   "hex(b64) || ':' || length(b64), '|') "
                   "FROM (SELECT id, b1, b4, b9, b64 FROM bit_direct ORDER BY id)",
            .expected = "1:01:1:1:05:1:5:0155:2:341:FFFFFFFFFFFFFFFF:8|"
                        "2:00:1:0:0F:1:15:0001:2:1:0000000000000001:8",
            .context = "direct bit descriptors expose binary display and numeric context",
        }
    );
    failures += exec_sql(
        database,
        "UPDATE bit_direct SET b4 = x'0c', b9 = 257 WHERE id = 2",
        "update direct bit descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT hex(b4) || ':' || (b4 + 0) || ':' || hex(b9) || ':' || (b9 + 0) "
                   "FROM bit_direct WHERE id = 2",
            .expected = "0C:12:0101:257",
            .context = "direct bit update coerces assigned values",
        }
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO bit_direct VALUES (3, 1, 16, 0, 0)",
            .message_fragment = "bit value is too long",
            .context = "direct bit descriptor rejects oversized values",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = bit_too_long_error,
            .sqlstate = "22001",
            .context = "oversized bit value exposes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear oversized bit fork condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO bit_direct VALUES (3, 1, -1, 0, 0)",
            .message_fragment = "bit value is out of range",
            .context = "direct bit descriptor rejects negative values",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = bit_out_of_range_error,
            .sqlstate = "22003",
            .context = "negative bit value exposes MySQL condition",
        }
    );

    sqlite3_close(database);
    return failures;
}

static int test_native_json_type_coercion(void) {
    enum {
        json_error = 3140,
    };

    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += exec_sql(
        database,
        "CREATE TABLE json_direct(id INTEGER PRIMARY KEY, doc TEXT)",
        "create direct JSON descriptor fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_column_type(
            database,
            NULL,
            "json_direct",
            "doc",
            &(const struct mylite_sqlite_fork_column_type){
                .kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_JSON,
            }
        ),
        database,
        "set direct JSON descriptor"
    );
    failures += exec_sql(
        database,
        "INSERT INTO json_direct VALUES "
        "(1, '{\"a\":1}'), (2, '[1,2]'), (3, 'true'), "
        "(4, 'null'), (5, '123'), (6, '\"x\"')",
        "insert direct JSON descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT COUNT(*) || ':' || MIN(json_valid(doc)) || ':' || "
                   "json_extract((SELECT doc FROM json_direct WHERE id = 1), '$.a') || ':' || "
                   "json_array_length((SELECT doc FROM json_direct WHERE id = 2)) "
                   "FROM json_direct",
            .expected = "6:1:1:2",
            .context = "direct JSON descriptors accept canonical JSON text",
        }
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO json_direct VALUES (7, 'not json')",
            .message_fragment = "invalid JSON text",
            .context = "direct JSON descriptor rejects invalid text",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = json_error,
            .sqlstate = "22032",
            .context = "invalid JSON text exposes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear invalid JSON text fork condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO json_direct VALUES (7, 1)",
            .message_fragment = "invalid JSON text",
            .context = "direct JSON descriptor rejects non-text JSON assignment",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = json_error,
            .sqlstate = "22032",
            .context = "non-text JSON assignment exposes MySQL condition",
        }
    );

    sqlite3_close(database);
    return failures;
}

static int test_native_enum_type_coercion(void) {
    enum {
        enum_truncated_error = 1265,
    };

    static const struct mylite_sqlite_fork_enum_value status_values[] = {
        {.text = "draft", .byte_length = sizeof("draft") - 1},
        {.text = "published", .byte_length = sizeof("published") - 1},
        {.text = "archived", .byte_length = sizeof("archived") - 1},
    };
    static const struct mylite_sqlite_fork_enum_value numeric_values[] = {
        {.text = "0", .byte_length = sizeof("0") - 1},
        {.text = "1", .byte_length = sizeof("1") - 1},
        {.text = "2", .byte_length = sizeof("2") - 1},
    };
    static const struct mylite_sqlite_fork_enum_value empty_values[] = {
        {.text = "", .byte_length = 0},
        {.text = "a", .byte_length = sizeof("a") - 1},
    };

    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += exec_sql(
        database,
        "CREATE TABLE enum_direct(id INTEGER PRIMARY KEY, status INTEGER)",
        "create direct enum descriptor fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_enum_column_type(
            database,
            NULL,
            "enum_direct",
            "status",
            &(const struct mylite_sqlite_fork_enum_column_type){
                .values = status_values,
                .value_count = sizeof(status_values) / sizeof(status_values[0]),
            }
        ),
        database,
        "set direct enum descriptor"
    );
    failures += exec_sql(
        database,
        "INSERT INTO enum_direct VALUES "
        "(1, 'draft'), (2, 2), (3, '2'), (4, 3.9), (5, NULL)",
        "insert direct enum descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat("
                   "id || ':' || COALESCE(status, 'NULL') || ':' || "
                   "COALESCE(status + 0, 'NULL'), '|') "
                   "FROM (SELECT id, status FROM enum_direct ORDER BY id)",
            .expected = "1:draft:1|2:published:2|3:published:2|4:archived:3|5:NULL:NULL",
            .context = "direct enum descriptor returns labels with numeric indexes",
        }
    );
    failures += exec_sql(
        database,
        "UPDATE enum_direct SET status = 'archived' WHERE id = 1",
        "update direct enum label"
    );
    failures += exec_sql(
        database,
        "UPDATE enum_direct SET status = '+2' WHERE id = 2",
        "update direct enum signed integer text"
    );
    failures += exec_sql(
        database,
        "UPDATE enum_direct SET status = '02' WHERE id = 3",
        "update direct enum zero-padded integer text"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat(id || ':' || status || ':' || (status + 0), '|') "
                   "FROM (SELECT id, status FROM enum_direct WHERE id IN (1, 2, 3) "
                   "ORDER BY id)",
            .expected = "1:archived:3|2:published:2|3:published:2",
            .context = "direct enum update coerces labels and integer text",
        }
    );

    failures += exec_sql(
        database,
        "CREATE TABLE enum_numeric(id INTEGER PRIMARY KEY, value INTEGER)",
        "create direct numeric-label enum fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_enum_column_type(
            database,
            NULL,
            "enum_numeric",
            "value",
            &(const struct mylite_sqlite_fork_enum_column_type){
                .values = numeric_values,
                .value_count = sizeof(numeric_values) / sizeof(numeric_values[0]),
            }
        ),
        database,
        "set direct numeric-label enum descriptor"
    );
    failures += exec_sql(
        database,
        "INSERT INTO enum_numeric VALUES "
        "(1, 2), (2, '2'), (3, '3'), (4, 1), (5, '1'), (6, '0')",
        "insert direct numeric-label enum values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat(id || ':' || value || ':' || (value + 0), '|') "
                   "FROM (SELECT id, value FROM enum_numeric ORDER BY id)",
            .expected = "1:1:2|2:2:3|3:2:3|4:0:1|5:1:2|6:0:1",
            .context = "direct enum prefers exact labels before numeric indexes",
        }
    );

    failures += exec_sql(
        database,
        "CREATE TABLE enum_empty(id INTEGER PRIMARY KEY, value INTEGER)",
        "create direct empty-label enum fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_enum_column_type(
            database,
            NULL,
            "enum_empty",
            "value",
            &(const struct mylite_sqlite_fork_enum_column_type){
                .values = empty_values,
                .value_count = sizeof(empty_values) / sizeof(empty_values[0]),
            }
        ),
        database,
        "set direct empty-label enum descriptor"
    );
    failures += exec_sql(
        database,
        "INSERT INTO enum_empty VALUES (1, ''), (2, 1), (3, '1')",
        "insert empty-label enum values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat(id || ':' || value || ':' || (value + 0), '|') "
                   "FROM (SELECT id, value FROM enum_empty ORDER BY id)",
            .expected = "1::1|2::1|3::1",
            .context = "direct enum preserves valid empty labels as index one",
        }
    );

    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO enum_direct VALUES (6, 0)",
            .message_fragment = "invalid enum value",
            .context = "direct enum descriptor rejects index zero in strict mode",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = enum_truncated_error,
            .sqlstate = "01000",
            .context = "invalid enum index exposes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear invalid enum index fork condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO enum_direct VALUES (6, 'missing')",
            .message_fragment = "invalid enum value",
            .context = "direct enum descriptor rejects unknown labels in strict mode",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = enum_truncated_error,
            .sqlstate = "01000",
            .context = "invalid enum label exposes MySQL condition",
        }
    );
    failures += exec_sql(
        database,
        "CREATE TABLE enum_alter(id INTEGER PRIMARY KEY, value INTEGER)",
        "create direct enum alter fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_enum_column_type(
            database,
            NULL,
            "enum_alter",
            "value",
            &(const struct mylite_sqlite_fork_enum_column_type){
                .values = status_values,
                .value_count = sizeof(status_values) / sizeof(status_values[0]),
            }
        ),
        database,
        "set direct enum alter descriptor"
    );
    failures += exec_sql(
        database,
        "ALTER TABLE enum_alter ADD COLUMN note TEXT",
        "native SQLite alter table does not double-free enum descriptors"
    );

    sqlite3_close(database);
    return failures;
}

static int test_native_set_type_coercion(void) {
    enum {
        set_truncated_error = 1265,
    };

    static const struct mylite_sqlite_fork_enum_value flag_values[] = {
        {.text = "a", .byte_length = sizeof("a") - 1},
        {.text = "b", .byte_length = sizeof("b") - 1},
        {.text = "c", .byte_length = sizeof("c") - 1},
        {.text = "d", .byte_length = sizeof("d") - 1},
    };
    static const struct mylite_sqlite_fork_enum_value numeric_values[] = {
        {.text = "0", .byte_length = sizeof("0") - 1},
        {.text = "1", .byte_length = sizeof("1") - 1},
        {.text = "2", .byte_length = sizeof("2") - 1},
    };
    static const struct mylite_sqlite_fork_enum_value empty_values[] = {
        {.text = "", .byte_length = 0},
        {.text = "a", .byte_length = sizeof("a") - 1},
    };
    static const struct mylite_sqlite_fork_enum_value wide_values[] = {
        {.text = "v1", .byte_length = sizeof("v1") - 1},
        {.text = "v2", .byte_length = sizeof("v2") - 1},
        {.text = "v3", .byte_length = sizeof("v3") - 1},
        {.text = "v4", .byte_length = sizeof("v4") - 1},
        {.text = "v5", .byte_length = sizeof("v5") - 1},
        {.text = "v6", .byte_length = sizeof("v6") - 1},
        {.text = "v7", .byte_length = sizeof("v7") - 1},
        {.text = "v8", .byte_length = sizeof("v8") - 1},
        {.text = "v9", .byte_length = sizeof("v9") - 1},
        {.text = "v10", .byte_length = sizeof("v10") - 1},
        {.text = "v11", .byte_length = sizeof("v11") - 1},
        {.text = "v12", .byte_length = sizeof("v12") - 1},
        {.text = "v13", .byte_length = sizeof("v13") - 1},
        {.text = "v14", .byte_length = sizeof("v14") - 1},
        {.text = "v15", .byte_length = sizeof("v15") - 1},
        {.text = "v16", .byte_length = sizeof("v16") - 1},
        {.text = "v17", .byte_length = sizeof("v17") - 1},
        {.text = "v18", .byte_length = sizeof("v18") - 1},
        {.text = "v19", .byte_length = sizeof("v19") - 1},
        {.text = "v20", .byte_length = sizeof("v20") - 1},
        {.text = "v21", .byte_length = sizeof("v21") - 1},
        {.text = "v22", .byte_length = sizeof("v22") - 1},
        {.text = "v23", .byte_length = sizeof("v23") - 1},
        {.text = "v24", .byte_length = sizeof("v24") - 1},
        {.text = "v25", .byte_length = sizeof("v25") - 1},
        {.text = "v26", .byte_length = sizeof("v26") - 1},
        {.text = "v27", .byte_length = sizeof("v27") - 1},
        {.text = "v28", .byte_length = sizeof("v28") - 1},
        {.text = "v29", .byte_length = sizeof("v29") - 1},
        {.text = "v30", .byte_length = sizeof("v30") - 1},
        {.text = "v31", .byte_length = sizeof("v31") - 1},
        {.text = "v32", .byte_length = sizeof("v32") - 1},
        {.text = "v33", .byte_length = sizeof("v33") - 1},
        {.text = "v34", .byte_length = sizeof("v34") - 1},
        {.text = "v35", .byte_length = sizeof("v35") - 1},
        {.text = "v36", .byte_length = sizeof("v36") - 1},
        {.text = "v37", .byte_length = sizeof("v37") - 1},
        {.text = "v38", .byte_length = sizeof("v38") - 1},
        {.text = "v39", .byte_length = sizeof("v39") - 1},
        {.text = "v40", .byte_length = sizeof("v40") - 1},
        {.text = "v41", .byte_length = sizeof("v41") - 1},
        {.text = "v42", .byte_length = sizeof("v42") - 1},
        {.text = "v43", .byte_length = sizeof("v43") - 1},
        {.text = "v44", .byte_length = sizeof("v44") - 1},
        {.text = "v45", .byte_length = sizeof("v45") - 1},
        {.text = "v46", .byte_length = sizeof("v46") - 1},
        {.text = "v47", .byte_length = sizeof("v47") - 1},
        {.text = "v48", .byte_length = sizeof("v48") - 1},
        {.text = "v49", .byte_length = sizeof("v49") - 1},
        {.text = "v50", .byte_length = sizeof("v50") - 1},
        {.text = "v51", .byte_length = sizeof("v51") - 1},
        {.text = "v52", .byte_length = sizeof("v52") - 1},
        {.text = "v53", .byte_length = sizeof("v53") - 1},
        {.text = "v54", .byte_length = sizeof("v54") - 1},
        {.text = "v55", .byte_length = sizeof("v55") - 1},
        {.text = "v56", .byte_length = sizeof("v56") - 1},
        {.text = "v57", .byte_length = sizeof("v57") - 1},
        {.text = "v58", .byte_length = sizeof("v58") - 1},
        {.text = "v59", .byte_length = sizeof("v59") - 1},
        {.text = "v60", .byte_length = sizeof("v60") - 1},
        {.text = "v61", .byte_length = sizeof("v61") - 1},
        {.text = "v62", .byte_length = sizeof("v62") - 1},
        {.text = "v63", .byte_length = sizeof("v63") - 1},
        {.text = "v64", .byte_length = sizeof("v64") - 1},
    };

    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += exec_sql(
        database,
        "CREATE TABLE set_direct(id INTEGER PRIMARY KEY, flags INTEGER)",
        "create direct set descriptor fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_set_column_type(
            database,
            NULL,
            "set_direct",
            "flags",
            &(const struct mylite_sqlite_fork_set_column_type){
                .values = flag_values,
                .value_count = sizeof(flag_values) / sizeof(flag_values[0]),
            }
        ),
        database,
        "set direct set descriptor"
    );
    failures += exec_sql(
        database,
        "INSERT INTO set_direct VALUES "
        "(1, 'a'), (2, 'a,b'), (3, 'b,a'), (4, 'a,a'), (5, ''), "
        "(6, 9), (7, 3.9), (8, NULL), (9, '3'), (10, '+3'), "
        "(11, '03'), (12, ' 3')",
        "insert direct set descriptor values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat("
                   "id || ':' || COALESCE(flags, 'NULL') || ':' || "
                   "COALESCE(flags + 0, 'NULL'), '|') "
                   "FROM (SELECT id, flags FROM set_direct ORDER BY id)",
            .expected = "1:a:1|2:a,b:3|3:a,b:3|4:a:1|5::0|6:a,d:9|"
                        "7:a,b:3|8:NULL:NULL|9:a,b:3|10:a,b:3|"
                        "11:a,b:3|12:a,b:3",
            .context = "direct set descriptor returns labels with numeric masks",
        }
    );
    failures += exec_sql(
        database,
        "UPDATE set_direct SET flags = 'd,a,d' WHERE id = 1",
        "update direct set duplicate ordered labels"
    );
    failures += exec_sql(
        database,
        "UPDATE set_direct SET flags = 15 WHERE id = 2",
        "update direct set full bitmask"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat(id || ':' || flags || ':' || (flags + 0), '|') "
                   "FROM (SELECT id, flags FROM set_direct WHERE id IN (1, 2) "
                   "ORDER BY id)",
            .expected = "1:a,d:9|2:a,b,c,d:15",
            .context = "direct set update normalizes labels in definition order",
        }
    );

    failures += exec_sql(
        database,
        "CREATE TABLE set_numeric(id INTEGER PRIMARY KEY, flags INTEGER)",
        "create direct numeric-label set fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_set_column_type(
            database,
            NULL,
            "set_numeric",
            "flags",
            &(const struct mylite_sqlite_fork_set_column_type){
                .values = numeric_values,
                .value_count = sizeof(numeric_values) / sizeof(numeric_values[0]),
            }
        ),
        database,
        "set direct numeric-label set descriptor"
    );
    failures += exec_sql(
        database,
        "INSERT INTO set_numeric VALUES "
        "(1, 2), (2, '2'), (3, '3'), (4, 1), "
        "(5, '1'), (6, '0'), (7, '0,2'), (8, '2,0')",
        "insert direct numeric-label set values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat(id || ':' || flags || ':' || (flags + 0), '|') "
                   "FROM (SELECT id, flags FROM set_numeric ORDER BY id)",
            .expected = "1:1:2|2:2:4|3:0,1:3|4:0:1|5:1:2|"
                        "6:0:1|7:0,2:5|8:0,2:5",
            .context = "direct set prefers exact numeric labels before masks",
        }
    );

    failures += exec_sql(
        database,
        "CREATE TABLE set_empty(id INTEGER PRIMARY KEY, flags INTEGER)",
        "create direct empty-label set fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_set_column_type(
            database,
            NULL,
            "set_empty",
            "flags",
            &(const struct mylite_sqlite_fork_set_column_type){
                .values = empty_values,
                .value_count = sizeof(empty_values) / sizeof(empty_values[0]),
            }
        ),
        database,
        "set direct empty-label set descriptor"
    );
    failures += exec_sql(
        database,
        "INSERT INTO set_empty VALUES (1, ''), (2, 1), (3, '1'), "
        "(4, 'a'), (5, 2), (6, 3)",
        "insert empty-label set values"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat(id || ':' || flags || ':' || (flags + 0), '|') "
                   "FROM (SELECT id, flags FROM set_empty ORDER BY id)",
            .expected = "1::0|2::1|3::1|4:a:2|5:a:2|6:a:3",
            .context = "direct set treats empty string assignment as empty mask",
        }
    );

    failures += exec_sql(
        database,
        "CREATE TABLE set_wide(id INTEGER PRIMARY KEY, flags INTEGER)",
        "create direct 64-member set fixture"
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_set_set_column_type(
            database,
            NULL,
            "set_wide",
            "flags",
            &(const struct mylite_sqlite_fork_set_column_type){
                .values = wide_values,
                .value_count = sizeof(wide_values) / sizeof(wide_values[0]),
            }
        ),
        database,
        "set direct 64-member set descriptor"
    );
    failures += exec_sql(
        database,
        "INSERT INTO set_wide VALUES (1, '9223372036854775808')",
        "insert direct 64th set member by unsigned mask text"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT flags FROM set_wide WHERE id = 1",
            .expected = "v64",
            .context = "direct set supports the 64th MySQL member bit",
        }
    );

    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO set_direct VALUES (20, 16)",
            .message_fragment = "invalid set value",
            .context = "direct set rejects masks with bits outside the descriptor",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = set_truncated_error,
            .sqlstate = "01000",
            .context = "invalid set mask exposes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear invalid set mask fork condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO set_direct VALUES (21, 'a,missing,b')",
            .message_fragment = "invalid set value",
            .context = "direct set rejects unknown labels in strict mode",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = set_truncated_error,
            .sqlstate = "01000",
            .context = "invalid set label exposes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear invalid set label fork condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO set_direct VALUES (22, '3 ')",
            .message_fragment = "invalid set value",
            .context = "direct set rejects trailing-space numeric text",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = set_truncated_error,
            .sqlstate = "01000",
            .context = "invalid set trailing-space text exposes MySQL condition",
        }
    );
    failures += expect_sqlite_ok(
        mylite_sqlite_fork_clear_condition(database),
        database,
        "clear invalid set trailing-space text fork condition"
    );
    failures += expect_sqlite_exec_error(
        database,
        (struct expected_sqlite_error){
            .sql = "INSERT INTO set_direct VALUES (23, '3.9')",
            .message_fragment = "invalid set value",
            .context = "direct set rejects non-integer numeric text",
        }
    );
    failures += expect_fork_condition(
        database,
        (struct expected_fork_condition){
            .mysql_errno = set_truncated_error,
            .sqlstate = "01000",
            .context = "invalid set numeric text exposes MySQL condition",
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

static int test_mylite_text_blob_family_coercion(void) {
    enum {
        text_blob_column_count = 9,
        text_blob_initial_row_count = 3,
        text_blob_after_replace_row_count = 4,
        text_blob_too_long_error = 1406,
    };

    static const char *const after_insert[] = {
        "1", "255", "255", "5",   "5",   "255", "62626262", "4",   "62657461",
        "2", "254", "127", "200", "100", "254", "C3A9C3A9", "200", "C3A9C3A9",
        "3", "2",   "2",   "6",   "6",   "2",   "3635",     "6",   "31323334",
    };
    static const char *const after_update[] = {
        "1", "254", "254", "300", "300", "254", "71717171", "300", "72727272",
        "2", "254", "127", "200", "100", "254", "C3A9C3A9", "200", "C3A9C3A9",
        "3", "2",   "2",   "6",   "6",   "2",   "3635",     "6",   "31323334",
    };
    static const char *const after_duplicate_update[] = {
        "1", "254", "254", "300", "300", "254", "71717171", "300", "72727272",
        "2", "3",   "3",   "7",   "7",   "2",   "7576",     "2",   "7778",
        "3", "2",   "2",   "6",   "6",   "2",   "3635",     "6",   "31323334",
    };
    static const char *const after_replace[] = {
        "1", "254", "254", "300", "300", "254", "71717171", "300", "72727272",
        "2", "3",   "3",   "7",   "7",   "2",   "7576",     "2",   "7778",
        "3", "2",   "2",   "6",   "6",   "2",   "3635",     "6",   "31323334",
        "4", "255", "255", "512", "512", "255", "75757575", "512", "76767676",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_mylite_ok(
        mylite_open_memory(&database),
        database,
        "open MyLite text/blob family coercion"
    );
    if (failures != 0) {
        return failures;
    }

    failures += exec_mylite_sql(
        database,
        "CREATE DATABASE mylite_text_blob_family CHARACTER SET utf8mb4 "
        "COLLATE utf8mb4_unicode_ci",
        "create MyLite text/blob family schema"
    );
    failures +=
        exec_mylite_sql(database, "USE mylite_text_blob_family", "use text/blob family schema");
    failures += exec_mylite_sql(
        database,
        "CREATE TABLE text_blob_basic ("
        "id INT PRIMARY KEY,"
        "tiny_text TINYTEXT NOT NULL,"
        "text_value TEXT NOT NULL,"
        "tiny_blob TINYBLOB NOT NULL,"
        "blob_value BLOB NOT NULL"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
        "create MyLite text/blob family table"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO text_blob_basic VALUES "
        "(1, REPEAT('a', 255), 'alpha', REPEAT('b', 255), 'beta'),"
        "(2, REPEAT('é', 127), REPEAT('é', 100), REPEAT('é', 127), REPEAT('é', 100)),"
        "(3, 65, 123456, 65, 123456)",
        "insert MyLite text/blob family rows"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, LENGTH(tiny_text), CHAR_LENGTH(tiny_text), "
                   "LENGTH(text_value), CHAR_LENGTH(text_value), "
                   "LENGTH(tiny_blob), LEFT(HEX(tiny_blob), 8), "
                   "LENGTH(blob_value), LEFT(HEX(blob_value), 8) "
                   "FROM text_blob_basic ORDER BY id",
            .values = after_insert,
            .column_count = text_blob_column_count,
            .row_count = text_blob_initial_row_count,
            .context = "MyLite text/blob family insert coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "UPDATE text_blob_basic SET tiny_text = REPEAT('z', 254), "
        "text_value = REPEAT('w', 300), tiny_blob = REPEAT('q', 254), "
        "blob_value = REPEAT('r', 300) WHERE id = 1",
        "update MyLite text/blob family row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, LENGTH(tiny_text), CHAR_LENGTH(tiny_text), "
                   "LENGTH(text_value), CHAR_LENGTH(text_value), "
                   "LENGTH(tiny_blob), LEFT(HEX(tiny_blob), 8), "
                   "LENGTH(blob_value), LEFT(HEX(blob_value), 8) "
                   "FROM text_blob_basic ORDER BY id",
            .values = after_update,
            .column_count = text_blob_column_count,
            .row_count = text_blob_initial_row_count,
            .context = "MyLite text/blob family update coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO text_blob_basic VALUES (2, 'dup', 'duptext', 'uv', 'wx') "
        "ON DUPLICATE KEY UPDATE tiny_text = VALUES(tiny_text), "
        "text_value = VALUES(text_value), tiny_blob = VALUES(tiny_blob), "
        "blob_value = VALUES(blob_value)",
        "insert duplicate update MyLite text/blob family row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, LENGTH(tiny_text), CHAR_LENGTH(tiny_text), "
                   "LENGTH(text_value), CHAR_LENGTH(text_value), "
                   "LENGTH(tiny_blob), LEFT(HEX(tiny_blob), 8), "
                   "LENGTH(blob_value), LEFT(HEX(blob_value), 8) "
                   "FROM text_blob_basic ORDER BY id",
            .values = after_duplicate_update,
            .column_count = text_blob_column_count,
            .row_count = text_blob_initial_row_count,
            .context = "MyLite text/blob family duplicate update coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "REPLACE INTO text_blob_basic VALUES "
        "(4, REPEAT('s', 255), REPEAT('t', 512), REPEAT('u', 255), REPEAT('v', 512))",
        "replace MyLite text/blob family row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, LENGTH(tiny_text), CHAR_LENGTH(tiny_text), "
                   "LENGTH(text_value), CHAR_LENGTH(text_value), "
                   "LENGTH(tiny_blob), LEFT(HEX(tiny_blob), 8), "
                   "LENGTH(blob_value), LEFT(HEX(blob_value), 8) "
                   "FROM text_blob_basic ORDER BY id",
            .values = after_replace,
            .column_count = text_blob_column_count,
            .row_count = text_blob_after_replace_row_count,
            .context = "MyLite text/blob family replace coercion matches MySQL fixture",
        }
    );

    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO text_blob_basic VALUES (5, REPEAT('a', 256), 'ok', 'ok', 'ok')",
        MYLITE_SQLITE_ERROR,
        "MyLite text family rejects over-byte-length ASCII values"
    );
    failures += expect_mylite_error_condition(
        database,
        text_blob_too_long_error,
        "MyLite over-length text family condition"
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO text_blob_basic VALUES (5, REPEAT('é', 128), 'ok', 'ok', 'ok')",
        MYLITE_SQLITE_ERROR,
        "MyLite text family rejects over-byte-length multibyte values"
    );
    failures += expect_mylite_error_condition(
        database,
        text_blob_too_long_error,
        "MyLite over-length multibyte text family condition"
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO text_blob_basic VALUES (5, 'ok', 'ok', REPEAT('b', 256), 'ok')",
        MYLITE_SQLITE_ERROR,
        "MyLite blob family rejects over-length values"
    );
    failures += expect_mylite_error_condition(
        database,
        text_blob_too_long_error,
        "MyLite over-length blob family condition"
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

static int test_mylite_timestamp_type_coercion(void) {
    enum {
        after_delete_column_count = 5,
        after_delete_row_count = 2,
        summary_column_count = 4,
        after_truncate_column_count = 2,
        after_reinsert_column_count = 5,
        remaining_table_column_count = 1,
        single_row_count = 1,
        invalid_timestamp_error = 1292,
    };

    static const char *const after_delete[] = {
        "1",
        "epoch-start",
        "1970-01-01 00:00:01",
        "1970-01-01 00:00:01.123",
        "1970-01-01 00:00:01.123457",
        "3",
        "ordinary",
        "2024-02-29 12:34:56",
        "2024-03-01 01:02:04.000",
        "2024-03-01 01:02:03.000001",
    };
    static const char *const summary[] = {
        "2",
        "1970-01-01 00:00:01",
        "2024-03-01 01:02:04.000",
        "epoch-start,ordinary",
    };
    static const char *const after_truncate[] = {"0", "0"};
    static const char *const after_reinsert[] = {
        "1",
        "restored",
        "2038-01-19 03:14:07",
        "2038-01-19 03:14:07.999",
        "2038-01-19 03:14:07.999999",
    };
    static const char *const remaining_tables[] = {"0"};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_mylite_ok(mylite_open_memory(&database), database, "open MyLite TIMESTAMP CRUD");
    if (failures != 0) {
        return failures;
    }

    failures += exec_mylite_sql(
        database,
        "CREATE DATABASE mylite_timestamp_column_crud "
        "CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci",
        "create MyLite TIMESTAMP CRUD schema"
    );
    failures +=
        exec_mylite_sql(database, "USE mylite_timestamp_column_crud", "use TIMESTAMP CRUD schema");
    failures += exec_mylite_sql(
        database,
        "CREATE TABLE wp_events_timestamp_like ("
        "event_id INT NOT NULL AUTO_INCREMENT,"
        "event_name VARCHAR(64) NOT NULL,"
        "created_at TIMESTAMP NOT NULL,"
        "updated_at TIMESTAMP(3) NOT NULL,"
        "seen_at TIMESTAMP(6) DEFAULT NULL,"
        "PRIMARY KEY (event_id),"
        "UNIQUE KEY event_name (event_name)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
        "create MyLite TIMESTAMP events table"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO wp_events_timestamp_like "
        "(event_name, created_at, updated_at, seen_at) VALUES "
        "('epoch-start', '1970-01-01 00:00:01', '1970-01-01 00:00:01.1234', "
        "'1970-01-01 00:00:01.1234567'),"
        "('max-edge', '2038-01-19 03:14:07', '2038-01-19 03:14:07.9994', "
        "'2038-01-19 03:14:07.999999'),"
        "('ordinary', 20240229123456, '2024-02-29 12:34:56.7896', NULL)",
        "insert MyLite TIMESTAMP event rows"
    );
    failures += exec_mylite_sql(
        database,
        "UPDATE wp_events_timestamp_like "
        "SET updated_at = '2024-03-01 01:02:03.9999', "
        "seen_at = '2024-03-01 01:02:03.000001' "
        "WHERE event_name = 'ordinary'",
        "update MyLite TIMESTAMP event row"
    );
    failures += exec_mylite_sql(
        database,
        "DELETE FROM wp_events_timestamp_like WHERE event_name = 'max-edge'",
        "delete MyLite TIMESTAMP event row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT event_id, event_name, created_at, updated_at, "
                   "IFNULL(seen_at, 'SQLNULL') "
                   "FROM wp_events_timestamp_like ORDER BY event_id",
            .values = after_delete,
            .column_count = after_delete_column_count,
            .row_count = after_delete_row_count,
            .context = "MyLite TIMESTAMP CRUD rows match MySQL fixture",
        }
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT COUNT(*), MIN(created_at), MAX(updated_at), "
                   "GROUP_CONCAT(event_name ORDER BY event_id SEPARATOR ',') "
                   "FROM wp_events_timestamp_like",
            .values = summary,
            .column_count = summary_column_count,
            .row_count = single_row_count,
            .context = "MyLite TIMESTAMP summary matches MySQL fixture",
        }
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO wp_events_timestamp_like "
        "(event_name, created_at, updated_at, seen_at) VALUES "
        "('too-low', '1970-01-01 00:00:00', '1970-01-01 00:00:01', NULL)",
        MYLITE_SQLITE_ERROR,
        "MyLite TIMESTAMP coercion rejects lower bound"
    );
    failures += expect_mylite_error_condition(
        database,
        invalid_timestamp_error,
        "MyLite lower-bound TIMESTAMP condition"
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO wp_events_timestamp_like "
        "(event_name, created_at, updated_at, seen_at) VALUES "
        "('too-high', '2038-01-19 03:14:08', '2038-01-19 03:14:07', NULL)",
        MYLITE_SQLITE_ERROR,
        "MyLite TIMESTAMP coercion rejects upper bound"
    );
    failures += expect_mylite_error_condition(
        database,
        invalid_timestamp_error,
        "MyLite upper-bound TIMESTAMP condition"
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO wp_events_timestamp_like "
        "(event_name, created_at, updated_at, seen_at) VALUES "
        "('round-high', '2038-01-19 03:14:07.5', '2038-01-19 03:14:07', NULL)",
        MYLITE_SQLITE_ERROR,
        "MyLite TIMESTAMP coercion rejects rounded upper overflow"
    );
    failures += expect_mylite_error_condition(
        database,
        invalid_timestamp_error,
        "MyLite rounded TIMESTAMP overflow condition"
    );
    failures += exec_mylite_sql(
        database,
        "TRUNCATE TABLE wp_events_timestamp_like",
        "truncate MyLite TIMESTAMP events table"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT COUNT(*), COALESCE(MAX(event_id), 0) FROM wp_events_timestamp_like",
            .values = after_truncate,
            .column_count = after_truncate_column_count,
            .row_count = single_row_count,
            .context = "MyLite TIMESTAMP truncate matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO wp_events_timestamp_like "
        "(event_name, created_at, updated_at, seen_at) VALUES "
        "('restored', '2038-01-19 03:14:07', '2038-01-19 03:14:07.9994', "
        "'2038-01-19 03:14:07.999999')",
        "reinsert MyLite TIMESTAMP event row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT event_id, event_name, created_at, updated_at, seen_at "
                   "FROM wp_events_timestamp_like",
            .values = after_reinsert,
            .column_count = after_reinsert_column_count,
            .row_count = single_row_count,
            .context = "MyLite TIMESTAMP reinsert after truncate matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "DROP TABLE wp_events_timestamp_like",
        "drop MyLite TIMESTAMP events table"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT COUNT(*) FROM information_schema.tables "
                   "WHERE table_schema = DATABASE() "
                   "AND table_name = 'wp_events_timestamp_like'",
            .values = remaining_tables,
            .column_count = remaining_table_column_count,
            .row_count = single_row_count,
            .context = "MyLite TIMESTAMP drop table matches MySQL fixture",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_mylite_time_type_coercion(void) {
    enum {
        time_column_count = 4,
        time_initial_row_count = 6,
        time_after_replace_row_count = 7,
        invalid_time_error = 1292,
    };

    static const char *const after_insert[] = {
        "1", "12:34:56",  "12:34:56.790",  "12:34:56.123457",
        "2", "-12:34:56", "-12:34:56.790", "-12:34:56.123457",
        "3", "26:03:04",  "26:03:04.568",  "26:03:04.123456",
        "4", "12:34:56",  "12:34:56.789",  "12:34:56.123456",
        "5", "00:12:00",  "01:02:03.457",  "00:00:00.000000",
        "6", "838:59:59", "838:59:59.000", "838:59:59.000000",
    };
    static const char *const after_update[] = {
        "1", "24:00:00",  "-23:59:59.999", "838:59:58.999999",
        "2", "-12:34:56", "-12:34:56.790", "-12:34:56.123457",
        "3", "26:03:04",  "26:03:04.568",  "26:03:04.123456",
        "4", "12:34:56",  "12:34:56.789",  "12:34:56.123456",
        "5", "00:12:00",  "01:02:03.457",  "00:00:00.000000",
        "6", "838:59:59", "838:59:59.000", "838:59:59.000000",
    };
    static const char *const after_duplicate_update[] = {
        "1", "24:00:00",  "-23:59:59.999", "838:59:58.999999",
        "2", "00:00:01",  "12:34:00.988",  "-838:59:59.000000",
        "3", "26:03:04",  "26:03:04.568",  "26:03:04.123456",
        "4", "12:34:56",  "12:34:56.789",  "12:34:56.123456",
        "5", "00:12:00",  "01:02:03.457",  "00:00:00.000000",
        "6", "838:59:59", "838:59:59.000", "838:59:59.000000",
    };
    static const char *const after_replace[] = {
        "1", "24:00:00",   "-23:59:59.999", "838:59:58.999999",
        "2", "00:00:01",   "12:34:00.988",  "-838:59:59.000000",
        "3", "26:03:04",   "26:03:04.568",  "26:03:04.123456",
        "4", "12:34:56",   "12:34:56.789",  "12:34:56.123456",
        "5", "00:12:00",   "01:02:03.457",  "00:00:00.000000",
        "6", "838:59:59",  "838:59:59.000", "838:59:59.000000",
        "7", "-838:59:59", "00:00:00.000",  "12:34:57.000000",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_mylite_ok(mylite_open_memory(&database), database, "open MyLite time coercion");
    if (failures != 0) {
        return failures;
    }

    failures += exec_mylite_sql(
        database,
        "CREATE DATABASE mylite_time_coercion CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci",
        "create MyLite time coercion schema"
    );
    failures += exec_mylite_sql(database, "USE mylite_time_coercion", "use time coercion schema");
    failures += exec_mylite_sql(
        database,
        "CREATE TABLE time_basic ("
        "id INT PRIMARY KEY,"
        "t TIME NOT NULL,"
        "t3 TIME(3) NOT NULL,"
        "t6 TIME(6) NOT NULL"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
        "create MyLite time coercion table"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO time_basic VALUES "
        "(1, '12:34:56', '12:34:56.7896', '12:34:56.1234567'),"
        "(2, '-12:34:56', '-12:34:56.7896', '-12:34:56.1234567'),"
        "(3, '1 02:03:04', '1 02:03:04.5678', '1 02:03:04.123456'),"
        "(4, 123456, 123456.789, 123456.123456),"
        "(5, ':12', '1:2:3.4567', '-00:00:00.0000004'),"
        "(6, '838:59:58.5', '838:59:58.9995', '838:59:58.9999995')",
        "insert MyLite time coercion rows"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, t, t3, t6 FROM time_basic ORDER BY id",
            .values = after_insert,
            .column_count = time_column_count,
            .row_count = time_initial_row_count,
            .context = "MyLite time insert coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "UPDATE time_basic SET t = '23:59:59.9', "
        "t3 = '-23:59:59.9994', t6 = '838:59:58.999999' WHERE id = 1",
        "update MyLite time coercion row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, t, t3, t6 FROM time_basic ORDER BY id",
            .values = after_update,
            .column_count = time_column_count,
            .row_count = time_initial_row_count,
            .context = "MyLite time update coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO time_basic VALUES "
        "(2, '00:00:00.5', '12:34.9876', '-838:59:58.9999995') "
        "ON DUPLICATE KEY UPDATE t = VALUES(t), t3 = VALUES(t3), t6 = VALUES(t6)",
        "insert duplicate update MyLite time coercion row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, t, t3, t6 FROM time_basic ORDER BY id",
            .values = after_duplicate_update,
            .column_count = time_column_count,
            .row_count = time_initial_row_count,
            .context = "MyLite time duplicate update coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "REPLACE INTO time_basic VALUES "
        "(7, '-838:59:58.5', '-00:00:00.0004', '123456.9999995')",
        "replace MyLite time coercion row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, t, t3, t6 FROM time_basic ORDER BY id",
            .values = after_replace,
            .column_count = time_column_count,
            .row_count = time_after_replace_row_count,
            .context = "MyLite time replace coercion matches MySQL fixture",
        }
    );

    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO time_basic VALUES (8, '838:59:59.000001', '00:00:00', '00:00:00')",
        MYLITE_SQLITE_ERROR,
        "MyLite time coercion rejects out-of-range values"
    );
    failures += expect_mylite_error_condition(
        database,
        invalid_time_error,
        "MyLite out-of-range time condition"
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO time_basic VALUES (8, '12:60:00', '00:00:00', '00:00:00')",
        MYLITE_SQLITE_ERROR,
        "MyLite time coercion rejects malformed values"
    );
    failures += expect_mylite_error_condition(
        database,
        invalid_time_error,
        "MyLite malformed time condition"
    );

    mylite_close(database);
    return failures;
}

static int test_mylite_year_type_coercion(void) {
    enum {
        year_column_count = 3,
        year_initial_row_count = 10,
        year_after_replace_row_count = 11,
        year_out_of_range_error = 1264,
        invalid_year_error = 1366,
    };

    static const char *const after_insert[] = {
        "1",    "0000", "0",    "2",    "2000", "2000", "3",    "2000", "2000", "4",
        "0000", "0",    "5",    "2001", "2001", "6",    "2069", "2069", "7",    "1970",
        "1970", "8",    "1901", "1901", "9",    "1902", "1902", "10",   "2155", "2155",
    };
    static const char *const after_update[] = {
        "1",    "2002", "2002", "2",    "1998", "1998", "3",    "2000", "2000", "4",
        "0000", "0",    "5",    "2001", "2001", "6",    "2069", "2069", "7",    "1970",
        "1970", "8",    "1901", "1901", "9",    "1902", "1902", "10",   "2155", "2155",
    };
    static const char *const after_duplicate_update[] = {
        "1",    "2002", "2002", "2",    "2012", "2012", "3",    "2000", "2000", "4",
        "0000", "0",    "5",    "2001", "2001", "6",    "2069", "2069", "7",    "1970",
        "1970", "8",    "1901", "1901", "9",    "1902", "1902", "10",   "2155", "2155",
    };
    static const char *const after_replace[] = {
        "1",    "2002", "2002", "2",    "2012", "2012", "3",    "2000", "2000", "4",    "0000",
        "0",    "5",    "2001", "2001", "6",    "2069", "2069", "7",    "1970", "1970", "8",
        "1901", "1901", "9",    "1902", "1902", "10",   "2155", "2155", "11",   "2068", "2068",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_mylite_ok(mylite_open_memory(&database), database, "open MyLite year coercion");
    if (failures != 0) {
        return failures;
    }

    failures += exec_mylite_sql(
        database,
        "CREATE DATABASE mylite_year_coercion CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci",
        "create MyLite year coercion schema"
    );
    failures += exec_mylite_sql(database, "USE mylite_year_coercion", "use year coercion schema");
    failures += exec_mylite_sql(
        database,
        "CREATE TABLE year_basic (id INT PRIMARY KEY, y YEAR NOT NULL) "
        "ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
        "create MyLite year coercion table"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO year_basic VALUES "
        "(1, 0), (2, '0'), (3, '00'), (4, '0000'), (5, 1), "
        "(6, '69'), (7, 70), (8, 1901.4), (9, 1901.5), (10, 2155)",
        "insert MyLite year coercion rows"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, y, y + 0 FROM year_basic ORDER BY id",
            .values = after_insert,
            .column_count = year_column_count,
            .row_count = year_initial_row_count,
            .context = "MyLite year insert coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "UPDATE year_basic SET y = '2' WHERE id = 1",
        "update quoted MyLite year row"
    );
    failures += exec_mylite_sql(
        database,
        "UPDATE year_basic SET y = 98 WHERE id = 2",
        "update numeric MyLite year row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, y, y + 0 FROM year_basic ORDER BY id",
            .values = after_update,
            .column_count = year_column_count,
            .row_count = year_initial_row_count,
            .context = "MyLite year update coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO year_basic VALUES (2, '2012') ON DUPLICATE KEY UPDATE y = VALUES(y)",
        "insert duplicate update MyLite year row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, y, y + 0 FROM year_basic ORDER BY id",
            .values = after_duplicate_update,
            .column_count = year_column_count,
            .row_count = year_initial_row_count,
            .context = "MyLite year duplicate update coercion matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "REPLACE INTO year_basic VALUES (11, '68')",
        "replace MyLite year row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT id, y, y + 0 FROM year_basic ORDER BY id",
            .values = after_replace,
            .column_count = year_column_count,
            .row_count = year_after_replace_row_count,
            .context = "MyLite year replace coercion matches MySQL fixture",
        }
    );

    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO year_basic VALUES (12, 100)",
        MYLITE_SQLITE_ERROR,
        "MyLite year coercion rejects 100"
    );
    failures += expect_mylite_error_condition(
        database,
        year_out_of_range_error,
        "MyLite out-of-range YEAR 100 condition"
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO year_basic VALUES (12, 1900)",
        MYLITE_SQLITE_ERROR,
        "MyLite year coercion rejects 1900"
    );
    failures += expect_mylite_error_condition(
        database,
        year_out_of_range_error,
        "MyLite out-of-range YEAR 1900 condition"
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO year_basic VALUES (12, 'bad')",
        MYLITE_SQLITE_ERROR,
        "MyLite year coercion rejects invalid text"
    );
    failures += expect_mylite_error_condition(
        database,
        invalid_year_error,
        "MyLite invalid YEAR text condition"
    );

    mylite_close(database);
    return failures;
}

static int test_mylite_bit_type_coercion(void) {
    enum {
        bit_column_count = 12,
        bit_after_delete_row_count = 2,
        bit_summary_column_count = 3,
        bit_truncate_column_count = 2,
        bit_reinsert_column_count = 9,
        bit_remaining_table_column_count = 1,
        single_row_count = 1,
        bit_too_long_error = 1406,
        bit_out_of_range_error = 1264,
    };

    static const char *const after_delete[] = {
        "1", "1", "search", "1", "1", "8", "5",  "1", "8", "341", "2", "16",
        "3", "2", "cache",  "1", "1", "8", "60", "1", "8", "1",   "2", "16",
    };
    static const char *const summary[] = {"2", "5", "60"};
    static const char *const after_truncate[] = {"0", "0"};
    static const char *const after_reinsert[] =
        {"1", "3", "restored", "1", "127", "2", "1", "1", "2"};
    static const char *const remaining_tables[] = {"0"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_mylite_ok(mylite_open_memory(&database), database, "open MyLite bit CRUD");
    if (failures != 0) {
        return failures;
    }

    failures += exec_mylite_sql(
        database,
        "CREATE DATABASE mylite_bit_column_crud CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci",
        "create MyLite bit CRUD schema"
    );
    failures += exec_mylite_sql(database, "USE mylite_bit_column_crud", "use bit CRUD schema");
    failures += exec_mylite_sql(
        database,
        "CREATE TABLE wp_feature_flags_like ("
        "flag_id INT NOT NULL AUTO_INCREMENT,"
        "blog_id BIGINT UNSIGNED NOT NULL,"
        "flag_key VARCHAR(64) NOT NULL,"
        "enabled BIT NOT NULL,"
        "mask BIT(8) NOT NULL,"
        "rollout BIT(9) DEFAULT NULL,"
        "PRIMARY KEY (flag_id),"
        "KEY blog_flag (blog_id, flag_key)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
        "create MyLite bit feature flags table"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO wp_feature_flags_like (blog_id, flag_key, enabled, mask, rollout) VALUES "
        "(1, 'search', b'1', b'00000101', b'101010101'),"
        "(1, 'editor', 0, 15, 257),"
        "(2, 'cache', b'', '', NULL)",
        "insert MyLite bit feature flag rows"
    );
    failures += exec_mylite_sql(
        database,
        "UPDATE wp_feature_flags_like "
        "SET enabled = b'1', mask = b'00111100', rollout = b'000000001' "
        "WHERE flag_key = 'cache'",
        "update MyLite bit feature flag row"
    );
    failures += exec_mylite_sql(
        database,
        "DELETE FROM wp_feature_flags_like WHERE flag_key = 'editor'",
        "delete MyLite bit feature flag row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT flag_id, blog_id, flag_key, "
                   "enabled + 0, LENGTH(enabled), BIT_LENGTH(enabled), "
                   "mask + 0, LENGTH(mask), BIT_LENGTH(mask), "
                   "rollout + 0, LENGTH(rollout), BIT_LENGTH(rollout) "
                   "FROM wp_feature_flags_like ORDER BY mask, flag_id",
            .values = after_delete,
            .column_count = bit_column_count,
            .row_count = bit_after_delete_row_count,
            .context = "MyLite bit CRUD rows match MySQL fixture",
        }
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT COUNT(*), MIN(mask + 0), MAX(mask + 0) "
                   "FROM wp_feature_flags_like",
            .values = summary,
            .column_count = bit_summary_column_count,
            .row_count = single_row_count,
            .context = "MyLite bit aggregate summary matches MySQL fixture",
        }
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO wp_feature_flags_like (blog_id, flag_key, enabled, mask) "
        "VALUES (9, 'too-long', b'1', 256)",
        MYLITE_SQLITE_ERROR,
        "MyLite bit coercion rejects oversized values"
    );
    failures += expect_mylite_error_condition(
        database,
        bit_too_long_error,
        "MyLite oversized bit condition"
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO wp_feature_flags_like (blog_id, flag_key, enabled, mask) "
        "VALUES (9, 'negative', b'1', -1)",
        MYLITE_SQLITE_ERROR,
        "MyLite bit coercion rejects negative values"
    );
    failures += expect_mylite_error_condition(
        database,
        bit_out_of_range_error,
        "MyLite negative bit condition"
    );
    failures += exec_mylite_sql(
        database,
        "TRUNCATE TABLE wp_feature_flags_like",
        "truncate MyLite bit feature flags"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT COUNT(*), COALESCE(MAX(flag_id), 0) FROM wp_feature_flags_like",
            .values = after_truncate,
            .column_count = bit_truncate_column_count,
            .row_count = single_row_count,
            .context = "MyLite bit truncate matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO wp_feature_flags_like (blog_id, flag_key, enabled, mask, rollout) "
        "VALUES (3, 'restored', b'1', X'7f', b'000000010')",
        "reinsert MyLite bit feature flag row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT flag_id, blog_id, flag_key, enabled + 0, mask + 0, rollout + 0, "
                   "LENGTH(enabled), LENGTH(mask), LENGTH(rollout) "
                   "FROM wp_feature_flags_like",
            .values = after_reinsert,
            .column_count = bit_reinsert_column_count,
            .row_count = single_row_count,
            .context = "MyLite bit reinsert after truncate matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "DROP TABLE wp_feature_flags_like",
        "drop MyLite bit feature flags table"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT COUNT(*) FROM information_schema.tables "
                   "WHERE table_schema = DATABASE() "
                   "AND table_name = 'wp_feature_flags_like'",
            .values = remaining_tables,
            .column_count = bit_remaining_table_column_count,
            .row_count = single_row_count,
            .context = "MyLite bit drop table matches MySQL fixture",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_mylite_select_descriptor_hydration(void) {
    enum {
        reopened_column_count = 5,
        single_row_count = 1,
    };

    static const char *const reopened_rows[] = {"1", "search", "341", "2", "16"};
    const char *path = MYLITE_SQLITE_FORK_TEST_FILE_PATH;
    mylite_db *database = NULL;
    int failures = 0;

    remove_sqlite_fork_test_files();

    failures += expect_mylite_ok(
        mylite_open(path, &database),
        database,
        "open MyLite descriptor hydration database"
    );
    if (failures != 0) {
        remove_sqlite_fork_test_files();
        return failures;
    }

    failures += exec_mylite_sql(
        database,
        "CREATE DATABASE mylite_descriptor_hydration CHARACTER SET utf8mb4 "
        "COLLATE utf8mb4_unicode_ci",
        "create MyLite descriptor hydration schema"
    );
    failures += exec_mylite_sql(
        database,
        "USE mylite_descriptor_hydration",
        "use descriptor hydration schema"
    );
    failures += exec_mylite_sql(
        database,
        "CREATE TABLE wp_flags_reopen_like ("
        "flag_id INT NOT NULL AUTO_INCREMENT,"
        "flag_key VARCHAR(64) NOT NULL,"
        "rollout BIT(9) NOT NULL,"
        "PRIMARY KEY(flag_id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
        "create MyLite descriptor hydration table"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO wp_flags_reopen_like(flag_key, rollout) VALUES "
        "('search', b'101010101')",
        "insert MyLite descriptor hydration row"
    );

    mylite_close(database);
    database = NULL;
    if (failures != 0) {
        remove_sqlite_fork_test_files();
        return failures;
    }

    failures += expect_mylite_ok(
        mylite_open(path, &database),
        database,
        "reopen MyLite descriptor hydration database"
    );
    if (failures == 0) {
        failures += exec_mylite_sql(
            database,
            "USE mylite_descriptor_hydration",
            "reuse descriptor hydration schema"
        );
    }
    if (failures == 0) {
        failures += expect_mylite_rows(
            database,
            (struct expected_mylite_rows){
                .sql = "SELECT flag_id, flag_key, rollout + 0, LENGTH(rollout), "
                       "BIT_LENGTH(rollout) FROM wp_flags_reopen_like",
                .values = reopened_rows,
                .column_count = reopened_column_count,
                .row_count = single_row_count,
                .context = "MyLite SELECT reload hydrates BIT read descriptors after reopen",
            }
        );
    }

    mylite_close(database);
    remove_sqlite_fork_test_files();
    return failures;
}

static int test_mylite_json_type_coercion(void) {
    enum {
        json_after_delete_column_count = 8,
        json_after_delete_row_count = 2,
        json_summary_column_count = 3,
        json_truncate_column_count = 2,
        json_reinsert_column_count = 5,
        json_remaining_table_column_count = 1,
        single_row_count = 1,
        json_invalid_error = 3140,
    };

    static const char *const after_delete[] = {
        "1",
        "site_meta",
        "OBJECT",
        NULL,
        NULL,
        "3",
        "1",
        "yes",
        "2",
        "theme_mods",
        "OBJECT",
        "green",
        "3",
        "3",
        "1",
        "no",
    };
    static const char *const summary[] = {"2", "2", "site_meta,theme_mods"};
    static const char *const after_truncate[] = {"0", "0"};
    static const char *const after_reinsert[] = {"1", "restored", "ARRAY", "2", "2"};
    static const char *const remaining_tables[] = {"0"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_mylite_ok(mylite_open_memory(&database), database, "open MyLite JSON CRUD");
    if (failures != 0) {
        return failures;
    }

    failures += exec_mylite_sql(
        database,
        "CREATE DATABASE mylite_json_column_crud CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci",
        "create MyLite JSON CRUD schema"
    );
    failures += exec_mylite_sql(database, "USE mylite_json_column_crud", "use JSON CRUD schema");
    failures += exec_mylite_sql(
        database,
        "CREATE TABLE wp_options_json_like ("
        "option_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        "option_name VARCHAR(64) NOT NULL,"
        "option_value JSON NOT NULL,"
        "autoload VARCHAR(20) NOT NULL DEFAULT 'yes',"
        "PRIMARY KEY (option_id),"
        "UNIQUE KEY option_name (option_name)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci",
        "create MyLite JSON options table"
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO wp_options_json_like (option_name, option_value, autoload) VALUES "
        "('site_meta', '{\"blog_id\":1,\"active\":true,\"tags\":[\"cms\",\"blog\"]}', 'yes'),"
        "('theme_mods', '{\"color\":\"blue\",\"layout\":{\"columns\":2}}', 'no'),"
        "('empty_flags', '[]', 'yes')",
        "insert MyLite JSON option rows"
    );
    failures += exec_mylite_sql(
        database,
        "UPDATE wp_options_json_like "
        "SET option_value = '{\"color\":\"green\",\"layout\":{\"columns\":3},\"enabled\":true}' "
        "WHERE option_name = 'theme_mods'",
        "update MyLite JSON option row"
    );
    failures += exec_mylite_sql(
        database,
        "DELETE FROM wp_options_json_like WHERE option_name = 'empty_flags'",
        "delete MyLite JSON option row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT option_id, option_name, JSON_TYPE(option_value), "
                   "JSON_UNQUOTE(JSON_EXTRACT(option_value, '$.color')), "
                   "JSON_EXTRACT(option_value, '$.layout.columns'), "
                   "JSON_LENGTH(option_value), JSON_VALID(option_value), autoload "
                   "FROM wp_options_json_like ORDER BY option_id",
            .values = after_delete,
            .column_count = json_after_delete_column_count,
            .row_count = json_after_delete_row_count,
            .context = "MyLite JSON CRUD rows match MySQL fixture",
        }
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT COUNT(*), SUM(JSON_VALID(option_value)), "
                   "GROUP_CONCAT(option_name ORDER BY option_id SEPARATOR ',') "
                   "FROM wp_options_json_like",
            .values = summary,
            .column_count = json_summary_column_count,
            .row_count = single_row_count,
            .context = "MyLite JSON aggregate summary matches MySQL fixture",
        }
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO wp_options_json_like(option_name, option_value) VALUES ('bad_text', 'not "
        "json')",
        MYLITE_SQLITE_ERROR,
        "MyLite JSON coercion rejects invalid text"
    );
    failures += expect_mylite_error_condition(
        database,
        json_invalid_error,
        "MyLite invalid JSON text condition"
    );
    failures += expect_mylite_sql_status(
        database,
        "INSERT INTO wp_options_json_like(option_name, option_value) VALUES ('bad_int', 1)",
        MYLITE_SQLITE_ERROR,
        "MyLite JSON coercion rejects non-text assignment"
    );
    failures += expect_mylite_error_condition(
        database,
        json_invalid_error,
        "MyLite non-text JSON condition"
    );
    failures += exec_mylite_sql(
        database,
        "TRUNCATE TABLE wp_options_json_like",
        "truncate MyLite JSON options table"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT COUNT(*), COALESCE(MAX(option_id), 0) FROM wp_options_json_like",
            .values = after_truncate,
            .column_count = json_truncate_column_count,
            .row_count = single_row_count,
            .context = "MyLite JSON truncate matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "INSERT INTO wp_options_json_like(option_name, option_value) "
        "VALUES ('restored', '[{\"id\":1},{\"id\":2}]')",
        "reinsert MyLite JSON option row"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT option_id, option_name, JSON_TYPE(option_value), "
                   "JSON_LENGTH(option_value), JSON_EXTRACT(option_value, '$[1].id') "
                   "FROM wp_options_json_like",
            .values = after_reinsert,
            .column_count = json_reinsert_column_count,
            .row_count = single_row_count,
            .context = "MyLite JSON reinsert after truncate matches MySQL fixture",
        }
    );
    failures += exec_mylite_sql(
        database,
        "DROP TABLE wp_options_json_like",
        "drop MyLite JSON options table"
    );
    failures += expect_mylite_rows(
        database,
        (struct expected_mylite_rows){
            .sql = "SELECT COUNT(*) FROM information_schema.tables "
                   "WHERE table_schema = DATABASE() "
                   "AND table_name = 'wp_options_json_like'",
            .values = remaining_tables,
            .column_count = json_remaining_table_column_count,
            .row_count = single_row_count,
            .context = "MyLite JSON drop table matches MySQL fixture",
        }
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

static void remove_sqlite_fork_test_files(void) {
    (void)remove(MYLITE_SQLITE_FORK_TEST_FILE_PATH);
    (void)remove(MYLITE_SQLITE_FORK_TEST_FILE_PATH "-journal");
    (void)remove(MYLITE_SQLITE_FORK_TEST_FILE_PATH "-wal");
    (void)remove(MYLITE_SQLITE_FORK_TEST_FILE_PATH "-shm");
}
