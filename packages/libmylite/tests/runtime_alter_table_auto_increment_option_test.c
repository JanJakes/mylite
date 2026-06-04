#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
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
    related_file_suffix_capacity = 8,
    show_table_status_query_capacity = 128,
    type_sql_capacity = 256,
    show_table_status_field_count = 18,
    show_table_status_name_column = 0,
    show_table_status_auto_increment_column = 10,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_parse = 1064,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_does_not_exist = 1146,
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

struct expected_statement {
    int64_t affected_rows;
    size_t warning_count;
};

struct auto_increment_type_case {
    const char *table_name;
    const char *type_name;
};

static int test_alter_auto_increment_success_metadata_and_persistence(void);
static int test_alter_auto_increment_type_families_and_diagnostics(void);
static int test_alter_auto_increment_independent_handles(void);
static int expect_type_alter_auto_increment(
    mylite_db *database,
    struct auto_increment_type_case test_case
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_show_table_status_auto_increment(
    mylite_db *database,
    const char *table_name,
    const char *expected,
    const char *context
);
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
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_alter_auto_increment_success_metadata_and_persistence();
    failures += test_alter_auto_increment_type_families_and_diagnostics();
    failures += test_alter_auto_increment_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_alter_auto_increment_success_metadata_and_persistence(void) {
    static const char *const last_insert_id_one[] = {"1"};
    static const char *const alter_state[] = {"1", "0", "0"};
    static const char *const show_create_t_10[] = {
        "t",
        "CREATE TABLE `t` (\n"
        "  `id` int NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB AUTO_INCREMENT=10 DEFAULT CHARSET=utf8mb4 "
        "COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const show_create_t_11[] = {
        "t",
        "CREATE TABLE `t` (\n"
        "  `id` int NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB AUTO_INCREMENT=11 DEFAULT CHARSET=utf8mb4 "
        "COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_t_10[] = {"10"};
    static const char *const rows_after_upward_reset[] = {"1", "10", "10", "100"};
    static const char *const show_create_t_5[] = {
        "t",
        "CREATE TABLE `t` (\n"
        "  `id` int NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB AUTO_INCREMENT=5 DEFAULT CHARSET=utf8mb4 "
        "COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const show_create_t_6[] = {
        "t",
        "CREATE TABLE `t` (\n"
        "  `id` int NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB AUTO_INCREMENT=6 DEFAULT CHARSET=utf8mb4 "
        "COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const show_create_t_7[] = {
        "t",
        "CREATE TABLE `t` (\n"
        "  `id` int NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 "
        "COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const rows_after_lowering[] = {
        "1",
        "10",
        "5",
        "50",
        "6",
        "60",
    };
    static const char *const zero_show_create[] = {
        "zero_t",
        "CREATE TABLE `zero_t` (\n"
        "  `id` int NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const zero_rows[] = {"5", "5"};
    static const char *const no_auto_show_create[] = {
        "no_auto",
        "CREATE TABLE `no_auto` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const no_auto_information_schema[] = {NULL};
    static const char *const qualified_show_create[] = {
        "qualified",
        "CREATE TABLE `qualified` (\n"
        "  `id` int NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB AUTO_INCREMENT=9 DEFAULT CHARSET=utf8mb4 "
        "COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const like_show_create[] = {
        "like_t",
        "CREATE TABLE `like_t` (\n"
        "  `id` int NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const like_rows[] = {"1", "1"};
    static const char *const truncate_rows[] = {"1", "1"};
    static const char *const updated_auto_rows[] = {"9", "50", "10", "100"};
    static const char *const renamed_rows[] = {"12", "12"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    mylite_db *database = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures +=
        expect_statement_ok(database, "CREATE TABLE t (id INT AUTO_INCREMENT PRIMARY KEY, v INT)");
    failures += expect_statement_result(
        database,
        "INSERT INTO t(v) VALUES(10)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = last_insert_id_one,
            .column_count = 1U,
            .row_count = 1U,
            .context = "initial last insert id",
        }
    );

    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;
    failures += expect_statement_result(
        database,
        "ALTER TABLE t AUTO_INCREMENT=10",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    session = mylite_connection_session_state(database);
    failures += expect_uint64(
        session->catalog_generation,
        catalog_generation,
        "alter auto increment leaves catalog generation"
    );
    failures += expect_uint64(
        session->sqlite_schema_generation,
        sqlite_schema_generation,
        "alter auto increment leaves sqlite schema generation"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID(), ROW_COUNT(), @@warning_count",
            .values = alter_state,
            .column_count = 3U,
            .row_count = 1U,
            .context = "alter auto increment statement state",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE t",
            .values = show_create_t_10,
            .column_count = 2U,
            .row_count = 1U,
            .context = "show create after upward reset",
        }
    );
    failures += expect_show_table_status_auto_increment(database, "t", "10", "status after reset");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA='app' AND TABLE_NAME='t'",
            .values = information_schema_t_10,
            .column_count = 1U,
            .row_count = 1U,
            .context = "information schema after reset",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO t(v) VALUES(100)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE t",
            .values = show_create_t_11,
            .column_count = 2U,
            .row_count = 1U,
            .context = "show create after generated insert",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = rows_after_upward_reset,
            .column_count = 2U,
            .row_count = 2U,
            .context = "rows after upward reset",
        }
    );

    failures += expect_statement_result(
        database,
        "DELETE FROM t WHERE id = 10",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE t AUTO_INCREMENT 5",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE t",
            .values = show_create_t_5,
            .column_count = 2U,
            .row_count = 1U,
            .context = "show create after lowered reset",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO t(v) VALUES(50)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE t AUTO_INCREMENT=3",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE t",
            .values = show_create_t_6,
            .column_count = 2U,
            .row_count = 1U,
            .context = "show create constrained by max row",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO t(v) VALUES(60)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE t AUTO_INCREMENT=0",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE t",
            .values = show_create_t_7,
            .column_count = 2U,
            .row_count = 1U,
            .context = "zero reset constrained by max row",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = rows_after_lowering,
            .column_count = 2U,
            .row_count = 3U,
            .context = "rows after lowered reset",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE zero_t (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE zero_t AUTO_INCREMENT=0",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE zero_t",
            .values = zero_show_create,
            .column_count = 2U,
            .row_count = 1U,
            .context = "empty zero reset show create",
        }
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE zero_t AUTO_INCREMENT=5",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO zero_t(v) VALUES(5)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM zero_t",
            .values = zero_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "empty table reset rows",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE no_auto (id INT PRIMARY KEY, v INT)");
    failures += expect_statement_result(
        database,
        "ALTER TABLE no_auto AUTO_INCREMENT=5",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE no_auto",
            .values = no_auto_show_create,
            .column_count = 2U,
            .row_count = 1U,
            .context = "non-auto table show create unchanged",
        }
    );
    failures +=
        expect_show_table_status_auto_increment(database, "no_auto", NULL, "non-auto status");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA='app' AND TABLE_NAME='no_auto'",
            .values = no_auto_information_schema,
            .column_count = 1U,
            .row_count = 1U,
            .context = "non-auto information schema",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE qualified (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE app.qualified AUTO_INCREMENT 9",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE qualified",
            .values = qualified_show_create,
            .column_count = 2U,
            .row_count = 1U,
            .context = "schema-qualified reset show create",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE like_t LIKE t");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE like_t",
            .values = like_show_create,
            .column_count = 2U,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE resets option",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO like_t(v) VALUES(1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM like_t",
            .values = like_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE generated row",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE trunc_t (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE trunc_t AUTO_INCREMENT=8",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO trunc_t(v) VALUES(8)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_ok(database, "TRUNCATE TABLE trunc_t");
    failures += expect_statement_result(
        database,
        "INSERT INTO trunc_t(v) VALUES(1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM trunc_t",
            .values = truncate_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "truncate resets altered counter",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE upd_t (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE upd_t AUTO_INCREMENT=5",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO upd_t(v) VALUES(50)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "UPDATE upd_t SET id = 9 WHERE id = 5",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO upd_t(v) VALUES(100)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM upd_t ORDER BY id",
            .values = updated_auto_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "update advancement after altered counter",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE renamed_source (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE renamed_source AUTO_INCREMENT=12",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_ok(database, "RENAME TABLE renamed_source TO renamed_target");
    failures += expect_statement_result(
        database,
        "INSERT INTO renamed_target(v) VALUES(12)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM renamed_target",
            .values = renamed_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "renamed table preserves altered counter",
        }
    );
    failures += expect_statement_ok(database, "DROP TABLE renamed_target");

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "alter auto increment preserves MyLite preamble"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = rows_after_lowering,
            .column_count = 2U,
            .row_count = 3U,
            .context = "reopened altered rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE t",
            .values = show_create_t_7,
            .column_count = 2U,
            .row_count = 1U,
            .context = "reopened show create counter",
        }
    );
    failures += expect_show_table_status_auto_increment(database, "t", "10", "reopened status");

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_alter_auto_increment_type_families_and_diagnostics(void) {
    static const struct auto_increment_type_case type_cases[] = {
        {.table_name = "ai_int", .type_name = "INT"},
        {.table_name = "ai_integer", .type_name = "INTEGER"},
        {.table_name = "ai_bigint", .type_name = "BIGINT"},
        {.table_name = "ai_int_unsigned", .type_name = "INT UNSIGNED"},
        {.table_name = "ai_integer_unsigned", .type_name = "INTEGER UNSIGNED"},
        {.table_name = "ai_bigint_unsigned", .type_name = "BIGINT UNSIGNED"},
    };
    static const char *const tiny_status[] = {"256"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "types") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open type file");
    failures += execute_error(
        database,
        "ALTER TABLE t AUTO_INCREMENT=5",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private.t AUTO_INCREMENT=5",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_private'",
        }
    );
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.t AUTO_INCREMENT=5",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_table AUTO_INCREMENT=5",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private AUTO_INCREMENT=5",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_private'",
        }
    );

    for (size_t case_index = 0U; case_index < sizeof(type_cases) / sizeof(type_cases[0]);
         ++case_index) {
        failures += expect_type_alter_auto_increment(database, type_cases[case_index]);
    }

    failures += expect_statement_ok(
        database,
        "CREATE TABLE huge_option (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += execute_error(
        database,
        "ALTER TABLE huge_option AUTO_INCREMENT=9223372036854775808",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "AUTO_INCREMENT table option supports only signed 64-bit nonnegative values",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE tiny_over (id TINYINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE tiny_over AUTO_INCREMENT=256",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA='app' AND TABLE_NAME='tiny_over'",
            .values = tiny_status,
            .column_count = 1U,
            .row_count = 1U,
            .context = "out-of-column-range counter metadata",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_alter_auto_increment_independent_handles(void) {
    static const char *const first_rows[] = {"4", "40"};
    static const char *const second_rows[] = {"9", "90"};
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
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures +=
        expect_statement_ok(first, "CREATE TABLE t (id INT AUTO_INCREMENT PRIMARY KEY, v INT)");
    failures +=
        expect_statement_ok(second, "CREATE TABLE t (id INT AUTO_INCREMENT PRIMARY KEY, v INT)");
    failures += expect_statement_result(
        first,
        "ALTER TABLE t AUTO_INCREMENT=4",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        second,
        "ALTER TABLE t AUTO_INCREMENT=9",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        first,
        "INSERT INTO t(v) VALUES(40)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        second,
        "INSERT INTO t(v) VALUES(90)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, v FROM t",
            .values = first_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "first independent altered counter",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, v FROM t",
            .values = second_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second independent altered counter",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);

    return failures;
}

static int expect_type_alter_auto_increment(
    mylite_db *database,
    struct auto_increment_type_case test_case
) {
    char sql[type_sql_capacity];
    static const char *const rows[] = {"5", "50"};
    int failures = 0;
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s (id %s AUTO_INCREMENT PRIMARY KEY, v INT)",
        test_case.table_name,
        test_case.type_name
    );

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        (void)fprintf(stderr, "%s: failed to format CREATE TABLE\n", test_case.table_name);
        return 1;
    }
    failures += expect_statement_ok(database, sql);
    written = snprintf(sql, sizeof(sql), "ALTER TABLE %s AUTO_INCREMENT=5", test_case.table_name);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        (void)fprintf(stderr, "%s: failed to format ALTER TABLE\n", test_case.table_name);
        return 1;
    }
    failures += expect_statement_result(
        database,
        sql,
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    written = snprintf(sql, sizeof(sql), "INSERT INTO %s(v) VALUES(50)", test_case.table_name);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        (void)fprintf(stderr, "%s: failed to format INSERT\n", test_case.table_name);
        return 1;
    }
    failures += expect_statement_result(
        database,
        sql,
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    written = snprintf(sql, sizeof(sql), "SELECT id, v FROM %s", test_case.table_name);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        (void)fprintf(stderr, "%s: failed to format SELECT\n", test_case.table_name);
        return 1;
    }
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = sql,
            .values = rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = test_case.type_name,
        }
    );

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = MYLITE_OK;

    *out_result = NULL;
    rc = mylite_execute(database, sql, strlen(sql), out_result);
    if (rc != MYLITE_OK) {
        (void)fprintf(
            stderr,
            "%s: expected success, got %d %s %s\n",
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

    if (rc == MYLITE_OK) {
        (void)fprintf(stderr, "%s: expected error\n", sql);
        mylite_result_free(result);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result != NULL) {
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

    if (result != NULL) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                failures += expect_result_value(
                    result,
                    row,
                    column,
                    query.values[(row * query.column_count) + column],
                    query.context
                );
            }
        }
        failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
        failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_show_table_status_auto_increment(
    mylite_db *database,
    const char *table_name,
    const char *expected,
    const char *context
) {
    char sql[show_table_status_query_capacity];
    mylite_result *result = NULL;
    int written = snprintf(sql, sizeof(sql), "SHOW TABLE STATUS LIKE '%s'", table_name);
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        (void)fprintf(stderr, "%s: failed to format SHOW TABLE STATUS query\n", context);
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    if (result != NULL) {
        failures +=
            expect_size(mylite_result_column_count(result), show_table_status_field_count, context);
        failures += expect_size(mylite_result_row_count(result), 1U, context);
        failures +=
            expect_result_value(result, 0U, show_table_status_name_column, table_name, context);
        failures += expect_result_value(
            result,
            0U,
            show_table_status_auto_increment_column,
            expected,
            context
        );
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
    return expect_text_or_null(mylite_result_value_text(result, row, column), expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_alter_table_auto_increment_option_%ld_%s.mylite",
        (long)current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        (void)fprintf(stderr, "failed to build test path\n");
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
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + related_file_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        (void)fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        (void)fprintf(stderr, "%s: failed to seek file\n", path);
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    if (bytes_read != size) {
        (void)fprintf(stderr, "%s: expected %zu bytes, got %zu\n", path, size, bytes_read);
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    (void)fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    (void)fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    (void)fprintf(
        stderr,
        "%s: expected %llu, got %llu\n",
        context,
        (unsigned long long)expected,
        (unsigned long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    (void)fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    (void)fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    (void)fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "NULL" : actual,
        needle == NULL ? "NULL" : needle
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
    (void)fprintf(stderr, "%s: bytes differ\n", context);
    return 1;
}
