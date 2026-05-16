#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

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
    test_path_suffix_capacity = 16,
    show_columns_field_count = 6,
    show_columns_enum_row_count = 5,
    information_schema_enum_field_count = 9,
    enum_status_display_length = 36,
    large_enum_sql_capacity = 1200,
    mysql_collation_utf8mb4_0900_ai_ci_id = 255,
    mysql_error_parse = 1064,
    mysql_error_duplicated_value_in_enum = 1291,
    mysql_error_invalid_default = 1067,
    mysql_error_bad_null = 1048,
    mysql_error_data_truncated = 1265,
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

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_enum_success_metadata_dml_and_persistence(void);
static int test_enum_diagnostics(void);
static int test_independent_enum_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, struct expected_dml_result expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_query_value_contains(
    mylite_db *database,
    const char *sql,
    size_t row,
    size_t column,
    const char *needle,
    const char *context
);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_enum_metadata(mylite_db *database);
static int expect_large_enum_descriptor_error(mylite_db *database);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_uint32(uint32_t actual, uint32_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
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

    failures += test_enum_success_metadata_dml_and_persistence();
    failures += test_enum_diagnostics();
    failures += test_independent_enum_handles();

    return failures == 0 ? 0 : 1;
}

static int test_enum_success_metadata_dml_and_persistence(void) {
    static const char *const show_columns_rows[] = {
        "id",
        "int",
        "NO",
        "PRI",
        NULL,
        "auto_increment",
        "status",
        "enum('draft','published')",
        "NO",
        "",
        NULL,
        "",
        "nullable_status",
        "enum('b','a')",
        "YES",
        "",
        NULL,
        "",
        "spaced",
        "enum('x','y')",
        "YES",
        "",
        "y",
        "",
        "numericish",
        "enum('0','1','2')",
        "YES",
        "",
        "2",
        "",
    };
    static const char *const information_schema_rows[] = {
        "nullable_status",
        "enum",
        "enum('b','a')",
        "1",
        "4",
        "YES",
        NULL,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "numericish",
        "enum",
        "enum('0','1','2')",
        "1",
        "4",
        "YES",
        "2",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "spaced",
        "enum",
        "enum('x','y')",
        "1",
        "4",
        "YES",
        "y",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "status",
        "enum",
        "enum('draft','published')",
        "9",
        "36",
        "NO",
        NULL,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const inserted_rows[] = {
        "1",
        "draft",
        "a",
        "2",
        "2",
        "published",
        "b",
        "1",
        "3",
        "draft",
        NULL,
        "2",
        "4",
        "draft",
        NULL,
        "0",
    };
    static const char *const draft_ids[] = {"1", "3", "4"};
    static const char *const ordinal_ids[] = {"2"};
    static const char *const null_safe_ids[] = {"3", "4"};
    static const char *const not_equal_ids[] = {"1"};
    static const char *const row_count_after_trailing_predicate_dml[] = {"4"};
    static const char *const updated_rows[] = {
        "1",
        "published",
        "2",
        "published",
        "3",
        "published",
        "4",
        "published",
    };
    static const char *const insert_set_rows[] = {
        "5",
        "draft",
        "a",
        "2",
    };
    static const char *const replace_rows[] = {
        "1",
        "published",
        "b",
        "0",
    };
    static const char *const trailing_rows[] = {
        "1",
        "y",
        "2",
        "x",
        "3",
        "x",
    };
    static const char *const added_rows[] = {
        "1",
        "red",
        "2",
        "red",
    };
    static const char *const empty_label_rows[] = {"1", "", "2", "", "3", "a"};
    static const char *const renamed_rows[] = {"b"};
    static const char *const reopened_rows[] = {
        "1",
        "published",
        "2",
        "2",
        "published",
        "1",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open enum file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE enum_values ("
        "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
        "status ENUM('draft','published') NOT NULL, "
        "nullable_status ENUM('b','a') NULL, "
        "spaced ENUM('x ','y  ') DEFAULT 'y', "
        "numericish ENUM('0','1','2') DEFAULT '2')"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM enum_values",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = show_columns_enum_row_count,
            .context = "enum SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "DESCRIBE enum_values",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = show_columns_enum_row_count,
            .context = "enum DESCRIBE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "EXPLAIN enum_values",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = show_columns_enum_row_count,
            .context = "enum EXPLAIN table",
        }
    );
    failures += expect_query_value_contains(
        database,
        "SHOW CREATE TABLE enum_values",
        0U,
        1U,
        "`status` enum('draft','published') NOT NULL",
        "enum SHOW CREATE TABLE status"
    );
    failures += expect_query_value_contains(
        database,
        "SHOW CREATE TABLE enum_values",
        0U,
        1U,
        "`spaced` enum('x','y') DEFAULT 'y'",
        "enum SHOW CREATE TABLE normalized spaced labels"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, IS_NULLABLE, COLUMN_DEFAULT, CHARACTER_SET_NAME, "
                   "COLLATION_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'enum_values' "
                   "AND COLUMN_NAME <> 'id' ORDER BY COLUMN_NAME",
            .values = information_schema_rows,
            .column_count = information_schema_enum_field_count,
            .row_count = 4U,
            .context = "enum INFORMATION_SCHEMA.COLUMNS",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO enum_values (status, nullable_status, numericish) VALUES "
        "('draft','a','2'), ('Published','B',2), (1,NULL,'3'), (DEFAULT, DEFAULT, '0')",
        (struct expected_dml_result){.affected_rows = 4, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, status, nullable_status, numericish FROM enum_values ORDER BY id",
            .values = inserted_rows,
            .column_count = 4U,
            .row_count = 4U,
            .context = "enum inserted rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM enum_values WHERE status = 'DRAFT' ORDER BY id",
            .values = draft_ids,
            .column_count = 1U,
            .row_count = 3U,
            .context = "enum case-insensitive label predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM enum_values WHERE status = 2 ORDER BY id",
            .values = ordinal_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "enum ordinal predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM enum_values WHERE status = '2' ORDER BY id",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "enum quoted numeric predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM enum_values WHERE nullable_status <=> NULL ORDER BY id",
            .values = null_safe_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "enum null-safe predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM enum_values WHERE nullable_status <> 'b' ORDER BY id",
            .values = not_equal_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "enum not-equal predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM enum_values WHERE status = 'draft ' ORDER BY id",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "enum predicate preserves trailing spaces",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE enum_values SET nullable_status = 'a' WHERE status = 'draft '",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "DELETE FROM enum_values WHERE status = 'draft '",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM enum_values",
            .values = row_count_after_trailing_predicate_dml,
            .column_count = 1U,
            .row_count = 1U,
            .context = "enum trailing-space predicate DML row count",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE enum_values SET status = '2' WHERE status = 1",
        (struct expected_dml_result){.affected_rows = 3, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, status FROM enum_values ORDER BY id",
            .values = updated_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "enum updated rows",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO enum_values SET status = 'draft ', nullable_status = 'a ', numericish = '2 '",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, status, nullable_status, numericish FROM enum_values "
                   "WHERE id >= 5 ORDER BY id",
            .values = insert_set_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "enum INSERT SET conversion",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE enum_replace ("
        "id INT, status ENUM('draft','published'), nullable_status ENUM('b','a'), "
        "numericish ENUM('0','1','2'))"
    );
    failures += expect_dml_ok(
        database,
        "REPLACE INTO enum_replace VALUES (1, 'published ', 'B ', 1)",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, status, nullable_status, numericish FROM enum_replace",
            .values = replace_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "enum REPLACE conversion",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE enum_trailing (id INT, v ENUM('x','y') DEFAULT 'y ')"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO enum_trailing VALUES (1, DEFAULT), (2, 'x '), (3, '2 ')",
        (struct expected_dml_result){.affected_rows = 3, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE enum_trailing SET v = 'x ' WHERE id = 3",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM enum_trailing ORDER BY id",
            .values = trailing_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "enum trailing-space value conversion",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE enum_clone LIKE enum_values");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM enum_clone",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = show_columns_enum_row_count,
            .context = "enum CREATE TABLE LIKE columns",
        }
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE enum_values ADD COLUMN added ENUM('red','blue') NOT NULL"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, added FROM enum_values WHERE id <= 2 ORDER BY id",
            .values = added_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "enum ALTER TABLE ADD COLUMN backfill",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE empty_label (id INT, v ENUM('', 'a'))");
    failures += expect_dml_ok(
        database,
        "INSERT INTO empty_label VALUES (1,''), (2,1), (3,2)",
        (struct expected_dml_result){.affected_rows = 3, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM empty_label ORDER BY id",
            .values = empty_label_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "enum empty label rows",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE enum_rename (id INT, v ENUM('a','b'))");
    failures += expect_statement_ok(database, "INSERT INTO enum_rename VALUES (1, 'b')");
    failures += expect_statement_ok(database, "RENAME TABLE enum_rename TO enum_renamed");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v FROM enum_renamed",
            .values = renamed_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "enum table rename preserves descriptors",
        }
    );
    failures += expect_statement_ok(database, "DROP TABLE enum_renamed");
    failures += expect_enum_metadata(database);
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen enum file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, status, numericish FROM enum_values WHERE id <= 2 ORDER BY id",
            .values = reopened_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "enum reopened rows",
        }
    );
    mylite_close(database);
    database = NULL;
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read enum file preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "enum file preamble"
    );

    remove_related_files(path);
    return failures;
}

static int test_enum_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open enum diagnostics");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "CREATE TABLE duplicate_enum (v ENUM('a ', 'A'))",
        (struct expected_sql_error){
            .code = mysql_error_duplicated_value_in_enum,
            .sqlstate = "HY000",
            .message_part = "duplicated value 'a' in ENUM",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE empty_enum (v ENUM())",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE numeric_default (v ENUM('a','b') DEFAULT 2)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'v'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE nul_enum (v ENUM('a\\0b'))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ENUM labels do not support NUL bytes",
        }
    );
    failures += expect_large_enum_descriptor_error(database);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE enum_values (status ENUM('draft','published') NOT NULL)"
    );
    failures += expect_statement_ok(database, "INSERT INTO enum_values VALUES ('draft')");
    failures += execute_error(
        database,
        "INSERT INTO enum_values (status) VALUES ('missing')",
        (struct expected_sql_error){
            .code = mysql_error_data_truncated,
            .sqlstate = "01000",
            .message_part = "Data truncated for column 'status' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO enum_values (status) VALUES (0)",
        (struct expected_sql_error){
            .code = mysql_error_data_truncated,
            .sqlstate = "01000",
            .message_part = "Data truncated for column 'status' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO enum_values (status) VALUES ('3')",
        (struct expected_sql_error){
            .code = mysql_error_data_truncated,
            .sqlstate = "01000",
            .message_part = "Data truncated for column 'status' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO enum_values (status) VALUES (NULL)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'status' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "SELECT status FROM enum_values ORDER BY status",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ORDER BY supports only integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT status FROM enum_values WHERE status > 'draft'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE enum predicates support only =, <=>, <>, and !=",
        }
    );
    failures += execute_error(
        database,
        "SELECT status FROM enum_values WHERE status LIKE 'd%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE enum predicates support only =, <=>, <>, and !=",
        }
    );
    failures += execute_error(
        database,
        "SELECT status FROM enum_values WHERE status BETWEEN 'draft' AND 'published'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE enum predicates do not yet support BETWEEN",
        }
    );
    failures += execute_error(
        database,
        "SELECT status FROM enum_values WHERE status IN ('draft')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE enum predicates do not yet support IN",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO enum_values (status) SELECT status FROM enum_values",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INSERT ... SELECT does not support implicit ENUM conversion",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE enum_source (status ENUM('draft','published'))"
    );
    failures += expect_statement_ok(database, "INSERT INTO enum_source VALUES ('published')");
    failures += execute_error(
        database,
        "UPDATE enum_values SET status = (SELECT status FROM enum_source LIMIT 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UPDATE scalar subquery assignment does not support implicit ENUM "
                            "conversion",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE enum_values MODIFY status ENUM('draft','published')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ALTER TABLE MODIFY COLUMN supports only baseline integer, character, "
                            "and temporal columns",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE enum_key (v ENUM('a','b'))");
    failures += execute_error(
        database,
        "CREATE INDEX v_idx ON enum_key (v)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Secondary indexes do not yet support this column type",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_enum_handles(void) {
    static const char *const first_rows[] = {"a"};
    static const char *const second_rows[] = {"b"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first enum handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second enum handle");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (v ENUM('a','b'))");
    failures += expect_statement_ok(second, "CREATE TABLE t (v ENUM('a','b'))");
    failures += expect_statement_ok(first, "INSERT INTO t VALUES ('a')");
    failures += expect_statement_ok(second, "INSERT INTO t VALUES ('b')");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT v FROM t",
            .values = first_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first enum handle rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT v FROM t",
            .values = second_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second enum handle rows",
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

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s failed: %s (%d %s)\n",
            sql,
            mylite_errmsg(database),
            mylite_errcode(database),
            mylite_sqlstate(database)
        );
        return 1;
    }
    if (out_result == NULL) {
        mylite_result_free(result);
    } else {
        *out_result = result;
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
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    return execute_ok(database, sql, NULL);
}

static int expect_dml_ok(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (failures == 0) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                const char *expected = query.values[(row * query.column_count) + column];

                failures += expect_result_value(result, row, column, expected, query.context);
            }
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_value_contains(
    mylite_db *database,
    const char *sql,
    size_t row,
    size_t column,
    const char *needle,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_row_count(result), row + 1U, context);
        failures += expect_size(mylite_result_column_count(result), column + 1U, context);
    }
    if (failures == 0) {
        failures += expect_contains(mylite_result_value_text(result, row, column), needle, context);
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
            fprintf(stderr, "%s[%zu,%zu]: expected NULL, got %s\n", context, row, column, actual);
            return 1;
        }
        return 0;
    }
    return expect_text(actual, expected, context);
}

