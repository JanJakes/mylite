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
    show_columns_set_row_count = 5,
    information_schema_set_field_count = 9,
    set_values_row_count = 5,
    set_flags_display_length = 96,
    set_nullable_display_length = 12,
    mysql_collation_utf8mb4_0900_ai_ci_id = 255,
    mysql_error_parse = 1064,
    mysql_error_duplicated_value_in_set = 1291,
    mysql_error_illegal_set_value = 1367,
    mysql_error_invalid_default = 1067,
    mysql_error_no_default = 1364,
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

static int test_set_success_metadata_dml_and_persistence(void);
static int test_set_diagnostics(void);
static int test_independent_set_handles(void);
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
static int expect_set_metadata(mylite_db *database);
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

    failures += test_set_success_metadata_dml_and_persistence();
    failures += test_set_diagnostics();
    failures += test_independent_set_handles();

    return failures == 0 ? 0 : 1;
}

static int test_set_success_metadata_dml_and_persistence(void) {
    static const char *const show_columns_rows[] = {
        "id",
        "int",
        "NO",
        "PRI",
        NULL,
        "auto_increment",
        "flags",
        "set('active','featured','archived')",
        "NO",
        "",
        NULL,
        "",
        "nullable_flags",
        "set('b','a')",
        "YES",
        "",
        NULL,
        "",
        "spaced",
        "set('x','y')",
        "YES",
        "",
        "y",
        "",
        "numericish",
        "set('0','1','2')",
        "YES",
        "",
        "0,2",
        "",
    };
    static const char *const information_schema_rows[] = {
        "flags",
        "set",
        "set('active','featured','archived')",
        "24",
        "96",
        "NO",
        NULL,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "nullable_flags",
        "set",
        "set('b','a')",
        "3",
        "12",
        "YES",
        NULL,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "numericish",
        "set",
        "set('0','1','2')",
        "5",
        "20",
        "YES",
        "0,2",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "spaced",
        "set",
        "set('x','y')",
        "3",
        "12",
        "YES",
        "y",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const inserted_rows[] = {
        "1",   "active,featured",
        "a",   "1",
        "2",   "active,archived",
        "b,a", "0,2",
        "3",   "featured",
        NULL,  "0,1",
        "4",   "active",
        NULL,  "0",
        "5",   "",
        NULL,  "",
    };
    static const char *const string_predicate_ids[] = {"1"};
    static const char *const numeric_predicate_ids[] = {"2"};
    static const char *const quoted_numeric_predicate_ids[] = {"4"};
    static const char *const null_safe_ids[] = {"3", "4", "5"};
    static const char *const not_equal_ids[] = {"2"};
    static const char *const updated_rows[] = {
        "1",
        "archived",
        "2",
        "active,archived",
        "3",
        "archived",
        "4",
        "archived",
        "5",
        "archived",
    };
    static const char *const insert_set_rows[] = {
        "6",
        "active,featured",
        "a",
        "0,2",
    };
    static const char *const replace_rows[] = {
        "1",
        "active,archived",
        "b,a",
        "0,1",
    };
    static const char *const trailing_rows[] = {
        "1",
        "y",
        "2",
        "x",
        "3",
        "x,y",
    };
    static const char *const added_rows[] = {
        "1",
        "red",
        "2",
        "red",
    };
    static const char *const altered_default_show_rows[] = {
        "v",
        "set('a','b')",
        "YES",
        "",
        "a,b",
        "",
    };
    static const char *const altered_default_rows[] = {
        "1",
        "a",
        "2",
        "a,b",
        "3",
        NULL,
        "4",
        "",
    };
    static const char *const renamed_rows[] = {"a,b"};
    static const char *const reopened_rows[] = {
        "1",
        "archived",
        "1",
        "2",
        "active,archived",
        "0,2",
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open set file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE set_values ("
        "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
        "flags SET('active','featured','archived') NOT NULL, "
        "nullable_flags SET('b','a') NULL, "
        "spaced SET('x ','y  ') DEFAULT 'y', "
        "numericish SET('0','1','2') DEFAULT '2,0')"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM set_values",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = show_columns_set_row_count,
            .context = "set SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "DESCRIBE set_values",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = show_columns_set_row_count,
            .context = "set DESCRIBE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "EXPLAIN set_values",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = show_columns_set_row_count,
            .context = "set EXPLAIN table",
        }
    );
    failures += expect_query_value_contains(
        database,
        "SHOW CREATE TABLE set_values",
        0U,
        1U,
        "`flags` set('active','featured','archived') NOT NULL",
        "set SHOW CREATE TABLE flags"
    );
    failures += expect_query_value_contains(
        database,
        "SHOW CREATE TABLE set_values",
        0U,
        1U,
        "`spaced` set('x','y') DEFAULT 'y'",
        "set SHOW CREATE TABLE normalized spaced labels"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, IS_NULLABLE, COLUMN_DEFAULT, CHARACTER_SET_NAME, "
                   "COLLATION_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'set_values' "
                   "AND COLUMN_NAME <> 'id' ORDER BY COLUMN_NAME",
            .values = information_schema_rows,
            .column_count = information_schema_set_field_count,
            .row_count = 4U,
            .context = "set INFORMATION_SCHEMA.COLUMNS",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO set_values (flags, nullable_flags, numericish) VALUES "
        "('featured,active,active','a','1'), "
        "(5,'A,b',5), ('FEATURED',NULL,'3'), ('active',DEFAULT,'0'), ('',NULL,'')",
        (struct expected_dml_result){.affected_rows = set_values_row_count, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flags, nullable_flags, numericish FROM set_values ORDER BY id",
            .values = inserted_rows,
            .column_count = 4U,
            .row_count = set_values_row_count,
            .context = "set inserted rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM set_values WHERE flags = 'ACTIVE,featured' ORDER BY id",
            .values = string_predicate_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "set case-insensitive canonical string predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM set_values WHERE flags = 5 ORDER BY id",
            .values = numeric_predicate_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "set numeric bitmap predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM set_values WHERE numericish = '0' ORDER BY id",
            .values = quoted_numeric_predicate_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "set quoted numeric string predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM set_values WHERE flags = 'featured,active' ORDER BY id",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "set predicate preserves member order",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM set_values WHERE nullable_flags <=> NULL ORDER BY id",
            .values = null_safe_ids,
            .column_count = 1U,
            .row_count = 3U,
            .context = "set null-safe predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM set_values WHERE nullable_flags <> 'a' ORDER BY id",
            .values = not_equal_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "set not-equal predicate",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE set_values SET flags = 'archived' WHERE flags <> 5",
        (struct expected_dml_result){.affected_rows = 4, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flags FROM set_values ORDER BY id",
            .values = updated_rows,
            .column_count = 2U,
            .row_count = set_values_row_count,
            .context = "set updated rows",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO set_values SET flags = 'featured,active ', nullable_flags = 'a ', "
        "numericish = '5'",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flags, nullable_flags, numericish FROM set_values "
                   "WHERE id >= 6 ORDER BY id",
            .values = insert_set_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "set INSERT SET conversion",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE set_replace (id INT, flags SET('active','featured','archived'), "
        "nullable_flags SET('b','a'), numericish SET('0','1','2'))"
    );
    failures += expect_dml_ok(
        database,
        "REPLACE INTO set_replace VALUES (1, 'archived,active ', 'A,b ', 3)",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flags, nullable_flags, numericish FROM set_replace",
            .values = replace_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "set REPLACE conversion",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE set_trailing (id INT, v SET('x','y') DEFAULT 'y ')"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO set_trailing VALUES (1, DEFAULT), (2, 'x '), (3, 'y,x ')",
        (struct expected_dml_result){.affected_rows = 3, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM set_trailing ORDER BY id",
            .values = trailing_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "set trailing-space value conversion",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE set_clone LIKE set_values");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM set_clone",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = show_columns_set_row_count,
            .context = "set CREATE TABLE LIKE columns",
        }
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE set_values ADD COLUMN added SET('red','blue') NOT NULL DEFAULT 'red'"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, added FROM set_values WHERE id <= 2 ORDER BY id",
            .values = added_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "set ALTER TABLE ADD COLUMN backfill",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE set_alter_default (id INT, v SET('a','b') NULL DEFAULT 'a')"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO set_alter_default (id) VALUES (1)",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "ALTER TABLE set_alter_default ALTER COLUMN v SET DEFAULT 'b,a'",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM set_alter_default LIKE 'v'",
            .values = altered_default_show_rows,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "set ALTER COLUMN SET DEFAULT metadata",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO set_alter_default (id) VALUES (2)",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "ALTER TABLE set_alter_default ALTER COLUMN v SET DEFAULT NULL",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO set_alter_default (id) VALUES (3)",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "ALTER TABLE set_alter_default ALTER COLUMN v DROP DEFAULT",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 0U}
    );
    failures += execute_error(
        database,
        "INSERT INTO set_alter_default (id) VALUES (4)",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'v' doesn't have a default value",
        }
    );
    failures += expect_dml_ok(
        database,
        "ALTER TABLE set_alter_default ALTER COLUMN v SET DEFAULT ''",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO set_alter_default (id) VALUES (4)",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM set_alter_default ORDER BY id",
            .values = altered_default_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "set ALTER COLUMN defaults affect future rows",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE set_rename (id INT, v SET('a','b'))");
    failures += expect_statement_ok(database, "INSERT INTO set_rename VALUES (1, 'b,a')");
    failures += expect_statement_ok(database, "RENAME TABLE set_rename TO set_renamed");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v FROM set_renamed",
            .values = renamed_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "set table rename preserves descriptors",
        }
    );
    failures += expect_statement_ok(database, "DROP TABLE set_renamed");
    failures += expect_set_metadata(database);
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen set file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flags, numericish FROM set_values WHERE id <= 2 ORDER BY id",
            .values = reopened_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "set reopened rows",
        }
    );
    mylite_close(database);
    database = NULL;
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read set file preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "set file preamble"
    );

    remove_related_files(path);
    return failures;
}

