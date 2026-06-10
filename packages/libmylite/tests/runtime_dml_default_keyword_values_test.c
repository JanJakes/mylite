#include <mylite/mylite.h>

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
    dml_default_projection_column_count = 10,
    empty_values_default_column_count = 5,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
    mysql_error_unknown_table = 1146,
    mysql_error_column_count = 1136,
    mysql_error_no_default = 1364,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_statement {
    int64_t affected_rows;
    size_t warning_count;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_empty_insert_values_lifecycle(void);
static int test_dml_default_success_persistence_and_rename(void);
static int test_dml_default_diagnostics_and_ignore_warnings(void);
static int test_dml_default_independent_handles(void);
static int seed_schema(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected,
    const char *context
);
static int expect_query_values(mylite_db *database, struct expected_query query);
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

    failures += test_empty_insert_values_lifecycle();
    failures += test_dml_default_success_persistence_and_rename();
    failures += test_dml_default_diagnostics_and_ignore_warnings();
    failures += test_dml_default_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_empty_insert_values_lifecycle(void) {
    static const char *const default_row[] = {"10", NULL, "x", "1.25", "2024-01-02"};
    static const char *const default_count[] = {"2"};
    static const char *const insert_value_row_rows[] = {
        "1",
        "2",
        "3",
        "4",
        "5",
        "6",
        "7",
        "8",
    };
    static const char *const expression_row[] = {"3", "7", NULL};
    static const char *const auto_rows[] = {"1", "5", "2", "5"};
    static const char *const auto_reopened_rows[] = {"1", "5", "2", "5", "3", "5"};
    static const char *const last_insert_id_one[] = {"1"};
    static const char *const last_insert_id_three[] = {"3"};
    static const char *const ignore_count[] = {"3"};
    static const char *const ignore_warnings[] = {
        "Warning",
        "1364",
        "Field 'id' doesn't have a default value",
        "Warning",
        "1364",
        "Field 's' doesn't have a default value",
        "Warning",
        "1364",
        "Field 'd' doesn't have a default value",
    };
    static const char *const ignore_row[] = {"0", NULL, "", "0000-00-00"};
    static const char *const replace_row[] = {"10", "x"};
    static const char *const replace_count[] = {"2"};
    static const char *const replace_multi_count[] = {"4"};
    static const char *const duplicate_value_twenty[] = {"20"};
    static const char *const duplicate_value_ten[] = {"10"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "empty_values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open empty values file");
    failures += seed_schema(database);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE defaults_t("
        "id INT NOT NULL DEFAULT 10, "
        "n INT NULL DEFAULT NULL, "
        "s VARCHAR(10) NOT NULL DEFAULT 'x', "
        "d DECIMAL(5,2) DEFAULT 1.25, "
        "dt DATE DEFAULT '2024-01-02')",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create empty values defaults table"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO defaults_t () VALUES ()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "explicit empty values insert"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, s, d, dt FROM defaults_t",
            .values = default_row,
            .column_count = empty_values_default_column_count,
            .row_count = 1U,
            .context = "explicit empty values row",
        }
    );
    failures += expect_statement_ok(
        database,
        "TRUNCATE defaults_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate empty values defaults"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO defaults_t VALUES ()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "omitted empty values insert"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, s, d, dt FROM defaults_t",
            .values = default_row,
            .column_count = empty_values_default_column_count,
            .row_count = 1U,
            .context = "omitted empty values row",
        }
    );
    failures += expect_statement_ok(
        database,
        "TRUNCATE defaults_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for multi empty values"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO defaults_t VALUES (), ()",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U},
        "multi omitted empty values insert"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM defaults_t",
            .values = default_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "multi omitted empty values count",
        }
    );
    failures += expect_statement_ok(
        database,
        "TRUNCATE defaults_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for explicit multi empty values"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO defaults_t () VALUES (), ()",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U},
        "multi empty values insert"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM defaults_t",
            .values = default_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "multi empty values count",
        }
    );
    failures += expect_statement_ok(
        database,
        "TRUNCATE defaults_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for value empty values"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO defaults_t VALUE ()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "value empty values insert"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, s, d, dt FROM defaults_t",
            .values = default_row,
            .column_count = empty_values_default_column_count,
            .row_count = 1U,
            .context = "value empty values row",
        }
    );
    failures += expect_statement_ok(
        database,
        "TRUNCATE defaults_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for row constructor empty values"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO defaults_t VALUES ROW(), ROW()",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U},
        "row constructor empty values insert"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM defaults_t",
            .values = default_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor empty values count",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE insert_row_syntax_t(id INT NOT NULL, v INT NOT NULL)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create value row syntax table"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO insert_row_syntax_t VALUE (1, 2), (3, 4)",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U},
        "value nonempty rows insert"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO insert_row_syntax_t VALUES ROW(5, 6), ROW(7, 8)",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U},
        "row constructor nonempty rows insert"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM insert_row_syntax_t ORDER BY id",
            .values = insert_value_row_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "value row syntax rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE expr_t("
        "a INT NOT NULL DEFAULT (1 + 2), "
        "b INT DEFAULT ((2 * 3) + 1), "
        "c INT DEFAULT (NULL))",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create empty values expression defaults"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO expr_t () VALUES ()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "empty values expression defaults"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, c FROM expr_t",
            .values = expression_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "expression default empty row",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE ai_t(id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, v INT NOT NULL DEFAULT 5)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create empty values auto increment table"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO ai_t () VALUES (), ()",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U},
        "multi empty values auto increment"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = last_insert_id_one,
            .column_count = 1U,
            .row_count = 1U,
            .context = "empty values first last insert id",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM ai_t ORDER BY id",
            .values = auto_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "empty values auto increment rows",
        }
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO ai_t VALUES ()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "omitted empty values auto increment"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = last_insert_id_three,
            .column_count = 1U,
            .row_count = 1U,
            .context = "empty values third last insert id",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE ignore_t(id INT NOT NULL, n INT NULL, s VARCHAR(5) NOT NULL, "
        "d DATE NOT NULL)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create empty values ignore table"
    );
    failures += expect_statement_ok(
        database,
        "INSERT IGNORE INTO ignore_t () VALUES ()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 3U},
        "empty values insert ignore"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = ignore_warnings,
            .column_count = 3U,
            .row_count = 3U,
            .context = "empty values insert ignore warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, s, d FROM ignore_t",
            .values = ignore_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "empty values insert ignore row",
        }
    );
    failures += expect_statement_ok(
        database,
        "INSERT IGNORE INTO ignore_t VALUES (), ()",
        (struct expected_statement){.affected_rows = 2, .warning_count = 3U},
        "multi omitted empty values insert ignore"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM ignore_t",
            .values = ignore_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "multi empty values insert ignore count",
        }
    );
    failures += expect_statement_ok(
        database,
        "TRUNCATE ignore_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate ignore for row constructors"
    );
    failures += expect_statement_ok(
        database,
        "INSERT IGNORE INTO ignore_t VALUES ROW(), ROW()",
        (struct expected_statement){.affected_rows = 2, .warning_count = 3U},
        "row constructor empty values insert ignore"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = ignore_warnings,
            .column_count = 3U,
            .row_count = 3U,
            .context = "row constructor empty values insert ignore warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM ignore_t",
            .values = default_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor empty values insert ignore count",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE replace_t(id INT NOT NULL DEFAULT 10, s VARCHAR(5) NOT NULL DEFAULT 'x')",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create empty values replace table"
    );
    failures += expect_statement_ok(
        database,
        "REPLACE INTO replace_t () VALUES ()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "explicit empty values replace"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, s FROM replace_t",
            .values = replace_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "replace empty values row",
        }
    );
    failures += expect_statement_ok(
        database,
        "REPLACE INTO replace_t VALUES ()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "omitted empty values replace"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM replace_t",
            .values = replace_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "replace empty values count",
        }
    );
    failures += expect_statement_ok(
        database,
        "REPLACE INTO replace_t () VALUES (), ()",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U},
        "multi empty values replace"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM replace_t",
            .values = replace_multi_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "multi replace empty values count",
        }
    );
    failures += expect_statement_ok(
        database,
        "TRUNCATE replace_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate replace for value empty values"
    );
    failures += expect_statement_ok(
        database,
        "REPLACE INTO replace_t VALUE ()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "replace value empty values"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, s FROM replace_t",
            .values = replace_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "replace value empty row",
        }
    );
    failures += expect_statement_ok(
        database,
        "TRUNCATE replace_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate replace for row constructors"
    );
    failures += expect_statement_ok(
        database,
        "REPLACE INTO replace_t VALUES ROW(), ROW()",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U},
        "replace row constructor empty values"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM replace_t",
            .values = default_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "replace row constructor empty values count",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE duplicate_t(id INT NOT NULL DEFAULT 1 PRIMARY KEY, "
        "v INT NOT NULL DEFAULT 10)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create empty values duplicate table"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO duplicate_t () VALUES ()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "insert duplicate seed empty values"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO duplicate_t () VALUES () ON DUPLICATE KEY UPDATE v = 20",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U},
        "empty values duplicate literal update"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v FROM duplicate_t",
            .values = duplicate_value_twenty,
            .column_count = 1U,
            .row_count = 1U,
            .context = "empty values duplicate literal row",
        }
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO duplicate_t VALUES () ON DUPLICATE KEY UPDATE v = DEFAULT",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U},
        "empty values duplicate default update"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO duplicate_t () VALUES () ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 1U},
        "empty values duplicate values reference"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v FROM duplicate_t",
            .values = duplicate_value_ten,
            .column_count = 1U,
            .row_count = 1U,
            .context = "empty values duplicate default row",
        }
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO duplicate_t () VALUES (), () ON DUPLICATE KEY UPDATE v = 20",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U},
        "multi empty values duplicate update"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v FROM duplicate_t",
            .values = duplicate_value_twenty,
            .column_count = 1U,
            .row_count = 1U,
            .context = "multi empty values duplicate row",
        }
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO duplicate_t VALUE () ON DUPLICATE KEY UPDATE v = DEFAULT",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U},
        "value empty duplicate default update"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO duplicate_t VALUES ROW() ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 1U},
        "row constructor duplicate values reference"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v FROM duplicate_t",
            .values = duplicate_value_ten,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row constructor duplicate values reference row",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE no_default_t(id INT NOT NULL, n INT NULL)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create empty values no default table"
    );
    failures += execute_error(
        database,
        "INSERT INTO no_default_t () VALUES ()",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'id' doesn't have a default value",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO defaults_t () VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_column_count,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO defaults_t VALUES (), (1, NULL, 'y', 2.00, '2024-01-03')",
        (struct expected_sql_error){
            .code = mysql_error_column_count,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 2",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO defaults_t VALUES (1, NULL, 'y', 2.00, '2024-01-03'), ()",
        (struct expected_sql_error){
            .code = mysql_error_column_count,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 2",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO defaults_t () SELECT id FROM defaults_t",
        (struct expected_sql_error){
            .code = mysql_error_column_count,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO defaults_t VALUE ROW()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO defaults_t VALUES ROW(), ()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO defaults_t VALUES (), ROW()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO defaults_t (id) VALUES ROW(1), ROW(2, 3)",
        (struct expected_sql_error){
            .code = mysql_error_column_count,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 2",
        }
    );

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read empty values preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "empty values preamble preserved"
    );
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen empty values file");
    failures += expect_statement_ok(
        database,
        "USE app",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "use empty values reopened schema"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM ai_t ORDER BY id",
            .values = auto_reopened_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "empty values reopened rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM insert_row_syntax_t ORDER BY id",
            .values = insert_value_row_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "value row syntax reopened rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_dml_default_success_persistence_and_rename(void) {
    static const char *const default_rows[] = {
        "1",
        "5",
        "-6",
        "4294967295",
        "8",
        "-9223372036854775808",
        "9223372036854775807",
        "9",
        "9",
        "9",
        "2",
        "5",
        "-6",
        "4294967295",
        "8",
        "-9223372036854775808",
        "9223372036854775807",
        NULL,
        "7",
        "2",
        "3",
        "5",
        "-6",
        "4294967295",
        "8",
        "-9223372036854775808",
        "9223372036854775807",
        NULL,
        "7",
        "3",
        "100",
        "5",
        "-6",
        "4294967295",
        "8",
        "-9223372036854775808",
        "9223372036854775807",
        NULL,
        "7",
        "9",
    };
    static const char *const ordered_rows[] = {"1", "5", "2", "4", "3", "5"};
    static const char *const renamed_row[] = {"4", "5"};
    static const char *const reopened_row[] = {"1", "5", "1"};
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += seed_schema(database);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE t("
        "id INT NOT NULL DEFAULT 100, i INT DEFAULT 5, ii INTEGER DEFAULT -6, "
        "iu INT UNSIGNED DEFAULT 4294967295, integeru INTEGER UNSIGNED DEFAULT 8, "
        "b BIGINT DEFAULT -9223372036854775808, "
        "bu BIGINT UNSIGNED DEFAULT 9223372036854775807, "
        "n INT NULL DEFAULT NULL, nn INT NOT NULL DEFAULT 7, plain INT NULL, "
        "nod INT NOT NULL)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create default table"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO t(id, i, n, nn, nod) VALUES(DEFAULT, DEFAULT, DEFAULT, DEFAULT, 9)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "insert values default"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO t SET id = 1, i = 9, n = 9, nn = 9, nod = 9",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "insert set explicit"
    );
    failures += expect_statement_ok(
        database,
        "REPLACE INTO t(id, i, ii, iu, integeru, b, bu, n, nn, nod) "
        "VALUES(2, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, 2)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "replace values default"
    );
    failures += expect_statement_ok(
        database,
        "REPLACE INTO t SET id = 3, i = DEFAULT, n = DEFAULT, nn = DEFAULT, nod = 3",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "replace set default"
    );
    failures += expect_statement_ok(
        database,
        "UPDATE t SET i = DEFAULT WHERE id IN (1, 2, 3, 100)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "update default changed rows"
    );
    failures += expect_statement_ok(
        database,
        "UPDATE t SET i = DEFAULT WHERE id IN (1, 2, 3, 100)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "update default no-op"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, ii, iu, integeru, b, bu, n, nn, nod FROM t ORDER BY id",
            .values = default_rows,
            .column_count = dml_default_projection_column_count,
            .row_count = 4U,
            .context = "DML DEFAULT rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE ordered(id INT NOT NULL, i INT DEFAULT 5, n INT NULL, "
        "nn INT NOT NULL DEFAULT 1)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create ordered table"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO ordered VALUES(1, 3, NULL, 1), (2, 4, 4, 2), (3, 9, NULL, 3)",
        (struct expected_statement){.affected_rows = 3, .warning_count = 0U},
        "seed ordered table"
    );
    failures += expect_statement_ok(
        database,
        "UPDATE ordered SET i = DEFAULT ORDER BY n LIMIT 2",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U},
        "ordered limited update default"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i FROM ordered ORDER BY id",
            .values = ordered_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "ordered limited update default rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "ALTER TABLE t RENAME TO renamed_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "rename"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO renamed_t(id, i, nn, nod) VALUES(4, 9, 9, 4)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "insert after rename"
    );
    failures += expect_statement_ok(
        database,
        "UPDATE renamed_t SET i = DEFAULT WHERE id = 4",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "update after rename"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i FROM renamed_t WHERE id = 4",
            .values = renamed_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "renamed update default row",
        }
    );
    failures += expect_statement_ok(
        database,
        "DROP TABLE renamed_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "drop renamed table"
    );
    failures += execute_error(
        database,
        "UPDATE renamed_t SET i = DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_unknown_table,
            .sqlstate = "42S02",
            .message_part = "renamed_t",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "DML DEFAULT preserves preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += expect_statement_ok(
        database,
        "USE app",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "use reopened schema"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, nn FROM ordered WHERE id = 1",
            .values = reopened_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "reopened ordered row",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_dml_default_diagnostics_and_ignore_warnings(void) {
    static const char *const ignore_warnings[] = {
        "Warning",
        "1364",
        "Field 'id' doesn't have a default value",
        "Warning",
        "1364",
        "Field 'nn' doesn't have a default value",
    };
    static const char *const ignore_row[] = {"0", "7", NULL, "0"};
    static const char *const dropped_ignore_rows[] = {"1", NULL, "2", NULL};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += seed_schema(database);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE strict_t(id INT NOT NULL, d INT DEFAULT 7, n INT NULL, nn INT NOT NULL)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create strict diagnostics table"
    );
    failures += execute_error(
        database,
        "INSERT INTO strict_t(id, d, n, nn) VALUES(DEFAULT, DEFAULT, DEFAULT, DEFAULT)",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'id' doesn't have a default value",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO strict_t(id, d, n, nn) VALUES(DEFAULT, DEFAULT, DEFAULT, DEFAULT)",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'id' doesn't have a default value",
        }
    );
    failures += expect_statement_ok(
        database,
        "INSERT IGNORE INTO strict_t(id, d, n, nn) VALUES(DEFAULT, DEFAULT, DEFAULT, DEFAULT)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 2U},
        "insert ignore default warnings"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = ignore_warnings,
            .column_count = 3U,
            .row_count = 2U,
            .context = "insert ignore default warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, n, nn FROM strict_t",
            .values = ignore_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "insert ignore default adjusted row",
        }
    );
    failures += execute_error(
        database,
        "UPDATE strict_t SET nn = DEFAULT WHERE id = 0",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'nn' doesn't have a default value",
        }
    );
    failures += expect_statement_ok(
        database,
        "UPDATE strict_t SET nn = DEFAULT WHERE id = 999",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "update default no match skips no default"
    );
    failures += expect_statement_ok(
        database,
        "UPDATE strict_t SET nn = DEFAULT LIMIT 0",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "update default limit zero skips no default"
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE drop_t(id INT NOT NULL, n INT NULL)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create dropped default table"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE drop_t ALTER n DROP DEFAULT",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "drop default"
    );
    failures += execute_error(
        database,
        "INSERT INTO drop_t(id, n) VALUES(1, DEFAULT)",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'n' doesn't have a default value",
        }
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO drop_t(id, n) VALUES(2, 9)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "seed drop table"
    );
    failures += execute_error(
        database,
        "UPDATE drop_t SET n = DEFAULT WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'n' doesn't have a default value",
        }
    );
    failures += expect_statement_ok(
        database,
        "TRUNCATE TABLE drop_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "clear drop table"
    );
    failures += expect_statement_ok(
        database,
        "INSERT IGNORE INTO drop_t(id, n) VALUES(1, DEFAULT), (2, DEFAULT)",
        (struct expected_statement){.affected_rows = 2, .warning_count = 2U},
        "insert ignore dropped default warnings"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n FROM drop_t WHERE id IN (1, 2) ORDER BY id",
            .values = dropped_ignore_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "dropped default ignore rows",
        }
    );

    failures += execute_error(
        database,
        "INSERT INTO strict_t(missing) VALUES(DEFAULT)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE strict_t SET missing = DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing'",
        }
    );
    failures += expect_statement_ok(
        database,
        "UPDATE strict_t SET d = DEFAULT(d)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "update default function no-op"
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_dml_default_independent_handles(void) {
    static const char *const first_rows[] = {"1", "10", "4", "10"};
    static const char *const second_rows[] = {"2", "20", "4", "20"};
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += seed_schema(first);
    failures += seed_schema(second);
    failures += expect_statement_ok(
        first,
        "CREATE TABLE t(id INT NOT NULL DEFAULT 1, i INT DEFAULT 10, nn INT NOT NULL DEFAULT 3)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "first defaults table"
    );
    failures += expect_statement_ok(
        second,
        "CREATE TABLE t(id INT NOT NULL DEFAULT 2, i INT DEFAULT 20, nn INT NOT NULL DEFAULT 6)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "second defaults table"
    );
    failures += expect_statement_ok(
        first,
        "INSERT INTO t () VALUES ()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "first empty insert default"
    );
    failures += expect_statement_ok(
        second,
        "INSERT INTO t () VALUES ()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "second empty insert default"
    );
    failures += expect_statement_ok(
        first,
        "INSERT INTO t VALUES(4, 40, 4)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "first explicit row"
    );
    failures += expect_statement_ok(
        second,
        "INSERT INTO t VALUES(4, 40, 4)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "second explicit row"
    );
    failures += expect_statement_ok(
        first,
        "UPDATE t SET i = DEFAULT WHERE id = 4",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "first update default"
    );
    failures += expect_statement_ok(
        second,
        "UPDATE t SET i = DEFAULT WHERE id = 4",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "second update default"
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, i FROM t ORDER BY id",
            .values = first_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "first handle default rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, i FROM t ORDER BY id",
            .values = second_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "second handle default rows",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);

    return failures;
}

static int seed_schema(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(
        database,
        "CREATE DATABASE app",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "create schema"
    );
    failures += expect_statement_ok(
        database,
        "USE app",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "use schema"
    );

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s' failed: %d %s %s\n",
            sql,
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
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);

    return failures;
}

static int expect_statement_ok(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, context);
    failures += expect_size(mylite_result_row_count(result), 0U, context);
    failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, context);
    failures += expect_size(mylite_result_warning_count(result), expected.warning_count, context);
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
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
        return expect_true(actual == NULL, context);
    }

    return expect_text(actual, expected, context);
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
        "%s/mylite_dml_default_keyword_values_%d_%s.mylite",
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
        fprintf(stderr, "%s: expected true\n", context);
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

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte buffers differ\n", context);
        return 1;
    }

    return 0;
}
