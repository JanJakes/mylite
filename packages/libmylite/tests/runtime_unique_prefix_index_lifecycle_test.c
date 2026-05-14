#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "runtime/mylite_diagnostics.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

#include <stdarg.h>

enum {
    test_path_capacity = 1024,
    long_prefix_length = 191,
    duplicate_key_display_length = 64,
    long_prefix_value_capacity = 256,
    long_prefix_sql_capacity = 1536,
    show_columns_field_count = 6,
    show_index_field_count = 15,
    statistics_probe_field_count = 6,
    unique_prefix_dml_row_count = 6,
    mysql_error_parse = 1064,
    mysql_error_duplicate_key = 1062,
    mysql_error_key_too_long = 1071,
    mysql_error_incorrect_prefix_key = 1089,
    mysql_error_blob_key_without_length = 1170,
    mysql_error_key_part_length_cannot_be_zero = 1391,
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

struct repeated_text_request {
    char *destination;
    size_t destination_size;
    char repeated;
    size_t repeat_count;
    const char *suffix;
};

static int test_unique_prefix_metadata_dml_and_persistence(void);
static int test_long_unique_prefix_dml(void);
static int test_unique_prefix_diagnostics(void);
static int test_unique_prefix_independent_handles(void);
static int create_schema(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_physical_index_count(
    mylite_db *database,
    int expected_count,
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
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int format_test_sql(char *destination, size_t destination_size, const char *format, ...);
static int fill_repeated_text(struct repeated_text_request request);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_unique_prefix_metadata_dml_and_persistence();
    failures += test_long_unique_prefix_dml();
    failures += test_unique_prefix_diagnostics();
    failures += test_unique_prefix_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_unique_prefix_metadata_dml_and_persistence(void) {
    static const char *const show_columns_rows[] = {
        "id", "int",     "YES", "",    NULL, "", "v",    "varchar(20)", "NO",  "UNI", NULL, "",
        "c",  "char(5)", "YES", "UNI", NULL, "", "body", "text",        "YES", "UNI", NULL, "",
    };
    static const char *const show_index_rows[] = {
        "prefix_unique",
        "0",
        "u_v",
        "1",
        "v",
        "A",
        "0",
        "3",
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
        "prefix_unique",
        "0",
        "u_c",
        "1",
        "c",
        "A",
        "0",
        "2",
        NULL,
        "YES",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
        "prefix_unique",
        "0",
        "u_body",
        "1",
        "body",
        "A",
        "0",
        "4",
        NULL,
        "YES",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const statistics_rows[] = {
        "u_body",
        "0",
        "1",
        "body",
        "4",
        "YES",
        "u_c",
        "0",
        "1",
        "c",
        "2",
        "YES",
        "u_v",
        "0",
        "1",
        "v",
        "3",
        "",
    };
    static const char *const show_create_rows[] = {
        "prefix_unique",
        "CREATE TABLE `prefix_unique` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `v` varchar(20) NOT NULL,\n"
        "  `c` char(5) DEFAULT NULL,\n"
        "  `body` text,\n"
        "  UNIQUE KEY `u_v` (`v`(3)),\n"
        "  UNIQUE KEY `u_c` (`c`(2)),\n"
        "  UNIQUE KEY `u_body` (`body`(4))\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const post_dml_rows[] = {
        "1", "abcdef", "abxxx", "payload-one", "2", "zzzzzz", NULL, "data-two",
        "4", "mnopqr", NULL,    NULL,          "5", "stuvwx", NULL, NULL,
        "7", "rstuvw", "ghijk", "four-data",   "8", "lmnopq", NULL, NULL,
    };
    static const char *const odku_rows[] = {"1", "abcdef", "new"};
    static const char *const add_create_prefix_count_rows[] = {"2"};
    static const char *const clone_prefix_count_rows[] = {"3"};
    static const char *const dropped_index_count_rows[] = {"0"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "metadata") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open unique prefix file");
    failures += create_schema(database);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE prefix_unique ("
        "id INT, v VARCHAR(20) NOT NULL, c CHAR(5), body TEXT, "
        "UNIQUE KEY u_v (v(3)), UNIQUE KEY u_c (c(2)), UNIQUE KEY u_body (body(4)))"
    );
    failures += expect_physical_index_count(database, 3, "unique prefix physical indexes");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM prefix_unique",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 4U,
            .context = "unique prefix SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM prefix_unique",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 3U,
            .context = "unique prefix SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE prefix_unique",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "unique prefix SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART, "
                   "NULLABLE FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'prefix_unique' ORDER BY INDEX_NAME",
            .values = statistics_rows,
            .column_count = statistics_probe_field_count,
            .row_count = 3U,
            .context = "unique prefix INFORMATION_SCHEMA.STATISTICS",
        }
    );

    failures += expect_dml_ok(
        database,
        "INSERT INTO prefix_unique VALUES "
        "(1,'abcdef','abxxx','payload-one'),"
        "(2,'zzzzzz','cdxxx','data-two')",
        2
    );
    failures += execute_error(
        database,
        "INSERT INTO prefix_unique VALUES (3,'abcxyz','efghi','data-three')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'abc' for key 'prefix_unique.u_v'",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO prefix_unique VALUES (3,'ghijkl','ABzzz','data-three')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'AB' for key 'prefix_unique.u_c'",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO prefix_unique VALUES (3,'ghijkl','efghi','payl-three')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'payl' for key 'prefix_unique.u_body'",
        }
    );
    failures +=
        expect_dml_ok(database, "INSERT INTO prefix_unique VALUES (4,'mnopqr',NULL,NULL)", 1);
    failures +=
        expect_dml_ok(database, "INSERT INTO prefix_unique VALUES (5,'stuvwx',NULL,NULL)", 1);
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO prefix_unique VALUES "
        "(6,'abczzz','efghi','data-three'),"
        "(7,'rstuvw','ghijk','four-data'),"
        "(8,'lmnopq',NULL,NULL)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_dml_ok(database, "UPDATE prefix_unique SET c = NULL WHERE id = 2", 1);
    failures += execute_error(
        database,
        "UPDATE prefix_unique SET v = 'zzzabc' WHERE id = 7",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'zzz' for key 'prefix_unique.u_v'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE prefix_unique SET body = 'data-five' WHERE id = 5",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'data' for key 'prefix_unique.u_body'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE prefix_unique SET c = 'GHabc' WHERE id = 8",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'GH' for key 'prefix_unique.u_c'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, c, body FROM prefix_unique ORDER BY id",
            .values = post_dml_rows,
            .column_count = 4U,
            .row_count = unique_prefix_dml_row_count,
            .context = "unique prefix DML state",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE add_create_prefix (id INT, v VARCHAR(20), body TEXT)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO add_create_prefix VALUES (1,'abcdef','payload-one'),(2,'zzzzzz','data-two')",
        2
    );
    failures +=
        expect_statement_ok(database, "ALTER TABLE add_create_prefix ADD UNIQUE KEY u_v (v(3))");
    failures +=
        expect_statement_ok(database, "CREATE UNIQUE INDEX u_body ON add_create_prefix (body(4))");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'add_create_prefix' "
                   "AND NON_UNIQUE = 0 AND SUB_PART IS NOT NULL",
            .values = add_create_prefix_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "ALTER and CREATE UNIQUE prefix metadata",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE odku_prefix (id INT, v VARCHAR(20), note VARCHAR(20), UNIQUE KEY u_v (v(3)))"
    );
    failures += expect_dml_ok(database, "INSERT INTO odku_prefix VALUES (1,'abcdef','old')", 1);
    failures += expect_dml_result(
        database,
        "INSERT INTO odku_prefix VALUES (2,'abcxyz','new') "
        "ON DUPLICATE KEY UPDATE note = VALUES(note)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, note FROM odku_prefix",
            .values = odku_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "unique prefix ODKU state",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE clone_prefix_unique LIKE prefix_unique");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'clone_prefix_unique' "
                   "AND SUB_PART IS NOT NULL",
            .values = clone_prefix_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE clones unique prefix metadata",
        }
    );
    failures += expect_statement_ok(database, "DROP INDEX u_c ON prefix_unique");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'prefix_unique' "
                   "AND INDEX_NAME = 'u_c'",
            .values = dropped_index_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "DROP INDEX removes unique prefix metadata",
        }
    );
    failures +=
        expect_statement_ok(database, "RENAME TABLE prefix_unique TO renamed_prefix_unique");
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after unique prefix lifecycle"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen unique prefix file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, c, body FROM renamed_prefix_unique ORDER BY id",
            .values = post_dml_rows,
            .column_count = 4U,
            .row_count = unique_prefix_dml_row_count,
            .context = "unique prefix rows persist after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_long_unique_prefix_dml(void) {
    static const char *const count_rows[] = {"1"};
    static const char *const odku_rows[] = {"2"};
    char duplicate_display[long_prefix_value_capacity];
    char first_value[long_prefix_value_capacity];
    char duplicate_value[long_prefix_value_capacity];
    char other_value[long_prefix_value_capacity];
    char duplicate_message[long_prefix_sql_capacity];
    char sql[long_prefix_sql_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += fill_repeated_text((struct repeated_text_request){
        .destination = duplicate_display,
        .destination_size = sizeof(duplicate_display),
        .repeated = 'a',
        .repeat_count = duplicate_key_display_length,
        .suffix = "",
    });
    failures += fill_repeated_text((struct repeated_text_request){
        .destination = first_value,
        .destination_size = sizeof(first_value),
        .repeated = 'a',
        .repeat_count = long_prefix_length,
        .suffix = "x",
    });
    failures += fill_repeated_text((struct repeated_text_request){
        .destination = duplicate_value,
        .destination_size = sizeof(duplicate_value),
        .repeated = 'a',
        .repeat_count = long_prefix_length,
        .suffix = "y",
    });
    failures += fill_repeated_text((struct repeated_text_request){
        .destination = other_value,
        .destination_size = sizeof(other_value),
        .repeated = 'b',
        .repeat_count = long_prefix_length,
        .suffix = "z",
    });
    if (failures != 0) {
        return failures;
    }

    failures += expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open long prefix db");
    failures += create_schema(database);

    failures += expect_statement_ok(
        database,
        "CREATE TABLE long_prefix_insert (v VARCHAR(255), UNIQUE KEY u_v (v(191)))"
    );
    if (format_test_sql(
            sql,
            sizeof(sql),
            "INSERT INTO long_prefix_insert VALUES ('%s')",
            first_value
        ) != 0) {
        ++failures;
    } else {
        failures += expect_dml_ok(database, sql, 1);
    }
    if (format_test_sql(
            sql,
            sizeof(sql),
            "INSERT INTO long_prefix_insert VALUES ('%s')",
            duplicate_value
        ) != 0 ||
        format_test_sql(
            duplicate_message,
            sizeof(duplicate_message),
            "Duplicate entry '%s' for key 'long_prefix_insert.u_v'",
            duplicate_display
        ) != 0) {
        ++failures;
    } else {
        failures += execute_error(
            database,
            sql,
            (struct expected_sql_error){
                .code = mysql_error_duplicate_key,
                .sqlstate = "23000",
                .message_part = duplicate_message,
            }
        );
    }

    failures += expect_statement_ok(
        database,
        "CREATE TABLE long_prefix_ignore (v VARCHAR(255), UNIQUE KEY u_v (v(191)))"
    );
    if (format_test_sql(
            sql,
            sizeof(sql),
            "INSERT INTO long_prefix_ignore VALUES ('%s')",
            first_value
        ) != 0) {
        ++failures;
    } else {
        failures += expect_dml_ok(database, sql, 1);
    }
    if (format_test_sql(
            sql,
            sizeof(sql),
            "INSERT IGNORE INTO long_prefix_ignore VALUES ('%s')",
            duplicate_value
        ) != 0) {
        ++failures;
    } else {
        failures += expect_dml_result(
            database,
            sql,
            (struct expected_dml_result){.affected_rows = 0, .warning_count = 1U}
        );
    }
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM long_prefix_ignore",
            .values = count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "long unique prefix INSERT IGNORE count",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE long_prefix_odku (v VARCHAR(255), n INT, UNIQUE KEY u_v (v(191)))"
    );
    if (format_test_sql(
            sql,
            sizeof(sql),
            "INSERT INTO long_prefix_odku VALUES ('%s', 1)",
            first_value
        ) != 0) {
        ++failures;
    } else {
        failures += expect_dml_ok(database, sql, 1);
    }
    if (format_test_sql(
            sql,
            sizeof(sql),
            "INSERT INTO long_prefix_odku VALUES ('%s', 2) "
            "ON DUPLICATE KEY UPDATE n = VALUES(n)",
            duplicate_value
        ) != 0) {
        ++failures;
    } else {
        failures += expect_dml_result(
            database,
            sql,
            (struct expected_dml_result){.affected_rows = 2, .warning_count = 1U}
        );
    }
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT n FROM long_prefix_odku",
            .values = odku_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "long unique prefix ODKU state",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE long_prefix_update (id INT, v VARCHAR(255), UNIQUE KEY u_v (v(191)))"
    );
    if (format_test_sql(
            sql,
            sizeof(sql),
            "INSERT INTO long_prefix_update VALUES (1, '%s'), (2, '%s')",
            first_value,
            other_value
        ) != 0) {
        ++failures;
    } else {
        failures += expect_dml_ok(database, sql, 2);
    }
    if (format_test_sql(
            sql,
            sizeof(sql),
            "UPDATE long_prefix_update SET v = '%s' WHERE id = 2",
            duplicate_value
        ) != 0 ||
        format_test_sql(
            duplicate_message,
            sizeof(duplicate_message),
            "Duplicate entry '%s' for key 'long_prefix_update.u_v'",
            duplicate_display
        ) != 0) {
        ++failures;
    } else {
        failures += execute_error(
            database,
            sql,
            (struct expected_sql_error){
                .code = mysql_error_duplicate_key,
                .sqlstate = "23000",
                .message_part = duplicate_message,
            }
        );
    }

    mylite_close(database);
    return failures;
}