static int test_set_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open set diagnostics");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "CREATE TABLE duplicate_set (v SET('a ', 'A'))",
        (struct expected_sql_error){
            .code = mysql_error_duplicated_value_in_set,
            .sqlstate = "HY000",
            .message_part = "duplicated value 'a' in SET",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE comma_set (v SET('a,b'))",
        (struct expected_sql_error){
            .code = mysql_error_illegal_set_value,
            .sqlstate = "22007",
            .message_part = "Illegal set 'a,b' value found during parsing",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE empty_set (v SET())",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE empty_member (v SET('', 'a'))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SET empty-string members are not yet supported",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE numeric_default (v SET('a','b') DEFAULT 2)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'v'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE invalid_default (v SET('a','b') DEFAULT 'c')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'v'",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE alter_default_diag (v SET('a','b'))");
    failures += execute_error(
        database,
        "ALTER TABLE alter_default_diag ALTER COLUMN v SET DEFAULT 2",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'v'",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE set_values (v SET('a','b') NOT NULL)");
    failures += expect_statement_ok(database, "INSERT INTO set_values VALUES ('a')");
    failures += execute_error(
        database,
        "INSERT INTO set_values VALUES (DEFAULT)",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'v' doesn't have a default value",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO set_values VALUES (NULL)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'v' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO set_values VALUES ('missing')",
        (struct expected_sql_error){
            .code = mysql_error_data_truncated,
            .sqlstate = "01000",
            .message_part = "Data truncated for column 'v' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO set_values VALUES (-1)",
        (struct expected_sql_error){
            .code = mysql_error_data_truncated,
            .sqlstate = "01000",
            .message_part = "Data truncated for column 'v' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO set_values VALUES (4)",
        (struct expected_sql_error){
            .code = mysql_error_data_truncated,
            .sqlstate = "01000",
            .message_part = "Data truncated for column 'v' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO set_values VALUES ('a,missing')",
        (struct expected_sql_error){
            .code = mysql_error_data_truncated,
            .sqlstate = "01000",
            .message_part = "Data truncated for column 'v' at row 1",
        }
    );
    failures += execute_error(
        database,
        "SELECT v FROM set_values ORDER BY v",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ORDER BY does not yet support SET columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT v FROM set_values WHERE v > 'a'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE set predicates support only =, <=>, <>, and !=",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO set_values SELECT v FROM set_values",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INSERT ... SELECT does not support implicit SET conversion",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE set_source (v SET('a','b'))");
    failures += expect_statement_ok(database, "INSERT INTO set_source VALUES ('b')");
    failures += execute_error(
        database,
        "UPDATE set_values SET v = (SELECT v FROM set_source LIMIT 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UPDATE scalar subquery assignment does not support implicit SET "
                            "conversion",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE set_values MODIFY v SET('a','b')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "ALTER TABLE MODIFY COLUMN supports only compatible baseline integer, character, "
                "text, binary string, and temporal column replacements",
        }
    );
    failures += execute_error(
        database,
        "CREATE INDEX v_idx ON set_values (v)",
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

static int test_independent_set_handles(void) {
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first set handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second set handle");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (v SET('a','b'))");
    failures += expect_statement_ok(second, "CREATE TABLE t (v SET('a','b'))");
    failures += expect_statement_ok(first, "INSERT INTO t VALUES ('a')");
    failures += expect_statement_ok(second, "INSERT INTO t VALUES ('b')");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT v FROM t",
            .values = first_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first set handle rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT v FROM t",
            .values = second_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second set handle rows",
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

static int expect_set_metadata(mylite_db *database) {
    mylite_result *result = NULL;
    int failures =
        execute_ok(database, "SELECT flags, nullable_flags FROM set_values LIMIT 0", &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 2U, "set metadata columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "set metadata rows");
        failures += expect_int(
            mylite_result_column_type(result, 0U),
            MYLITE_RESULT_COLUMN_TYPE_STRING,
            "set flags type"
        );
        failures += expect_uint32(
            mylite_result_column_flags(result, 0U),
            MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_SET |
                MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
            "set flags metadata flags"
        );
        failures += expect_uint32(
            mylite_result_column_flags(result, 1U),
            MYLITE_RESULT_COLUMN_FLAG_SET,
            "set nullable metadata flags"
        );
        failures += expect_uint32(
            mylite_result_column_charset_id(result, 0U),
            mysql_collation_utf8mb4_0900_ai_ci_id,
            "set flags charset"
        );
        failures += expect_uint32(
            mylite_result_column_collation_id(result, 0U),
            mysql_collation_utf8mb4_0900_ai_ci_id,
            "set flags collation"
        );
        failures += expect_uint64(
            mylite_result_column_display_length(result, 0U),
            set_flags_display_length,
            "set flags display length"
        );
        failures += expect_uint64(
            mylite_result_column_display_length(result, 1U),
            set_nullable_display_length,
            "set nullable display length"
        );
    }
    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written =
        snprintf(path, path_size, "runtime_set_type_%s_%d.mylite", name, current_process_id());

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
    if (actual != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %s, got %s\n",
        context,
        expected,
        actual == NULL ? "(null)" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %s to contain %s\n",
        context,
        actual == NULL ? "(null)" : actual,
        needle
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
    fprintf(stderr, "%s: byte comparison failed\n", context);
    return 1;
}
