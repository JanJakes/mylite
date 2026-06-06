#include <mylite/mylite.h>

#include "runtime_test_support.h"

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
    sql_capacity = 2048,
    mysql_error_parse = 1064,
    mysql_error_database_access_denied = 1044,
    mysql_error_no_database_selected = 1046,
    mysql_error_column_ambiguous = 1052,
    mysql_error_not_unique_table_alias = 1066,
    mysql_error_unknown_column = 1054,
    mysql_error_unknown_table_in_multi_delete = 1109,
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
    size_t row_count;
    size_t column_count;
    const char *context;
};

struct join_table_names {
    const char *left_name;
    const char *right_name;
};

static int test_joined_delete_success_persistence_and_table_lifecycle(void);
static int test_joined_delete_diagnostics(void);
static int test_joined_delete_foreign_keys_and_independent_handles(void);
static int seed_app_schema(mylite_db *database);
static int create_join_tables(mylite_db *database, struct join_table_names names);
static int create_fk_join_tables(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_delete_ok(mylite_db *database, const char *sql, int64_t affected_rows);
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

    failures += test_joined_delete_success_persistence_and_table_lifecycle();
    failures += test_joined_delete_diagnostics();
    failures += test_joined_delete_foreign_keys_and_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_joined_delete_success_persistence_and_table_lifecycle(void) {
    static const char *const ids_2_4[] = {"2", "4"};
    static const char *const ids_2_3_4[] = {"2", "3", "4"};
    static const char *const ids_1_3[] = {"1", "3"};
    static const char *const count_zero[] = {"0"};
    static const char *const remaining_transient_names[] = {
        "_transient_keep",
        "_transient_timeout_keep",
        "regular",
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += seed_app_schema(database);

    failures += create_join_tables(
        database,
        (struct join_table_names){.left_name = "left_a", .right_name = "right_a"}
    );
    failures += expect_delete_ok(
        database,
        "DELETE left_a FROM left_a JOIN right_a ON left_a.k = right_a.k",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM left_a ORDER BY id",
            .values = ids_2_4,
            .row_count = 2U,
            .column_count = 1U,
            .context = "joined delete duplicate matches count target rows once",
        }
    );

    failures += create_join_tables(
        database,
        (struct join_table_names){.left_name = "left_b", .right_name = "right_b"}
    );
    failures += expect_delete_ok(
        database,
        "DELETE FROM left_b USING left_b JOIN right_b ON left_b.k = right_b.k "
        "WHERE right_b.v > 900",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM left_b ORDER BY id",
            .values = ids_2_4,
            .row_count = 2U,
            .column_count = 1U,
            .context = "USING joined delete filters joined rows",
        }
    );

    failures += create_join_tables(
        database,
        (struct join_table_names){.left_name = "left_c", .right_name = "right_c"}
    );
    failures +=
        expect_delete_ok(database, "DELETE l FROM left_c AS l JOIN right_c AS r ON l.k = r.k", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM left_c ORDER BY id",
            .values = ids_2_4,
            .row_count = 2U,
            .column_count = 1U,
            .context = "alias target joined delete",
        }
    );

    failures += create_join_tables(
        database,
        (struct join_table_names){.left_name = "left_straight", .right_name = "right_straight"}
    );
    failures += expect_delete_ok(
        database,
        "DELETE l FROM left_straight AS l STRAIGHT_JOIN right_straight AS r "
        "ON l.k = r.k WHERE r.v > 900",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM left_straight ORDER BY id",
            .values = ids_2_4,
            .row_count = 2U,
            .column_count = 1U,
            .context = "straight joined delete reuses inner join path",
        }
    );

    failures += create_join_tables(
        database,
        (struct join_table_names){.left_name = "left_d", .right_name = "right_d"}
    );
    failures += expect_delete_ok(
        database,
        "DELETE left_d FROM left_d LEFT JOIN right_d ON left_d.k = right_d.k "
        "WHERE right_d.id IS NULL",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM left_d ORDER BY id",
            .values = ids_1_3,
            .row_count = 2U,
            .column_count = 1U,
            .context = "left joined delete removes unmatched target rows",
        }
    );

    failures += create_join_tables(
        database,
        (struct join_table_names){.left_name = "left_e", .right_name = "right_e"}
    );
    failures += expect_delete_ok(
        database,
        "DELETE left_e FROM left_e JOIN right_e WHERE right_e.id = 9",
        4
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM left_e",
            .values = count_zero,
            .row_count = 1U,
            .column_count = 1U,
            .context = "joined delete without ON uses joined source WHERE",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE wp_options (option_id INT, option_name VARCHAR(255), "
        "option_value LONGTEXT, autoload VARCHAR(20))"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO wp_options VALUES "
        "(1, '_transient_old', 'payload', 'no'), "
        "(2, '_transient_timeout_old', '1', 'no'), "
        "(3, '_transient_keep', 'payload', 'no'), "
        "(4, '_transient_timeout_keep', '9999999999', 'no'), "
        "(5, 'regular', 'value', 'yes')"
    );
    failures += expect_delete_ok(
        database,
        "DELETE a, b FROM wp_options a, wp_options b "
        "WHERE a.option_name LIKE '\\_transient\\_%' "
        "AND a.option_name NOT LIKE '\\_transient\\_timeout_%' "
        "AND b.option_name = CONCAT( '_transient_timeout_', SUBSTRING( a.option_name, 12 ) ) "
        "AND b.option_value < UNIX_TIMESTAMP()",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT option_name FROM wp_options ORDER BY option_id",
            .values = remaining_transient_names,
            .row_count = 3U,
            .column_count = 1U,
            .context = "multi-target comma joined delete removes expired transient pair",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE wp_options_literal (option_id INT, option_name VARCHAR(255), "
        "option_value LONGTEXT, autoload VARCHAR(20))"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO wp_options_literal VALUES "
        "(1, '_transient_old', 'payload', 'no'), "
        "(2, '_transient_timeout_old', '1', 'no'), "
        "(3, '_transient_keep', 'payload', 'no'), "
        "(4, '_transient_timeout_keep', '9999999999', 'no'), "
        "(5, 'regular', 'value', 'yes')"
    );
    failures += expect_delete_ok(
        database,
        "DELETE a, b FROM wp_options_literal a, wp_options_literal b "
        "WHERE a.option_name LIKE '\\_transient\\_%' "
        "AND a.option_name NOT LIKE '\\_transient\\_timeout_%' "
        "AND b.option_name = CONCAT( '_transient_timeout_', SUBSTRING( a.option_name, 12 ) ) "
        "AND b.option_value < 100",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT option_name FROM wp_options_literal ORDER BY option_id",
            .values = remaining_transient_names,
            .row_count = 3U,
            .column_count = 1U,
            .context = "multi-target comma joined delete compares text timeout to integer literal",
        }
    );

    failures += create_join_tables(
        database,
        (struct join_table_names){.left_name = "rename_left", .right_name = "rename_right"}
    );
    failures += expect_statement_ok(database, "RENAME TABLE rename_left TO renamed_left");
    failures += expect_delete_ok(
        database,
        "DELETE renamed_left FROM renamed_left JOIN rename_right ON renamed_left.k = "
        "rename_right.k "
        "WHERE rename_right.id = 9",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM renamed_left ORDER BY id",
            .values = ids_2_3_4,
            .row_count = 3U,
            .column_count = 1U,
            .context = "joined delete after table rename",
        }
    );

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "joined delete preserves MyLite preamble"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM renamed_left ORDER BY id",
            .values = ids_2_3_4,
            .row_count = 3U,
            .column_count = 1U,
            .context = "joined delete persists after reopen",
        }
    );
    failures += expect_delete_ok(
        database,
        "DELETE renamed_left FROM renamed_left JOIN rename_right ON renamed_left.k = "
        "rename_right.k",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM renamed_left ORDER BY id",
            .values = ids_2_4,
            .row_count = 2U,
            .column_count = 1U,
            .context = "joined delete after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_joined_delete_diagnostics(void) {
    mylite_db *database = NULL;
    mylite_db *missing_default_database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_test_open_temporary(&missing_default_database), MYLITE_OK, "open no db");
    failures += expect_statement_ok(missing_default_database, "CREATE DATABASE app");
    failures +=
        expect_statement_ok(missing_default_database, "CREATE TABLE app.lefts (id INT, k INT)");
    failures +=
        expect_statement_ok(missing_default_database, "CREATE TABLE app.rights (id INT, k INT)");
    failures += execute_error(
        missing_default_database,
        "DELETE lefts FROM app.lefts JOIN app.rights ON lefts.k = rights.k",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += expect_delete_ok(
        missing_default_database,
        "DELETE app.lefts FROM app.lefts JOIN app.rights ON lefts.k = rights.k",
        0
    );
    failures += execute_error(
        missing_default_database,
        "DELETE l FROM app.lefts AS l JOIN app.rights AS r ON l.k = r.k",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    mylite_close(missing_default_database);

    failures +=
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open diagnostics memory");
    failures += seed_app_schema(database);
    failures += create_join_tables(
        database,
        (struct join_table_names){.left_name = "lefts", .right_name = "rights"}
    );

    failures += execute_error(
        database,
        "DELETE information_schema.SCHEMATA FROM app.lefts JOIN app.rights ON lefts.k = rights.k",
        (struct expected_sql_error){
            .code = mysql_error_database_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied for user",
        }
    );

    failures += execute_error(
        database,
        "DELETE lefts FROM lefts AS l JOIN rights AS r ON l.k = r.k",
        (struct expected_sql_error){
            .code = mysql_error_unknown_table_in_multi_delete,
            .sqlstate = "42S02",
            .message_part = "Unknown table 'lefts' in MULTI DELETE",
        }
    );
    failures += execute_error(
        database,
        "DELETE missing FROM lefts JOIN rights ON lefts.k = rights.k",
        (struct expected_sql_error){
            .code = mysql_error_unknown_table_in_multi_delete,
            .sqlstate = "42S02",
            .message_part = "Unknown table 'missing' in MULTI DELETE",
        }
    );
    failures += execute_error(
        database,
        "DELETE l FROM lefts AS l JOIN rights AS l ON l.k = l.k",
        (struct expected_sql_error){
            .code = mysql_error_not_unique_table_alias,
            .sqlstate = "42000",
            .message_part = "Not unique table/alias: 'l'",
        }
    );
    failures += execute_error(
        database,
        "DELETE lefts FROM lefts JOIN missing ON lefts.k = missing.k",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "DELETE lefts FROM lefts JOIN rights ON lefts.nope = rights.k",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'lefts.nope' in 'on clause'",
        }
    );
    failures += execute_error(
        database,
        "DELETE lefts FROM lefts JOIN rights ON lefts.k = rights.k WHERE nope = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "DELETE lefts FROM lefts JOIN rights ON lefts.k = rights.k WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_column_ambiguous,
            .sqlstate = "23000",
            .message_part = "Column 'id' in where clause is ambiguous",
        }
    );
    failures += execute_error(
        database,
        "DELETE lefts FROM lefts JOIN rights ON lefts.k = rights.k WHERE lefts.id = rights.id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE column-to-column predicates are supported only inside EXISTS",
        }
    );
    failures += execute_error(
        database,
        "DELETE lefts FROM lefts JOIN rights ON lefts.k = rights.k ORDER BY lefts.id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "DELETE lefts FROM lefts JOIN rights ON lefts.k = rights.k LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE rowid_shadow (rowid INT, _rowid_ INT, oid INT)"
    );
    failures += expect_statement_ok(database, "CREATE TABLE rowid_other (id INT)");
    failures += expect_statement_ok(database, "INSERT INTO rowid_shadow VALUES (1,2,3)");
    failures += expect_statement_ok(database, "INSERT INTO rowid_other VALUES (1)");
    failures += execute_error(
        database,
        "DELETE rowid_shadow FROM rowid_shadow JOIN rowid_other",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "joined DELETE requires an unshadowed SQLite rowid alias",
        }
    );

    failures += expect_statement_ok(database, "USE information_schema");
    failures += execute_error(
        database,
        "DELETE l FROM app.lefts AS l JOIN app.rights AS r ON l.k = r.k",
        (struct expected_sql_error){
            .code = mysql_error_database_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied for user",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_joined_delete_foreign_keys_and_independent_handles(void) {
    static const char *const cascade_child_values[] = {"10", "1", "12", NULL, "13", "3"};
    static const char *const set_null_child_values[] = {
        "20",
        "1",
        "21",
        NULL,
        "22",
        NULL,
    };
    static const char *const first_ids[] = {"2", "4"};
    static const char *const second_ids[] = {"1", "2", "3", "4"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open FK memory");
    failures += seed_app_schema(database);
    failures += create_fk_join_tables(database);
    failures +=
        expect_delete_ok(database, "DELETE pdel FROM pdel JOIN marker ON pdel.id = marker.id", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, parent_id FROM cdel ORDER BY id",
            .values = cascade_child_values,
            .row_count = 3U,
            .column_count = 2U,
            .context = "joined delete preserves direct cascade action",
        }
    );
    failures +=
        expect_delete_ok(database, "DELETE pset FROM pset JOIN marker ON pset.id = marker.id", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, parent_id FROM cset ORDER BY id",
            .values = set_null_child_values,
            .row_count = 3U,
            .column_count = 2U,
            .context = "joined delete preserves direct set-null action",
        }
    );
    mylite_close(database);

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return failures + 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += seed_app_schema(first);
    failures += seed_app_schema(second);
    failures += create_join_tables(
        first,
        (struct join_table_names){.left_name = "lefts", .right_name = "rights"}
    );
    failures += create_join_tables(
        second,
        (struct join_table_names){.left_name = "lefts", .right_name = "rights"}
    );
    failures +=
        expect_delete_ok(first, "DELETE lefts FROM lefts JOIN rights ON lefts.k = rights.k", 2);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id FROM lefts ORDER BY id",
            .values = first_ids,
            .row_count = 2U,
            .column_count = 1U,
            .context = "first handle joined delete state",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id FROM lefts ORDER BY id",
            .values = second_ids,
            .row_count = 4U,
            .column_count = 1U,
            .context = "second handle remains independent",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int seed_app_schema(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "CREATE DATABASE app", &result);

    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    return failures;
}