static int expect_enum_metadata(mylite_db *database) {
    mylite_result *result = NULL;
    int failures =
        execute_ok(database, "SELECT status, nullable_status FROM enum_values LIMIT 0", &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 2U, "enum metadata columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "enum metadata rows");
        failures += expect_int(
            mylite_result_column_type(result, 0U),
            MYLITE_RESULT_COLUMN_TYPE_STRING,
            "enum status type"
        );
        failures += expect_uint32(
            mylite_result_column_flags(result, 0U),
            MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_ENUM |
                MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
            "enum status flags"
        );
        failures += expect_uint32(
            mylite_result_column_flags(result, 1U),
            MYLITE_RESULT_COLUMN_FLAG_ENUM,
            "enum nullable flags"
        );
        failures += expect_uint32(
            mylite_result_column_charset_id(result, 0U),
            mysql_collation_utf8mb4_0900_ai_ci_id,
            "enum status charset"
        );
        failures += expect_uint32(
            mylite_result_column_collation_id(result, 0U),
            mysql_collation_utf8mb4_0900_ai_ci_id,
            "enum status collation"
        );
        failures += expect_uint64(
            mylite_result_column_display_length(result, 0U),
            enum_status_display_length,
            "enum status length"
        );
        failures += expect_uint64(
            mylite_result_column_display_length(result, 1U),
            4U,
            "enum nullable length"
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_large_enum_descriptor_error(mylite_db *database) {
    char sql[large_enum_sql_capacity];
    size_t offset = 0U;
    const size_t label_length = 1100U;
    int written = snprintf(sql, sizeof(sql), "CREATE TABLE large_enum (v ENUM('");

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        return 1;
    }
    offset = (size_t)written;
    if (offset + label_length + 3U >= sizeof(sql)) {
        return 1;
    }
    memset(sql + offset, 'a', label_length);
    offset += label_length;
    memcpy(sql + offset, "'))", 4U);

    return execute_error(
        database,
        sql,
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ENUM definition is too large for this MyLite build",
        }
    );
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written =
        snprintf(path, path_size, "runtime_enum_type_%s_%d.mylite", name, current_process_id());

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
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    fclose(file);
    return read_count == size ? 0 : 1;
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

static int expect_uint32(uint32_t actual, uint32_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %u, got %u\n", context, expected, actual);
    return 1;
}

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %llu, got %llu\n",
        context,
        (unsigned long long)expected,
        (unsigned long long)actual
    );
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %s, got %s\n",
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
        "%s: expected message containing %s, got %s\n",
        context,
        needle,
        actual == NULL ? "(null)" : actual
    );
    return 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) == 0) {
        return 0;
    }
    fprintf(stderr, "%s: bytes differ\n", context);
    return 1;
}
