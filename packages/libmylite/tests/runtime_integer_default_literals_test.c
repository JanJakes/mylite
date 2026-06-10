#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "sqlite3.h"
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
    mysql_error_bad_null = 1048,
    mysql_error_invalid_default = 1067,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_incorrect_column_name = 1166,
    mysql_error_field_no_default = 1364,
    default_projection_column_count = 14,
    expression_default_projection_column_count = 7,
    alter_add_projection_column_count = 5,
    alter_set_default_projection_column_count = 7,
    alter_set_default_boundary_column_count = 10,
    quoted_default_projection_column_count = 4,
    alter_removed_default_row_count = 5,
    drop_default_description_row_count = 8,
    show_columns_column_count = 6,
};

struct expected_statement {
    int64_t affected_rows;
    size_t warning_count;
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

struct expected_contains_query {
    const char *sql;
    const char *needle;
    const char *context;
};

static int test_create_insert_metadata_and_persistence(void);
static int test_create_column_attribute_order(void);
static int test_integer_expression_defaults(void);
static int test_alter_defaults(void);
static int test_alter_column_set_default(void);
static int test_alter_column_drop_default(void);
static int test_default_diagnostics_and_if_not_exists(void);
static int test_catalog_v1_migration(void);
static int test_catalog_v2_migration(void);
static int test_independent_default_handles(void);
static int seed_schema(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_single_value_contains(mylite_db *database, struct expected_contains_query query);
static int expect_single_value_not_contains(
    mylite_db *database,
    struct expected_contains_query query
);
static int make_catalog_look_like_v1(sqlite3 *sqlite);
static int make_catalog_look_like_v2(sqlite3 *sqlite);
static int execute_sql(sqlite3 *connection, const char *sql);
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
static int expect_not_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_create_insert_metadata_and_persistence();
    failures += test_create_column_attribute_order();
    failures += test_integer_expression_defaults();
    failures += test_alter_defaults();
    failures += test_alter_column_set_default();
    failures += test_alter_column_drop_default();
    failures += test_default_diagnostics_and_if_not_exists();
    failures += test_catalog_v1_migration();
    failures += test_catalog_v2_migration();
    failures += test_independent_default_handles();

    return failures == 0 ? 0 : 1;
}

static int test_create_insert_metadata_and_persistence(void) {
    static const char *const default_row[] = {
        "5",
        "9",
        "-7",
        "255",
        "-32768",
        "65535",
        "-8388608",
        "16777215",
        "-9223372036854775808",
        "9223372036854775807",
        "1",
        "0",
        "11",
        NULL,
    };
    static const char *const explicit_null[] = {NULL};
    static const char *const show_i[] = {"i", "int", "YES", "", "5", ""};
    static const char *const show_nn[] = {"nn", "int", "NO", "", "11", ""};
    static const char *const show_nul[] = {"nul", "int", "YES", "", NULL, ""};
    static const char *const quoted_default_row[] = {
        "0",
        "7",
        "-3",
        "9223372036854775807",
    };
    static const char *const show_quoted_p[] = {"p", "int", "YES", "", "7", ""};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "create_insert") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open create defaults");
    failures += seed_schema(database);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE defaults ("
        "id INT NOT NULL, "
        "i INT DEFAULT 5, "
        "ip INTEGER DEFAULT +9, "
        "n INT DEFAULT -7, "
        "tiu TINYINT UNSIGNED DEFAULT 255, "
        "si SMALLINT DEFAULT -32768, "
        "siu SMALLINT UNSIGNED DEFAULT 65535, "
        "mi MEDIUMINT DEFAULT -8388608, "
        "miu MEDIUMINT UNSIGNED DEFAULT 16777215, "
        "bi BIGINT DEFAULT -9223372036854775808, "
        "bu BIGINT UNSIGNED DEFAULT 9223372036854775807, "
        "b BOOL DEFAULT TRUE, "
        "f BOOLEAN DEFAULT FALSE, "
        "nn INT NOT NULL DEFAULT 11, "
        "nul INT DEFAULT NULL)"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO defaults (id) VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO defaults SET id = 2",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO defaults (id, i) VALUES (3, NULL)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, ip, n, tiu, si, siu, mi, miu, bi, bu, b, f, nn, nul "
                   "FROM defaults WHERE id = 1",
            .values = default_row,
            .column_count = default_projection_column_count,
            .row_count = 1U,
            .context = "created defaults fill omitted values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i FROM defaults WHERE id = 3",
            .values = explicit_null,
            .column_count = 1U,
            .row_count = 1U,
            .context = "explicit NULL overrides nullable default",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM defaults LIKE 'i'",
            .values = show_i,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS integer default",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM defaults LIKE 'nn'",
            .values = show_nn,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS not-null integer default",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM defaults LIKE 'nul'",
            .values = show_nul,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS default null",
        }
    );
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE defaults",
            .needle = "`i` int DEFAULT '5'",
            .context = "SHOW CREATE TABLE nullable integer default",
        }
    );
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE defaults",
            .needle = "`nn` int NOT NULL DEFAULT '11'",
            .context = "SHOW CREATE TABLE not-null integer default",
        }
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE quoted_defaults ("
        "i INT DEFAULT '0', "
        "p INT DEFAULT '+7', "
        "n INT DEFAULT '-3', "
        "bu BIGINT UNSIGNED DEFAULT '9223372036854775807')"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO quoted_defaults () VALUES ()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, p, n, bu FROM quoted_defaults",
            .values = quoted_default_row,
            .column_count = quoted_default_projection_column_count,
            .row_count = 1U,
            .context = "quoted integer defaults fill omitted values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM quoted_defaults LIKE 'p'",
            .values = show_quoted_p,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS quoted signed integer default",
        }
    );
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE quoted_defaults",
            .needle = "`p` int DEFAULT '7'",
            .context = "SHOW CREATE TABLE quoted positive integer default",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "integer defaults preserve MyLite preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen create defaults");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, ip, n, tiu, si, siu, mi, miu, bi, bu, b, f, nn, nul "
                   "FROM defaults WHERE id = 1",
            .values = default_row,
            .column_count = default_projection_column_count,
            .row_count = 1U,
            .context = "reopened defaults persist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_create_column_attribute_order(void) {
    static const char *const attr_order_columns[] = {
        "id",
        "int",
        "NO",
        "",
        "7",
        "",
        "pk_before",
        "int",
        "NO",
        "PRI",
        "3",
        "",
        "uq_after",
        "int",
        "YES",
        "UNI",
        "6",
        "",
    };
    static const char *const attr_order_row[] = {"7", "10", "6"};
    static const char *const primary_after_columns[] = {"id", "int", "NO", "PRI", "4", ""};
    static const char *const primary_after_row[] = {"4"};
    static const char *const unique_before_columns[] = {"id", "int", "YES", "UNI", "5", ""};
    static const char *const unique_before_row[] = {"5"};
    static const char *const auto_increment_columns[] = {
        "id",
        "int",
        "NO",
        "PRI",
        NULL,
        "auto_increment",
        "name",
        "varchar(32)",
        "NO",
        "",
        "",
        "",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "column_attribute_order") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open attribute order");
    failures += seed_schema(database);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE attr_order ("
        "id INT DEFAULT 7 NOT NULL, "
        "pk_before INT PRIMARY KEY DEFAULT 3, "
        "uq_after INT DEFAULT 6 UNIQUE KEY)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM attr_order",
            .values = attr_order_columns,
            .column_count = show_columns_column_count,
            .row_count = 3U,
            .context = "SHOW COLUMNS legacy attribute order",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO attr_order (pk_before) VALUES (10)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, pk_before, uq_after FROM attr_order",
            .values = attr_order_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "legacy attribute order defaults materialize",
        }
    );
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE attr_order",
            .needle = "`id` int NOT NULL DEFAULT '7'",
            .context = "SHOW CREATE TABLE default before nullability normalized",
        }
    );

    failures +=
        execute_statement_ok(database, "CREATE TABLE primary_after (id INT DEFAULT 4 PRIMARY KEY)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM primary_after",
            .values = primary_after_columns,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS default before primary key",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO primary_after () VALUES ()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM primary_after",
            .values = primary_after_row,
            .column_count = 1U,
            .row_count = 1U,
            .context = "default before primary key materializes",
        }
    );

    failures +=
        execute_statement_ok(database, "CREATE TABLE unique_before (id INT UNIQUE DEFAULT 5)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM unique_before",
            .values = unique_before_columns,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS unique before default",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO unique_before () VALUES ()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM unique_before",
            .values = unique_before_row,
            .column_count = 1U,
            .row_count = 1U,
            .context = "unique before default materializes",
        }
    );

    failures += execute_statement_ok(
        database,
        "CREATE TABLE auto_increment_legacy ("
        "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
        "name VARCHAR(32) DEFAULT '' NOT NULL)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM auto_increment_legacy",
            .values = auto_increment_columns,
            .column_count = show_columns_column_count,
            .row_count = 2U,
            .context = "SHOW COLUMNS auto increment legacy order",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE invalid_default_null_order (id INT DEFAULT NULL NOT NULL)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'id'",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_integer_expression_defaults(void) {
    static const char *const inserted_row[] = {
        "3",
        "9",
        "4",
        "1",
        NULL,
        "-9223372036854775808",
        "9223372036854775807",
    };
    static const char *const show_i[] = {"i", "int", "YES", "", "(1 + 2)", "DEFAULT_GENERATED"};
    static const char *const show_nul[] = {
        "nul",
        "int",
        "YES",
        "",
        "NULL",
        "DEFAULT_GENERATED",
    };
    static const char *const information_schema_i[] = {"i", "(1 + 2)", "DEFAULT_GENERATED"};
    static const char *const information_schema_nul[] = {"nul", "NULL", "DEFAULT_GENERATED"};
    static const char *const alter_rows[] = {"1", "1", "2", "10", "3", NULL};
    static const char *const ddl_path_added_rows[] = {"1", "1", "5", "2", "1", "5"};
    static const char *const ddl_path_modified_rows[] = {"1", "1", "2", "1", "3", "20"};
    static const char *const ddl_path_changed_rows[] = {
        "1",
        "1",
        "2",
        "1",
        "3",
        "20",
        "4",
        "13",
    };
    static const char *const show_added[] = {
        "added",
        "int",
        "YES",
        "",
        "(2 + 3)",
        "DEFAULT_GENERATED",
    };
    static const char *const show_modified[] = {
        "v",
        "int",
        "YES",
        "",
        "(4 * 5)",
        "DEFAULT_GENERATED",
    };
    static const char *const show_changed[] = {
        "changed",
        "int",
        "YES",
        "",
        "(6 + 7)",
        "DEFAULT_GENERATED",
    };
    static const char *const ctas_show_a[] = {
        "a",
        "int",
        "YES",
        "",
        "(1 + 2)",
        "DEFAULT_GENERATED",
    };
    static const char *const ctas_show_b[] = {"b", "int", "YES", "", NULL, ""};
    static const char *const ctas_rows[] = {"3", "9"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "integer_expression_defaults") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open expression defaults");
    failures += seed_schema(database);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE expr_defaults ("
        "id INT NOT NULL, "
        "i INT DEFAULT (1 + 2), "
        "m INT DEFAULT ((2 * 3) + MOD(7, 4)), "
        "d INT DEFAULT (9 DIV 2), "
        "r INT DEFAULT (9 % 4), "
        "nul INT DEFAULT (NULL), "
        "bi BIGINT DEFAULT (-9223372036854775808), "
        "bu BIGINT UNSIGNED DEFAULT (9223372036854775807))"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO expr_defaults (id) VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO expr_defaults (id, i) VALUES (2, DEFAULT)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, m, d, r, nul, bi, bu FROM expr_defaults WHERE id = 1",
            .values = inserted_row,
            .column_count = expression_default_projection_column_count,
            .row_count = 1U,
            .context = "expression defaults materialize inserted row",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM expr_defaults LIKE 'i'",
            .values = show_i,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS integer expression default",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM expr_defaults LIKE 'nul'",
            .values = show_nul,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS NULL expression default",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_DEFAULT, EXTRA FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'expr_defaults' "
                   "AND COLUMN_NAME = 'i'",
            .values = information_schema_i,
            .column_count = 3U,
            .row_count = 1U,
            .context = "information schema integer expression default metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_DEFAULT, EXTRA FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'expr_defaults' "
                   "AND COLUMN_NAME = 'nul'",
            .values = information_schema_nul,
            .column_count = 3U,
            .row_count = 1U,
            .context = "information schema NULL expression default metadata",
        }
    );
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE expr_defaults",
            .needle = "`i` int DEFAULT ((1 + 2))",
            .context = "SHOW CREATE TABLE integer expression default",
        }
    );
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE expr_defaults",
            .needle = "`nul` int DEFAULT (NULL)",
            .context = "SHOW CREATE TABLE NULL expression default",
        }
    );
    failures += execute_statement_ok(database, "CREATE TABLE expr_like LIKE expr_defaults");
    failures += expect_statement_result(
        database,
        "INSERT INTO expr_like (id) VALUES (10)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM expr_like LIKE 'i'",
            .values = show_i,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE preserves expression default metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, m, d, r, nul, bi, bu FROM expr_like WHERE id = 10",
            .values = inserted_row,
            .column_count = expression_default_projection_column_count,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE materializes expression defaults",
        }
    );

    failures += execute_statement_ok(
        database,
        "CREATE TABLE null_default (id INT NOT NULL, nn INT NOT NULL DEFAULT (NULL))"
    );
    failures += execute_error(
        database,
        "INSERT INTO null_default (id) VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'nn' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_range (a TINYINT DEFAULT (127 + 1))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'a'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_unsigned (a INT UNSIGNED DEFAULT (-1))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'a'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_division (a INT DEFAULT (1 DIV 0))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'a'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_operand (a INT DEFAULT ('1'))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'a'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_column_ref (base INT, a INT DEFAULT (base))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'a'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_decimal_operand (a INT DEFAULT (1.2))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'a'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_float_operand (a INT DEFAULT (1e0))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'a'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_hex_operand (a INT DEFAULT (0x1))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'a'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_bit_operand (a INT DEFAULT (b'1'))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'a'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_function_operand (a INT DEFAULT (ABS(-1)))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'a'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_subquery_operand (a INT DEFAULT ((SELECT 1)))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'a'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_variable_operand (a INT DEFAULT (@@sql_mode))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'a'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_cast_operand (a INT DEFAULT (CAST(1 AS BINARY)))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'a'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_boolean_operand (a INT DEFAULT (1 AND 1))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'a'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_arithmetic_division (a INT DEFAULT (1 / 0))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'a'",
        }
    );

    failures += execute_statement_ok(
        database,
        "CREATE TABLE alter_expr (id INT NOT NULL, a INT DEFAULT 1)"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO alter_expr (id) VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE alter_expr ALTER a SET DEFAULT (2 * 5)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO alter_expr (id) VALUES (2)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE alter_expr ALTER a SET DEFAULT (NULL)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO alter_expr (id) VALUES (3)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a FROM alter_expr ORDER BY id",
            .values = alter_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "ALTER SET expression defaults affect only later inserts",
        }
    );
    failures +=
        execute_statement_ok(database, "CREATE TABLE ddl_paths (id INT NOT NULL, v INT DEFAULT 1)");
    failures += expect_statement_result(
        database,
        "INSERT INTO ddl_paths (id) VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE ddl_paths ADD COLUMN added INT DEFAULT (2 + 3)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO ddl_paths (id) VALUES (2)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, added FROM ddl_paths ORDER BY id",
            .values = ddl_path_added_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "ALTER ADD expression default backfill and future insert",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM ddl_paths LIKE 'added'",
            .values = show_added,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "ALTER ADD expression default metadata",
        }
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE ddl_paths MODIFY v INT DEFAULT (4 * 5)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO ddl_paths (id) VALUES (3)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM ddl_paths ORDER BY id",
            .values = ddl_path_modified_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "ALTER MODIFY expression default affects future rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM ddl_paths LIKE 'v'",
            .values = show_modified,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "ALTER MODIFY expression default metadata",
        }
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE ddl_paths CHANGE v changed INT DEFAULT (6 + 7)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO ddl_paths (id) VALUES (4)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, changed FROM ddl_paths ORDER BY id",
            .values = ddl_path_changed_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "ALTER CHANGE expression default affects future rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM ddl_paths LIKE 'changed'",
            .values = show_changed,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "ALTER CHANGE expression default metadata",
        }
    );

    failures +=
        execute_statement_ok(database, "CREATE TABLE ctas_src (a INT DEFAULT (1 + 2), b INT)");
    failures += expect_statement_result(
        database,
        "INSERT INTO ctas_src (b) VALUES (9)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "CREATE TABLE ctas_copy AS SELECT a, b FROM ctas_src",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM ctas_copy LIKE 'a'",
            .values = ctas_show_a,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "CTAS expression default metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM ctas_copy LIKE 'b'",
            .values = ctas_show_b,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "CTAS non-default metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b FROM ctas_copy",
            .values = ctas_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "CTAS expression default materialized row",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen expression defaults");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, m, d, r, nul, bi, bu FROM expr_defaults WHERE id = 1",
            .values = inserted_row,
            .column_count = expression_default_projection_column_count,
            .row_count = 1U,
            .context = "reopened expression defaults persist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_alter_defaults(void) {
    static const char *const after_add[] = {
        "1",
        "1",
        "12",
        "13",
        NULL,
        "2",
        "1",
        "12",
        "13",
        NULL,
    };
    static const char *const after_modify[] = {"1", "1", "2", "1", "3", "8"};
    static const char *const after_change[] = {"1", "1", "2", "1", "3", "8", "4", "9"};
    static const char *const after_remove_default[] = {
        "1",
        "1",
        "2",
        "1",
        "3",
        "8",
        "4",
        "9",
        "5",
        NULL,
    };
    static const char *const show_renamed[] = {"renamed", "int", "YES", "", NULL, ""};
    static const char *const quoted_alter_rows[] = {
        "1",
        "1",
        "5",
        "2",
        "1",
        "5",
        "3",
        "8",
        "5",
        "4",
        "-9",
        "5",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "alter") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open alter defaults");
    failures += seed_schema(database);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE alter_defaults (id INT NOT NULL, v INT DEFAULT 1)"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO alter_defaults (id) VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE alter_defaults ADD COLUMN added INT DEFAULT 12",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE alter_defaults ADD COLUMN added_nn INT NOT NULL DEFAULT 13",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE alter_defaults ADD COLUMN added_null INT DEFAULT NULL",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO alter_defaults (id) VALUES (2)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, added, added_nn, added_null FROM alter_defaults ORDER BY id",
            .values = after_add,
            .column_count = alter_add_projection_column_count,
            .row_count = 2U,
            .context = "ALTER ADD default backfill and future insert",
        }
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE alter_defaults MODIFY v INT DEFAULT 8",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO alter_defaults (id) VALUES (3)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM alter_defaults ORDER BY id",
            .values = after_modify,
            .column_count = 2U,
            .row_count = 3U,
            .context = "ALTER MODIFY default affects future rows only",
        }
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE alter_defaults CHANGE v renamed INT DEFAULT 9",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO alter_defaults (id) VALUES (4)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, renamed FROM alter_defaults ORDER BY id",
            .values = after_change,
            .column_count = 2U,
            .row_count = 4U,
            .context = "ALTER CHANGE default affects future rows only",
        }
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE alter_defaults MODIFY renamed INT",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO alter_defaults (id) VALUES (5)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, renamed FROM alter_defaults ORDER BY id",
            .values = after_remove_default,
            .column_count = 2U,
            .row_count = alter_removed_default_row_count,
            .context = "ALTER MODIFY without DEFAULT removes previous default",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM alter_defaults LIKE 'renamed'",
            .values = show_renamed,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS removed default",
        }
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE quoted_alter_defaults (id INT NOT NULL, v INT DEFAULT 1)"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO quoted_alter_defaults (id) VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE quoted_alter_defaults ADD COLUMN added INT DEFAULT '5'",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO quoted_alter_defaults (id) VALUES (2)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE quoted_alter_defaults MODIFY v INT DEFAULT '+8'",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO quoted_alter_defaults (id) VALUES (3)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE quoted_alter_defaults CHANGE v changed INT DEFAULT '-9'",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO quoted_alter_defaults (id) VALUES (4)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, changed, added FROM quoted_alter_defaults ORDER BY id",
            .values = quoted_alter_rows,
            .column_count = 3U,
            .row_count = 4U,
            .context = "ALTER ADD MODIFY CHANGE quoted integer defaults",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_alter_column_set_default(void) {
    static const char *const final_rows[] = {
        "1", "1", "3", "2", "4", "5",  "0", "2", "8", "3", "2",
        "4", "5", "0", "3", "8", NULL, "7", "4", "5", "1",
    };
    static const char *const reopened_row[] = {"4", "8", NULL, "7", "4", "5", "1"};
    static const char *const show_v[] = {"v", "int", "YES", "", "8", ""};
    static const char *const show_nullable[] = {"nullable_i", "int", "YES", "", NULL, ""};
    static const char *const show_nn[] = {"nn", "int", "NO", "", "7", ""};
    static const char *const boundary_row[] = {
        "-128",
        "255",
        "-32768",
        "65535",
        "-8388608",
        "16777215",
        "-2147483648",
        "4294967295",
        "-9223372036854775808",
        "9223372036854775807",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "alter_set_default") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open alter set default");
    failures += execute_error(
        database,
        "ALTER TABLE numbers ALTER v SET DEFAULT 1",
        (struct expected_sql_error){
            mysql_error_no_database_selected,
            "3D000",
            "No database selected",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.numbers ALTER v SET DEFAULT 1",
        (struct expected_sql_error){mysql_error_unknown_database, "42000", "Unknown database"}
    );
    failures += seed_schema(database);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE numbers ("
        "id INT NOT NULL, "
        "v INT DEFAULT 1, "
        "nullable_i INT DEFAULT 3, "
        "nn INT NOT NULL DEFAULT 2, "
        "u INT UNSIGNED DEFAULT 4, "
        "bu BIGINT UNSIGNED DEFAULT 5, "
        "b BOOL DEFAULT FALSE)"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO numbers (id) VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE numbers ALTER v SET DEFAULT +8",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM numbers LIKE 'v'",
            .values = show_v,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS after ALTER SET DEFAULT integer",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO numbers (id) VALUES (2)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE numbers ALTER nullable_i SET DEFAULT NULL",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE numbers ALTER COLUMN nn SET DEFAULT 7",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE numbers ALTER COLUMN b SET DEFAULT TRUE",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO numbers (id) VALUES (3)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, nullable_i, nn, u, bu, b FROM numbers ORDER BY id",
            .values = final_rows,
            .column_count = alter_set_default_projection_column_count,
            .row_count = 3U,
            .context = "ALTER SET DEFAULT preserves existing rows and affects future rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM numbers LIKE 'nullable_i'",
            .values = show_nullable,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS after ALTER SET DEFAULT NULL",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM numbers LIKE 'nn'",
            .values = show_nn,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS after ALTER SET DEFAULT not null",
        }
    );
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE numbers",
            .needle = "`nn` int NOT NULL DEFAULT '7'",
            .context = "SHOW CREATE after ALTER SET DEFAULT",
        }
    );

    failures += execute_statement_ok(
        database,
        "CREATE TABLE boundaries ("
        "id INT NOT NULL, ti TINYINT, tiu TINYINT UNSIGNED, si SMALLINT, "
        "siu SMALLINT UNSIGNED, mi MEDIUMINT, miu MEDIUMINT UNSIGNED, "
        "i INT, iu INT UNSIGNED, bi BIGINT, bu BIGINT UNSIGNED)"
    );
    failures += execute_statement_ok(database, "ALTER TABLE boundaries ALTER ti SET DEFAULT -128");
    failures += execute_statement_ok(database, "ALTER TABLE boundaries ALTER tiu SET DEFAULT 255");
    failures +=
        execute_statement_ok(database, "ALTER TABLE boundaries ALTER si SET DEFAULT -32768");
    failures +=
        execute_statement_ok(database, "ALTER TABLE boundaries ALTER siu SET DEFAULT 65535");
    failures +=
        execute_statement_ok(database, "ALTER TABLE boundaries ALTER mi SET DEFAULT -8388608");
    failures +=
        execute_statement_ok(database, "ALTER TABLE boundaries ALTER miu SET DEFAULT 16777215");
    failures +=
        execute_statement_ok(database, "ALTER TABLE boundaries ALTER i SET DEFAULT -2147483648");
    failures +=
        execute_statement_ok(database, "ALTER TABLE boundaries ALTER iu SET DEFAULT 4294967295");
    failures += execute_statement_ok(
        database,
        "ALTER TABLE boundaries ALTER bi SET DEFAULT -9223372036854775808"
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE boundaries ALTER bu SET DEFAULT 9223372036854775807"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO boundaries (id) VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ti, tiu, si, siu, mi, miu, i, iu, bi, bu FROM boundaries",
            .values = boundary_row,
            .column_count = alter_set_default_boundary_column_count,
            .row_count = 1U,
            .context = "ALTER SET DEFAULT integer boundary values",
        }
    );

    failures += execute_error(
        database,
        "ALTER TABLE numbers ALTER missing SET DEFAULT 1",
        (struct expected_sql_error){mysql_error_unknown_column, "42S22", "Unknown column"}
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers ALTER nn SET DEFAULT NULL",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers ALTER u SET DEFAULT -1",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers ALTER v SET DEFAULT 2147483648",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers ALTER bu SET DEFAULT 9223372036854775808",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private ALTER v SET DEFAULT 1",
        (struct expected_sql_error){
            mysql_error_incorrect_table_name,
            "42000",
            "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers ALTER _mylite_private SET DEFAULT 1",
        (struct expected_sql_error){
            mysql_error_incorrect_column_name,
            "42000",
            "Incorrect column name",
        }
    );

    failures += execute_statement_ok(database, "ALTER TABLE numbers RENAME TO renamed_numbers");
    failures += expect_statement_result(
        database,
        "INSERT INTO renamed_numbers (id) VALUES (4)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "ALTER SET DEFAULT preserves MyLite preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen alter set default");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, nullable_i, nn, u, bu, b "
                   "FROM renamed_numbers WHERE id = 4",
            .values = reopened_row,
            .column_count = alter_set_default_projection_column_count,
            .row_count = 1U,
            .context = "reopened ALTER SET DEFAULT values persist after rename",
        }
    );
    failures += execute_statement_ok(database, "DROP TABLE renamed_numbers");
    failures += execute_error(
        database,
        "ALTER TABLE renamed_numbers ALTER v SET DEFAULT 1",
        (struct expected_sql_error){
            mysql_error_table_does_not_exist,
            "42S02",
            "doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_alter_column_drop_default(void) {
    static const char *const qualified_row[] = {"2", "5"};
    static const char *const show_v[] = {"v", "int", "YES", "", NULL, ""};
    static const char *const described_numbers[] = {
        "id",         "int",
        "NO",         "",
        NULL,         "",
        "v",          "int",
        "YES",        "",
        NULL,         "",
        "implicit_i", "int",
        "YES",        "",
        NULL,         "",
        "nullable_i", "int",
        "YES",        "",
        NULL,         "",
        "nn",         "int",
        "NO",         "",
        NULL,         "",
        "u",          "int unsigned",
        "YES",        "",
        NULL,         "",
        "bu",         "bigint unsigned",
        "YES",        "",
        NULL,         "",
        "b",          "tinyint(1)",
        "YES",        "",
        NULL,         "",
    };
    static const char *const original_row[] = {"1", "1", NULL, "2", "4", "5", "0"};
    static const char *const restored_row[] = {"2", "9", NULL, "7", "40", "50", "1"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t sqlite_schema_generation = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "alter_drop_default") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open alter drop default");
    failures += execute_error(
        database,
        "ALTER TABLE numbers ALTER v DROP DEFAULT",
        (struct expected_sql_error){
            mysql_error_no_database_selected,
            "3D000",
            "No database selected",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.numbers ALTER v DROP DEFAULT",
        (struct expected_sql_error){mysql_error_unknown_database, "42000", "Unknown database"}
    );
    failures += expect_statement_result(
        database,
        "CREATE DATABASE app",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE app.qualified_numbers (id INT NOT NULL, n INT DEFAULT 2)"
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE app.qualified_numbers ALTER COLUMN n DROP DEFAULT",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += execute_error(
        database,
        "INSERT INTO app.qualified_numbers (id) VALUES (1)",
        (struct expected_sql_error){
            mysql_error_field_no_default,
            "HY000",
            "doesn't have a default value",
        }
    );
    failures +=
        execute_statement_ok(database, "ALTER TABLE app.qualified_numbers ALTER n SET DEFAULT 5");
    failures += expect_statement_result(
        database,
        "INSERT INTO app.qualified_numbers (id) VALUES (2)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n FROM qualified_numbers",
            .values = qualified_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "schema-qualified DROP DEFAULT restores with SET DEFAULT",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_numbers ALTER n DROP DEFAULT",
        (struct expected_sql_error){
            mysql_error_table_does_not_exist,
            "42S02",
            "doesn't exist",
        }
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE numbers ("
        "id INT NOT NULL, "
        "v INT DEFAULT 1, "
        "implicit_i INT, "
        "nullable_i INT DEFAULT NULL, "
        "nn INT NOT NULL DEFAULT 2, "
        "u INT UNSIGNED DEFAULT 4, "
        "bu BIGINT UNSIGNED DEFAULT 5, "
        "b BOOL DEFAULT FALSE)"
    );
    session = mylite_connection_session_state(database);
    failures += expect_true(session != NULL, "alter drop default session state exists");
    if (session != NULL) {
        sqlite_schema_generation = session->sqlite_schema_generation;
    }
    failures += expect_statement_result(
        database,
        "INSERT INTO numbers (id) VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER v DROP DEFAULT");
    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER nullable_i DROP DEFAULT");
    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER COLUMN nn DROP DEFAULT");
    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER u DROP DEFAULT");
    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER bu DROP DEFAULT");
    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER b DROP DEFAULT");
    session = mylite_connection_session_state(database);
    failures += expect_true(
        session != NULL && session->sqlite_schema_generation == sqlite_schema_generation,
        "ALTER DROP DEFAULT preserves SQLite schema generation"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM numbers LIKE 'v'",
            .values = show_v,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS after ALTER DROP DEFAULT",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "DESCRIBE numbers",
            .values = described_numbers,
            .column_count = show_columns_column_count,
            .row_count = drop_default_description_row_count,
            .context = "DESCRIBE after ALTER DROP DEFAULT",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "EXPLAIN numbers",
            .values = described_numbers,
            .column_count = show_columns_column_count,
            .row_count = drop_default_description_row_count,
            .context = "EXPLAIN table after ALTER DROP DEFAULT",
        }
    );
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE numbers",
            .needle = "`implicit_i` int DEFAULT NULL",
            .context = "SHOW CREATE keeps implicit nullable default",
        }
    );
    failures += expect_single_value_not_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE numbers",
            .needle = "`v` int DEFAULT",
            .context = "SHOW CREATE omits dropped nullable default",
        }
    );
    failures += expect_single_value_not_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE numbers",
            .needle = "`nullable_i` int DEFAULT",
            .context = "SHOW CREATE omits dropped DEFAULT NULL",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, nullable_i, nn, u, bu, b FROM numbers WHERE id = 1",
            .values = original_row,
            .column_count = alter_set_default_projection_column_count,
            .row_count = 1U,
            .context = "ALTER DROP DEFAULT preserves existing rows",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO numbers (id) VALUES (2)",
        (struct expected_sql_error){
            mysql_error_field_no_default,
            "HY000",
            "Field 'v' doesn't have a default value",
        }
    );
    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER v SET DEFAULT 9");
    failures += execute_error(
        database,
        "INSERT INTO numbers (id) VALUES (2)",
        (struct expected_sql_error){
            mysql_error_field_no_default,
            "HY000",
            "Field 'nullable_i' doesn't have a default value",
        }
    );
    failures +=
        execute_statement_ok(database, "ALTER TABLE numbers ALTER nullable_i SET DEFAULT NULL");
    failures += execute_error(
        database,
        "INSERT INTO numbers (id) VALUES (2)",
        (struct expected_sql_error){
            mysql_error_field_no_default,
            "HY000",
            "Field 'nn' doesn't have a default value",
        }
    );
    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER nn SET DEFAULT 7");
    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER u SET DEFAULT 40");
    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER bu SET DEFAULT 50");
    failures += execute_statement_ok(database, "ALTER TABLE numbers ALTER b SET DEFAULT TRUE");
    failures += expect_statement_result(
        database,
        "INSERT INTO numbers (id) VALUES (2)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, nullable_i, nn, u, bu, b FROM numbers WHERE id = 2",
            .values = restored_row,
            .column_count = alter_set_default_projection_column_count,
            .row_count = 1U,
            .context = "ALTER SET DEFAULT after DROP DEFAULT restores future rows",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers ALTER missing DROP DEFAULT",
        (struct expected_sql_error){mysql_error_unknown_column, "42S22", "Unknown column"}
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private ALTER v DROP DEFAULT",
        (struct expected_sql_error){
            mysql_error_incorrect_table_name,
            "42000",
            "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers ALTER _mylite_private DROP DEFAULT",
        (struct expected_sql_error){
            mysql_error_incorrect_column_name,
            "42000",
            "Incorrect column name",
        }
    );

    failures += execute_statement_ok(database, "ALTER TABLE numbers RENAME TO renamed_numbers");
    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "ALTER DROP DEFAULT preserves MyLite preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen alter drop default");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, nullable_i, nn, u, bu, b "
                   "FROM renamed_numbers WHERE id = 2",
            .values = restored_row,
            .column_count = alter_set_default_projection_column_count,
            .row_count = 1U,
            .context = "reopened ALTER DROP DEFAULT values persist after rename",
        }
    );
    failures += execute_statement_ok(database, "DROP TABLE renamed_numbers");
    failures += execute_error(
        database,
        "ALTER TABLE renamed_numbers ALTER v DROP DEFAULT",
        (struct expected_sql_error){
            mysql_error_table_does_not_exist,
            "42S02",
            "doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_default_diagnostics_and_if_not_exists(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open default diagnostics");
    failures += seed_schema(database);
    failures += execute_error(
        database,
        "CREATE TABLE bad_null (id INT NOT NULL DEFAULT NULL)",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_int (id INT DEFAULT 2147483648)",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_unsigned (id INT UNSIGNED DEFAULT -1)",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_big_unsigned (id BIGINT UNSIGNED DEFAULT 9223372036854775808)",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_tiny (id TINYINT DEFAULT 128)",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_quoted_int (id INT DEFAULT 'abc')",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_quoted_unsigned (id INT UNSIGNED DEFAULT '-1')",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_quoted_big_signed (id BIGINT DEFAULT '9223372036854775808')",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_quoted_big_unsigned ("
        "id BIGINT UNSIGNED DEFAULT '9223372036854775808')",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );
    failures += execute_statement_ok(database, "CREATE TABLE no_default (id INT NOT NULL, v INT)");
    failures += execute_error(
        database,
        "INSERT INTO no_default (v) VALUES (1)",
        (struct expected_sql_error
        ){mysql_error_field_no_default, "HY000", "doesn't have a default value"}
    );
    failures += execute_statement_ok(database, "CREATE TABLE ifne (id INT)");
    failures += expect_statement_result(
        database,
        "CREATE TABLE IF NOT EXISTS ifne (id INT DEFAULT 2147483648)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 1U}
    );
    failures += execute_error(
        database,
        "CREATE TABLE IF NOT EXISTS ifne (bad INT NOT NULL DEFAULT NULL)",
        (struct expected_sql_error){mysql_error_invalid_default, "42000", "Invalid default value"}
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_catalog_v1_migration(void) {
    static const char *const migrated_default[] = {"v", "int", "YES", "", NULL, ""};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "migration") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open migration source");
    failures += seed_schema(database);
    failures += execute_statement_ok(database, "CREATE TABLE migrated (id INT, v INT)");
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += make_catalog_look_like_v1(sqlite);
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open migrated catalog");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM migrated LIKE 'v'",
            .values = migrated_default,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "migrated v1 column has no integer default",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_catalog_v2_migration(void) {
    static const char *const restored_row[] = {"2", "7", NULL};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "migration_v2") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open v2 migration source");
    failures += seed_schema(database);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE migrated (id INT NOT NULL, v INT DEFAULT 7, n INT DEFAULT NULL)"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += make_catalog_look_like_v2(sqlite);
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open v2 migrated catalog");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "ALTER TABLE migrated ALTER n DROP DEFAULT");
    failures += execute_error(
        database,
        "INSERT INTO migrated (id) VALUES (1)",
        (struct expected_sql_error){
            mysql_error_field_no_default,
            "HY000",
            "Field 'n' doesn't have a default value",
        }
    );
    failures += execute_statement_ok(database, "ALTER TABLE migrated ALTER n SET DEFAULT NULL");
    failures += expect_statement_result(
        database,
        "INSERT INTO migrated (id) VALUES (2)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, n FROM migrated",
            .values = restored_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "v2 catalog migration admits dropped default state",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_independent_default_handles(void) {
    static const char *const first_values[] = {"1", "3"};
    static const char *const second_values[] = {"2", "2", "2"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first_handle") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second_handle") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first defaults");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second defaults");
    failures += seed_schema(first);
    failures += seed_schema(second);
    failures += execute_statement_ok(first, "CREATE TABLE t (id INT NOT NULL, v INT DEFAULT 1)");
    failures += execute_statement_ok(second, "CREATE TABLE t (id INT NOT NULL, v INT DEFAULT 2)");
    failures += expect_statement_result(
        first,
        "INSERT INTO t (id) VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        second,
        "INSERT INTO t (id) VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += execute_statement_ok(first, "ALTER TABLE t ALTER v SET DEFAULT 3");
    failures += expect_statement_result(
        first,
        "INSERT INTO t (id) VALUES (2)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        second,
        "INSERT INTO t (id) VALUES (2)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += execute_statement_ok(first, "ALTER TABLE t ALTER v DROP DEFAULT");
    failures += execute_error(
        first,
        "INSERT INTO t (id) VALUES (3)",
        (struct expected_sql_error){
            mysql_error_field_no_default,
            "HY000",
            "doesn't have a default value",
        }
    );
    failures += expect_statement_result(
        second,
        "INSERT INTO t (id) VALUES (3)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT v FROM t ORDER BY id",
            .values = first_values,
            .column_count = 1U,
            .row_count = 2U,
            .context = "first handle altered default value",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT v FROM t ORDER BY id",
            .values = second_values,
            .column_count = 1U,
            .row_count = 3U,
            .context = "second handle independent dropped default state",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);

    return failures;
}

static int seed_schema(mylite_db *database) {
    int failures = expect_statement_result(
        database,
        "CREATE DATABASE app",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );

    failures += execute_statement_ok(database, "USE app");
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
            sql,
            rc,
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
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    return expect_statement_result(
        database,
        sql,
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
}

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
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
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    if (query.values != NULL) {
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                size_t value_index = (row * query.column_count) + column;

                failures += expect_result_value(
                    result,
                    row,
                    column,
                    query.values[value_index],
                    query.context
                );
            }
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
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
        return expect_true(actual == NULL, context);
    }

    return expect_text(actual, expected, context);
}

static int expect_single_value_contains(mylite_db *database, struct expected_contains_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), 2U, query.context);
    failures += expect_size(mylite_result_row_count(result), 1U, query.context);
    failures +=
        expect_contains(mylite_result_value_text(result, 0U, 1U), query.needle, query.context);
    mylite_result_free(result);

    return failures;
}

static int expect_single_value_not_contains(
    mylite_db *database,
    struct expected_contains_query query
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), 2U, query.context);
    failures += expect_size(mylite_result_row_count(result), 1U, query.context);
    failures +=
        expect_not_contains(mylite_result_value_text(result, 0U, 1U), query.needle, query.context);
    mylite_result_free(result);

    return failures;
}