static int create_join_tables(mylite_db *database, struct join_table_names names) {
    char sql[sql_capacity];
    int written =
        snprintf(sql, sizeof(sql), "CREATE TABLE %s (id INT, k INT, v INT)", names.left_name);
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "left join seed SQL is too long\n");
        return 1;
    }
    failures += expect_statement_ok(database, sql);

    written =
        snprintf(sql, sizeof(sql), "CREATE TABLE %s (id INT, k INT, v INT)", names.right_name);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "right join seed SQL is too long\n");
        return failures + 1;
    }
    failures += expect_statement_ok(database, sql);

    written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO %s VALUES (1,10,100),(2,20,200),(3,30,300),(4,NULL,400)",
        names.left_name
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "left insert seed SQL is too long\n");
        return failures + 1;
    }
    failures += expect_statement_ok(database, sql);

    written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO %s VALUES (9,10,900),(10,10,901),(11,30,902),(12,NULL,903)",
        names.right_name
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "right insert seed SQL is too long\n");
        return failures + 1;
    }
    failures += expect_statement_ok(database, sql);
    return failures;
}

static int create_fk_join_tables(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE TABLE marker (id INT PRIMARY KEY)");
    failures += expect_statement_ok(database, "INSERT INTO marker VALUES (2)");
    failures += expect_statement_ok(database, "CREATE TABLE pdel (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE cdel (id INT PRIMARY KEY, parent_id INT NULL, "
        "CONSTRAINT fk_cdel_parent FOREIGN KEY (parent_id) REFERENCES pdel (id) "
        "ON DELETE CASCADE)"
    );
    failures += expect_statement_ok(database, "INSERT INTO pdel VALUES (1),(2),(3)");
    failures +=
        expect_statement_ok(database, "INSERT INTO cdel VALUES (10,1),(11,2),(12,NULL),(13,3)");
    failures += expect_statement_ok(database, "CREATE TABLE pset (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE cset (id INT PRIMARY KEY, parent_id INT NULL, "
        "CONSTRAINT fk_cset_parent FOREIGN KEY (parent_id) REFERENCES pset (id) "
        "ON DELETE SET NULL)"
    );
    failures += expect_statement_ok(database, "INSERT INTO pset VALUES (1),(2),(3)");
    failures += expect_statement_ok(database, "INSERT INTO cset VALUES (20,1),(21,2),(22,NULL)");
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

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures != 0) {
        mylite_result_free(result);
        return failures;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_size(mylite_result_warning_count(result), 0U, "statement warning count");
    mylite_result_free(result);
    return failures;
}

static int expect_delete_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures != 0) {
        mylite_result_free(result);
        return failures;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, "delete column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "delete row count");
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, "delete affected");
    failures += expect_size(mylite_result_warning_count(result), 0U, "delete warning count");
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (failures != 0) {
        mylite_result_free(result);
        return failures;
    }
    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
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
        "%s/mylite_joined_delete_lifecycle_%d_%s.mylite",
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
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