static int test_unique_prefix_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open diagnostics db");
    failures += create_schema(database);

    failures += execute_error(
        database,
        "CREATE TABLE int_prefix (id INT, UNIQUE KEY u (id(4)))",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_prefix_key,
            .sqlstate = "HY000",
            .message_part = "Incorrect prefix key",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE zero_prefix (v VARCHAR(20), UNIQUE KEY u (v(0)))",
        (struct expected_sql_error){
            .code = mysql_error_key_part_length_cannot_be_zero,
            .sqlstate = "HY000",
            .message_part = "Key part 'v' length cannot be 0",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE oversized_varchar_prefix (v VARCHAR(10), UNIQUE KEY u (v(20)))",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_prefix_key,
            .sqlstate = "HY000",
            .message_part = "Incorrect prefix key",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE too_long_prefix (v VARCHAR(1000), UNIQUE KEY u (v(769)))",
        (struct expected_sql_error){
            .code = mysql_error_key_too_long,
            .sqlstate = "42000",
            .message_part = "Specified key was too long",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE text_unique (body TEXT, UNIQUE KEY u (body))",
        (struct expected_sql_error){
            .code = mysql_error_blob_key_without_length,
            .sqlstate = "42000",
            .message_part =
                "BLOB/TEXT column 'body' used in key specification without a key length",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE composite_prefix (a VARCHAR(10), b VARCHAR(10), UNIQUE KEY u (a(3), b(2)))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Composite unique prefix indexes are not supported",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE dup_alter (v VARCHAR(20))");
    failures += expect_dml_ok(database, "INSERT INTO dup_alter VALUES ('abcdef'),('abcxyz')", 2);
    failures += execute_error(
        database,
        "ALTER TABLE dup_alter ADD UNIQUE KEY u_v (v(3))",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'abc' for key 'dup_alter.u_v'",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE dup_create (v VARCHAR(20))");
    failures += expect_dml_ok(database, "INSERT INTO dup_create VALUES ('abcdef'),('abcxyz')", 2);
    failures += execute_error(
        database,
        "CREATE UNIQUE INDEX u_v ON dup_create (v(3))",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'abc' for key 'dup_create.u_v'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE non_ascii (v VARCHAR(20), UNIQUE KEY u_v (v(3)))"
    );
    failures += execute_error(
        database,
        "INSERT INTO non_ascii VALUES ('\xC3\xA9"
        "abc')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "non-ASCII string key values are not supported",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_unique_prefix_independent_handles(void) {
    static const char *const first_values[] = {"abcdef"};
    static const char *const second_values[] = {"zzzzzz"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += create_schema(first);
    failures += create_schema(second);
    failures +=
        expect_statement_ok(first, "CREATE TABLE t (id INT, v VARCHAR(20), UNIQUE KEY u_v (v(3)))");
    failures += expect_statement_ok(
        second,
        "CREATE TABLE t (id INT, v VARCHAR(20), UNIQUE KEY u_v (v(3)))"
    );
    failures += expect_dml_ok(first, "INSERT INTO t VALUES (1, 'abcdef')", 1);
    failures += expect_dml_ok(second, "INSERT INTO t VALUES (1, 'zzzzzz')", 1);
    failures += execute_error(
        first,
        "INSERT INTO t VALUES (2, 'abcxyz')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'abc' for key 't.u_v'",
        }
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT v FROM t WHERE id = 1",
            .values = first_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first unique prefix file state",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT v FROM t WHERE id = 1",
            .values = second_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second unique prefix file state",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int create_schema(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        const struct mylite_diagnostics *diagnostics = mylite_connection_diagnostics(database);

        fprintf(
            stderr,
            "expected success for [%s], got rc=%d err=%d state=%s message=%s\n",
            sql,
            rc,
            mylite_diagnostics_errcode(diagnostics),
            mylite_diagnostics_sqlstate(diagnostics),
            mylite_diagnostics_errmsg(diagnostics)
        );
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    const struct mylite_diagnostics *diagnostics = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "expected error for [%s]\n", sql);
        mylite_result_free(result);
        return 1;
    }
    diagnostics = mylite_connection_diagnostics(database);
    failures += expect_int(mylite_diagnostics_errcode(diagnostics), expected.code, sql);
    failures += expect_text(mylite_diagnostics_sqlstate(diagnostics), expected.sqlstate, sql);
    failures += expect_contains(mylite_diagnostics_errmsg(diagnostics), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    return expect_dml_result(
        database,
        sql,
        (struct expected_dml_result){.affected_rows = affected_rows, .warning_count = 0U}
    );
}

static int expect_dml_result(
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
    }
    for (size_t row = 0U; failures == 0 && row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_physical_index_count(
    mylite_db *database,
    int expected_count,
    const char *context
) {
    enum { sqlite_use_nul_terminated_string = -1 };

    sqlite3 *connection = mylite_connection_sqlite_for_test(database);
    sqlite3_stmt *statement = NULL;
    int actual_count = 0;
    int rc = SQLITE_OK;

    if (connection == NULL) {
        fprintf(stderr, "%s: missing SQLite test connection\n", context);
        return 1;
    }

    rc = sqlite3_prepare_v2(
        connection,
        "SELECT count(*) FROM sqlite_schema "
        "WHERE type = 'index' AND name GLOB '_mylite_user_index_*'",
        sqlite_use_nul_terminated_string,
        &statement,
        NULL
    );
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s: prepare physical index query failed: %d\n", context, rc);
        return 1;
    }

    rc = sqlite3_step(statement);
    if (rc == SQLITE_ROW) {
        actual_count = sqlite3_column_int(statement, 0);
        rc = SQLITE_OK;
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && rc == SQLITE_OK) {
        rc = SQLITE_ERROR;
    }
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s: physical index query failed: %d\n", context, rc);
        return 1;
    }

    return expect_int(actual_count, expected_count, context);
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
            fprintf(
                stderr,
                "%s: expected NULL at row %zu column %zu, got [%s]\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }

    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_unique_prefix_index_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
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
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fclose(file);
        return 1;
    }
    fclose(file);
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
        fprintf(stderr, "%s: expected %" PRId64 ", got %" PRId64 "\n", context, expected, actual);
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

static int format_test_sql(char *destination, size_t destination_size, const char *format, ...) {
    va_list args;
    int written = 0;

    if (destination == NULL || destination_size == 0U || format == NULL) {
        return 1;
    }
    va_start(args, format);
    written = vsnprintf(destination, destination_size, format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= destination_size) {
        fprintf(stderr, "failed to format test SQL\n");
        return 1;
    }

    return 0;
}

static int fill_repeated_text(struct repeated_text_request request) {
    size_t suffix_length = request.suffix == NULL ? 0U : strlen(request.suffix);

    if (request.destination == NULL || request.destination_size == 0U ||
        suffix_length >= request.destination_size ||
        request.repeat_count > request.destination_size - suffix_length - 1U) {
        return 1;
    }
    memset(request.destination, request.repeated, request.repeat_count);
    if (suffix_length != 0U) {
        memcpy(&request.destination[request.repeat_count], request.suffix, suffix_length);
    }
    request.destination[request.repeat_count + suffix_length] = '\0';

    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte range did not match\n", context);
        return 1;
    }
    return 0;
}