static int make_catalog_look_like_v1(sqlite3 *sqlite) {
    int failures = 0;

    failures += execute_sql(sqlite, "DROP TABLE _mylite_catalog_index_columns");
    failures += execute_sql(sqlite, "DROP TABLE _mylite_catalog_indexes");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN auto_increment_next");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN default_charset");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN default_collation");
    failures += execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN comment");
    failures += execute_sql(
        sqlite,
        "ALTER TABLE _mylite_catalog_tables DROP COLUMN fulltext_doc_id_initialized"
    );
    failures += execute_sql(
        sqlite,
        "ALTER TABLE _mylite_catalog_tables DROP COLUMN created_time_utc_epoch"
    );
    failures += execute_sql(
        sqlite,
        "ALTER TABLE _mylite_catalog_tables DROP COLUMN updated_time_utc_epoch"
    );
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN row_format_option");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN key_block_size");
    failures += execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN pack_keys");
    failures += execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN checksum");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN stats_persistent");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN stats_auto_recalc");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN stats_sample_pages");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_schemas DROP COLUMN default_charset");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_schemas DROP COLUMN default_collation");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_columns DROP COLUMN is_auto_increment");
    failures += execute_sql(sqlite, "ALTER TABLE _mylite_catalog_columns DROP COLUMN default_kind");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_columns DROP COLUMN default_integer");
    failures += execute_sql(sqlite, "ALTER TABLE _mylite_catalog_columns DROP COLUMN is_visible");
    failures += execute_sql(
        sqlite,
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 1, minimum_reader_schema_version = 1"
    );

    return failures;
}

