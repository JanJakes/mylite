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
    path_suffix_capacity = 16,
    show_columns_field_count = 6,
    show_index_field_count = 15,
    statistics_probe_field_count = 5,
    mysql_error_duplicate_key = 1062,
    mysql_error_parse = 1064,
    mysql_error_key_column_missing = 1072,
    mysql_error_key_too_long = 1071,
    mysql_error_invalid_use_of_null = 1138,
    mysql_error_primary_key_part_null = 1171,
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

static int test_create_time_composite_string_primary_key(void);
static int test_alter_add_composite_string_primary_key(void);
static int test_composite_string_primary_key_diagnostics(void);
static int test_composite_string_primary_key_independent_files(void);
static int create_schema(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_alter_primary_key_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
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

    failures += test_create_time_composite_string_primary_key();
    failures += test_alter_add_composite_string_primary_key();
    failures += test_composite_string_primary_key_diagnostics();
    failures += test_composite_string_primary_key_independent_files();

    return failures == 0 ? 0 : 1;
}

static int test_create_time_composite_string_primary_key(void) {
    static const char *const show_columns_rows[] = {
        "a",
        "varchar(10)",
        "NO",
        "PRI",
        NULL,
        "",
        "b",
        "varchar(10)",
        "NO",
        "PRI",
        NULL,
        "",
        "v",
        "int",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const show_index_rows[] = {
        "cspk", "0", "PRIMARY", "1", "a", "A", "0", NULL, NULL, "", "BTREE", "", "", "YES", NULL,
        "cspk", "0", "PRIMARY", "2", "b", "A", "0", NULL, NULL, "", "BTREE", "", "", "YES", NULL,
    };
    static const char *const show_create_rows[] = {
        "cspk",
        "CREATE TABLE `cspk` (\n"
        "  `a` varchar(10) NOT NULL,\n"
        "  `b` varchar(10) NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`a`,`b`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_columns_rows[] = {
        "a",
        "NO",
        "PRI",
        NULL,
        "b",
        "NO",
        "PRI",
        NULL,
        "v",
        "YES",
        "",
        NULL,
    };
    static const char *const information_schema_constraints_rows[] =
        {"PRIMARY", "PRIMARY KEY", "YES"};
    static const char *const information_schema_key_column_usage_rows[] = {
        "PRIMARY",
        "a",
        "1",
        NULL,
        "PRIMARY",
        "b",
        "2",
        NULL,
    };
    static const char *const information_schema_statistics_rows[] = {
        "PRIMARY",
        "0",
        "1",
        "a",
        "",
        "PRIMARY",
        "0",
        "2",
        "b",
        "",
    };
    static const char *const inserted_rows[] = {
        "a",
        "b",
        "1",
        "a",
        "b ",
        "2",
        "a ",
        "b",
        "3",
    };
    static const char *const post_ignore_rows[] = {
        "a",
        "b",
        "1",
        "a",
        "b ",
        "2",
        "a ",
        "b",
        "3",
        "z",
        "q",
        "4",
    };
    static const char *const clone_show_create_rows[] = {
        "clone_cspk",
        "CREATE TABLE `clone_cspk` (\n"
        "  `a` varchar(10) NOT NULL,\n"
        "  `b` varchar(10) NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`a`,`b`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const drop_show_create_rows[] = {
        "cspk",
        "CREATE TABLE `cspk` (\n"
        "  `a` varchar(10) NOT NULL,\n"
        "  `b` varchar(10) NOT NULL,\n"
        "  `v` int DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "create_composite_string_pk") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open create file");
    failures += create_schema(database);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE cspk (a VARCHAR(10), b VARCHAR(10), v INT, PRIMARY KEY (a,b))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM cspk",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 3U,
            .context = "composite string SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM cspk",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 2U,
            .context = "composite string SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE cspk",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "composite string SHOW CREATE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, IS_NULLABLE, COLUMN_KEY, COLUMN_DEFAULT "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'cspk' ORDER BY ORDINAL_POSITION",
            .values = information_schema_columns_rows,
            .column_count = 4U,
            .row_count = 3U,
            .context = "composite string I_S COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'cspk' ORDER BY CONSTRAINT_NAME",
            .values = information_schema_constraints_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "composite string I_S TABLE_CONSTRAINTS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION, "
                   "REFERENCED_TABLE_NAME FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'cspk' "
                   "ORDER BY ORDINAL_POSITION",
            .values = information_schema_key_column_usage_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "composite string I_S KEY_COLUMN_USAGE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, NULLABLE "
                   "FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'cspk' ORDER BY SEQ_IN_INDEX",
            .values = information_schema_statistics_rows,
            .column_count = statistics_probe_field_count,
            .row_count = 2U,
            .context = "composite string I_S STATISTICS",
        }
    );

    failures +=
        expect_dml_ok(database, "INSERT INTO cspk VALUES ('a','b',1),('a','b ',2),('a ','b',3)", 3);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM cspk ORDER BY v",
            .values = inserted_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "composite string inserted rows",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO cspk VALUES ('A','b',9)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'A-b' for key 'cspk.PRIMARY'",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO cspk VALUES ('A','b',9),('z','q',4)",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
    );
    failures += execute_error(
        database,
        "UPDATE cspk SET b = 'b' WHERE v = 2",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'a-b' for key 'cspk.PRIMARY'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM cspk ORDER BY v",
            .values = post_ignore_rows,
            .column_count = 3U,
            .row_count = 4U,
            .context = "composite string rows after duplicate paths",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO cspk VALUES ('\xC3\xA9','x',5)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "non-ASCII string key values are not supported",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE clone_cspk LIKE cspk");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE clone_cspk",
            .values = clone_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE clones composite string primary key",
        }
    );
    failures += expect_dml_result(
        database,
        "ALTER TABLE cspk DROP PRIMARY KEY",
        (struct expected_dml_result){.affected_rows = 4, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE cspk",
            .values = drop_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "DROP PRIMARY KEY after composite string key",
        }
    );
    failures += expect_bytes(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)) == 0 ? actual_preamble
                                                                              : NULL,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after composite string primary key"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen create file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE clone_cspk",
            .values = clone_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "reopened clone keeps composite string primary key",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM cspk ORDER BY v",
            .values = post_ignore_rows,
            .column_count = 3U,
            .row_count = 4U,
            .context = "reopened rows after composite string primary key",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alter_add_composite_string_primary_key(void) {
    static const char *const show_columns_rows[] = {
        "a",
        "varchar(10)",
        "NO",
        "PRI",
        NULL,
        "",
        "b",
        "int",
        "NO",
        "PRI",
        NULL,
        "",
        "v",
        "int",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const show_create_rows[] = {
        "alter_ok",
        "CREATE TABLE `alter_ok` (\n"
        "  `a` varchar(10) NOT NULL,\n"
        "  `b` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`a`,`b`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const rows_after_dml[] = {
        "a",
        "1",
        "1",
        "A",
        "2",
        "2",
        "a ",
        "1",
        "3",
        "z",
        "9",
        "9",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open alter database");
    failures += create_schema(database);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE alter_ok (a VARCHAR(10) NOT NULL, b INT NOT NULL, v INT)"
    );
    failures +=
        expect_dml_ok(database, "INSERT INTO alter_ok VALUES ('a',1,1),('A',2,2),('a ',1,3)", 3);
    failures += expect_alter_primary_key_ok(database, "ALTER TABLE alter_ok ADD PRIMARY KEY (a,b)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM alter_ok",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 3U,
            .context = "ALTER composite string SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE alter_ok",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "ALTER composite string SHOW CREATE",
        }
    );
    failures += execute_error(
        database,
        "UPDATE alter_ok SET b = 1 WHERE v = 2",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'A-1' for key 'alter_ok.PRIMARY'",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO alter_ok VALUES ('z',9,9)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM alter_ok ORDER BY v",
            .values = rows_after_dml,
            .column_count = 3U,
            .row_count = 4U,
            .context = "ALTER composite string rows",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_composite_string_primary_key_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open diagnostics database");
    failures += create_schema(database);

    failures += execute_error(
        database,
        "CREATE TABLE explicit_null_pk (a VARCHAR(10) NULL, b INT, PRIMARY KEY (a,b))",
        (struct expected_sql_error){
            .code = mysql_error_primary_key_part_null,
            .sqlstate = "42000",
            .message_part = "All parts of a PRIMARY KEY must be NOT NULL",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE default_null_pk (a VARCHAR(10) DEFAULT NULL, b INT, PRIMARY KEY (a,b))",
        (struct expected_sql_error){
            .code = mysql_error_primary_key_part_null,
            .sqlstate = "42000",
            .message_part = "All parts of a PRIMARY KEY must be NOT NULL",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE pk_len_ok (a VARCHAR(255), b VARCHAR(255), c VARCHAR(255), "
        "PRIMARY KEY (a,b,c))"
    );
    failures += execute_error(
        database,
        "CREATE TABLE pk_len_bad (a VARCHAR(255), b VARCHAR(255), c VARCHAR(255), "
        "d VARCHAR(255), PRIMARY KEY (a,b,c,d))",
        (struct expected_sql_error){
            .code = mysql_error_key_too_long,
            .sqlstate = "42000",
            .message_part = "Specified key was too long; max key length is 3072 bytes",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE prefix_pk (a VARCHAR(10), b INT, PRIMARY KEY (a(3), b))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '('",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE unknown_part (a VARCHAR(10), PRIMARY KEY (a,b))",
        (struct expected_sql_error){
            .code = mysql_error_key_column_missing,
            .sqlstate = "42000",
            .message_part = "Key column 'b' doesn't exist in table",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE alter_dup (a VARCHAR(10) NOT NULL, b INT NOT NULL)"
    );
    failures += expect_dml_ok(database, "INSERT INTO alter_dup VALUES ('a',1),('A',1)", 2);
    failures += execute_error(
        database,
        "ALTER TABLE alter_dup ADD PRIMARY KEY (a,b)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'a-1' for key 'alter_dup.PRIMARY'",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE alter_null (a VARCHAR(10), b INT)");
    failures += expect_dml_ok(database, "INSERT INTO alter_null VALUES (NULL,1)", 1);
    failures += execute_error(
        database,
        "ALTER TABLE alter_null ADD PRIMARY KEY (a,b)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_use_of_null,
            .sqlstate = "22004",
            .message_part = "Invalid use of NULL value",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE alter_len_bad (a VARCHAR(255) NOT NULL, b VARCHAR(255) NOT NULL, "
        "c VARCHAR(255) NOT NULL, d VARCHAR(255) NOT NULL)"
    );
    failures += execute_error(
        database,
        "ALTER TABLE alter_len_bad ADD PRIMARY KEY (a,b,c,d)",
        (struct expected_sql_error){
            .code = mysql_error_key_too_long,
            .sqlstate = "42000",
            .message_part = "Specified key was too long; max key length is 3072 bytes",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_composite_string_primary_key_independent_files(void) {
    static const char *const first_rows[] = {"alpha", "1", "10"};
    static const char *const second_rows[] = {"alpha", "1", "20"};
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
    failures += create_schema(first);
    failures += create_schema(second);
    failures += expect_statement_ok(
        first,
        "CREATE TABLE t (a VARCHAR(10), b INT, v INT, PRIMARY KEY (a,b))"
    );
    failures += expect_statement_ok(
        second,
        "CREATE TABLE t (a VARCHAR(10), b INT, v INT, PRIMARY KEY (a,b))"
    );
    failures += expect_dml_ok(first, "INSERT INTO t VALUES ('alpha',1,10)", 1);
    failures += expect_dml_ok(second, "INSERT INTO t VALUES ('alpha',1,20)", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM t",
            .values = first_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "first independent file rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM t",
            .values = second_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "second independent file rows",
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
        fprintf(
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

static int expect_alter_primary_key_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), 0, sql);
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
        "/tmp/mylite_composite_string_pk_%s_%d.mylite",
        name,
        current_process_id()
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
    char buffer[test_path_capacity + path_suffix_capacity];
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
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected,
            actual != NULL ? actual : "<NULL>"
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual != NULL ? actual : "<NULL>",
            needle
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
    if (actual == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }
    return 0;
}