static int make_catalog_look_like_v2(sqlite3 *sqlite) {
    int failures = 0;

    failures += execute_sql(sqlite, "DROP TABLE _mylite_catalog_index_columns");
    failures += execute_sql(sqlite, "DROP TABLE _mylite_catalog_indexes");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN auto_increment_next");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN default_charset");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN default_collation");
    failures += execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN comment");
    failures += execute_sql(
        sqlite,
        "ALTER TABLE _mylite_catalog_tables DROP COLUMN fulltext_doc_id_initialized"
    );
    failures += execute_sql(
        sqlite,
        "ALTER TABLE _mylite_catalog_tables DROP COLUMN created_time_utc_epoch"
    );
    failures += execute_sql(
        sqlite,
        "ALTER TABLE _mylite_catalog_tables DROP COLUMN updated_time_utc_epoch"
    );
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN row_format_option");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN key_block_size");
    failures += execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN pack_keys");
    failures += execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN checksum");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN stats_persistent");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN stats_auto_recalc");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN stats_sample_pages");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_schemas DROP COLUMN default_charset");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_schemas DROP COLUMN default_collation");
    failures += execute_sql(
        sqlite,
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v3;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1)),"
        "default_integer INTEGER,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, default_kind, default_integer, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, default_kind, default_integer, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v3;"
        "DROP TABLE _mylite_catalog_columns_v3;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 2, minimum_reader_schema_version = 2;"
    );

    return failures;
}

static int execute_sql(sqlite3 *connection, const char *sql) {
    int rc = sqlite3_exec(connection, sql, NULL, NULL, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "sqlite exec failed for '%s': %d\n", sql, rc);
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
        "%s/mylite_integer_default_literals_%d_%s.mylite",
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
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        (void)fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
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

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: condition is false\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected '%s', got '%s'\n",
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
            "%s: expected '%s' to contain '%s'\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
        );
        return 1;
    }

    return 0;
}

static int expect_not_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) != NULL) {
        fprintf(
            stderr,
            "%s: expected '%s' not to contain '%s'\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
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
