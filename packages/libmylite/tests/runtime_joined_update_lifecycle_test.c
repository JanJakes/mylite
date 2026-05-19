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
    sql_capacity = 2048,
    mysql_error_database_access_denied = 1044,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_column_ambiguous = 1052,
    mysql_error_not_unique_table_alias = 1066,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_unknown_column = 1054,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_wrong_usage = 1221,
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

static int test_joined_update_success_persistence_and_table_lifecycle(void);
static int test_joined_update_diagnostics(void);
static int test_joined_update_independent_handles(void);
static int seed_app_schema(mylite_db *database);
static int create_join_tables(mylite_db *database, struct join_table_names names);
static int create_fk_join_update_tables(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_update_ok(mylite_db *database, const char *sql, int64_t affected_rows);
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

    failures += test_joined_update_success_persistence_and_table_lifecycle();
    failures += test_joined_update_diagnostics();
    failures += test_joined_update_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_joined_update_success_persistence_and_table_lifecycle(void) {
    static const char *const duplicate_update_values[] = {
        "1",
        "7",
        "2",
        "200",
        "3",
        "7",
        "4",
        "400",
    };
    static const char *const filtered_update_values[] = {
        "1",
        "100",
        "2",
        "200",
        "3",
        "9",
        "4",
        "400",
    };
    static const char *const left_join_update_values[] = {
        "1",
        "100",
        "2",
        "8",
        "3",
        "300",
        "4",
        "8",
    };
    static const char *const cross_join_update_values[] = {
        "6",
        "6",
        "6",
        "6",
    };
    static const char *const alias_update_values[] = {
        "1",
        "11",
        "2",
        "200",
        "3",
        "300",
        "4",
        "400",
    };
    static const char *const unqualified_update_values[] = {
        "1",
        "13",
        "2",
        "200",
        "3",
        "300",
        "4",
        "400",
    };
    static const char *const right_update_values[] = {
        "9",
        "777",
        "10",
        "777",
        "11",
        "902",
        "12",
        "903",
    };
    static const char *const no_right_target_values[] = {
        "0",
    };
    static const char *const cascade_child_values[] = {
        "10",
        "1",
        "11",
        "20",
        "12",
        NULL,
        "13",
        "3",
    };
    static const char *const set_null_child_values[] = {
        "20",
        "1",
        "21",
        NULL,
        "22",
        NULL,
        "23",
        "3",
    };
    static const char *const rename_update_values[] = {
        "1",
        "100",
        "2",
        "200",
        "3",
        "55",
        "4",
        "400",
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
    failures += expect_update_ok(
        database,
        "UPDATE left_a JOIN right_a ON left_a.k = right_a.k SET left_a.v = 7",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM left_a ORDER BY id",
            .values = duplicate_update_values,
            .row_count = 4U,
            .column_count = 2U,
            .context = "joined update duplicate matches update target rows once",
        }
    );
    failures += expect_update_ok(
        database,
        "UPDATE left_a JOIN right_a ON left_a.k = right_a.k SET left_a.v = 7",
        0
    );

    failures += create_join_tables(
        database,
        (struct join_table_names){.left_name = "left_b", .right_name = "right_b"}
    );
    failures += expect_update_ok(
        database,
        "UPDATE left_b JOIN right_b ON left_b.k = right_b.k SET left_b.v = 9 "
        "WHERE right_b.w = 902",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM left_b ORDER BY id",
            .values = filtered_update_values,
            .row_count = 4U,
            .column_count = 2U,
            .context = "joined update filters on right source",
        }
    );

    failures += create_join_tables(
        database,
        (struct join_table_names){.left_name = "left_c", .right_name = "right_c"}
    );
    failures += expect_update_ok(
        database,
        "UPDATE left_c LEFT JOIN right_c ON left_c.k = right_c.k SET left_c.v = 8 "
        "WHERE right_c.id IS NULL",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM left_c ORDER BY id",
            .values = left_join_update_values,
            .row_count = 4U,
            .column_count = 2U,
            .context = "left joined update reaches unmatched target rows",
        }
    );

    failures += create_join_tables(
        database,
        (struct join_table_names){.left_name = "left_d", .right_name = "right_d"}
    );
    failures += expect_update_ok(
        database,
        "UPDATE left_d JOIN right_d SET left_d.v = 6 WHERE right_d.id = 9",
        4
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v FROM left_d ORDER BY id",
            .values = cross_join_update_values,
            .row_count = 4U,
            .column_count = 1U,
            .context = "inner joined update without ON uses joined source WHERE",
        }
    );

    failures += create_join_tables(
        database,
        (struct join_table_names){.left_name = "left_e", .right_name = "right_e"}
    );
    failures += expect_update_ok(
        database,
        "UPDATE left_e AS l JOIN right_e AS r ON l.k = r.k SET l.v = 11 WHERE r.w = 900",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM left_e ORDER BY id",
            .values = alias_update_values,
            .row_count = 4U,
            .column_count = 2U,
            .context = "alias-qualified joined update target",
        }
    );
    failures += create_join_tables(
        database,
        (struct join_table_names){.left_name = "left_unique", .right_name = "right_unique"}
    );
    failures += expect_update_ok(
        database,
        "UPDATE left_unique JOIN right_unique ON left_unique.k = right_unique.k SET v = 13 "
        "WHERE right_unique.w = 901",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM left_unique ORDER BY id",
            .values = unqualified_update_values,
            .row_count = 4U,
            .column_count = 2U,
            .context = "unqualified unique assignment target",
        }
    );

    failures += create_join_tables(
        database,
        (struct join_table_names){.left_name = "left_f", .right_name = "right_f"}
    );
    failures += expect_update_ok(
        database,
        "UPDATE left_f JOIN right_f ON left_f.k = right_f.k SET right_f.w = 777 "
        "WHERE left_f.id = 1",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, w FROM right_f ORDER BY id",
            .values = right_update_values,
            .row_count = 4U,
            .column_count = 2U,
            .context = "joined update can target right source",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE left_no_target (id INT, k INT)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE right_no_target (id INT, k INT, w INT NOT NULL)"
    );
    failures += expect_statement_ok(database, "INSERT INTO left_no_target VALUES (1, 123)");
    failures += expect_update_ok(
        database,
        "UPDATE left_no_target LEFT JOIN right_no_target "
        "ON left_no_target.k = right_no_target.k SET right_no_target.w = NULL",
        0
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM right_no_target",
            .values = no_right_target_values,
            .row_count = 1U,
            .column_count = 1U,
            .context = "right target left join skips NULL-extended rows",
        }
    );

    failures += create_fk_join_update_tables(database);
    failures += expect_update_ok(
        database,
        "UPDATE pcascade JOIN marker_update ON pcascade.id = marker_update.id "
        "SET pcascade.id = 20",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, parent_id FROM ccascade ORDER BY id",
            .values = cascade_child_values,
            .row_count = 4U,
            .column_count = 2U,
            .context = "joined update preserves direct cascade action",
        }
    );
    failures += expect_update_ok(
        database,
        "UPDATE psetnull JOIN marker_update ON psetnull.id = marker_update.id "
        "SET psetnull.id = 20",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, parent_id FROM csetnull ORDER BY id",
            .values = set_null_child_values,
            .row_count = 4U,
            .column_count = 2U,
            .context = "joined update preserves direct set-null action",
        }
    );

    failures += create_join_tables(
        database,
        (struct join_table_names){.left_name = "rename_left", .right_name = "rename_right"}
    );
    failures += expect_statement_ok(database, "RENAME TABLE rename_left TO renamed_left");
    failures += expect_update_ok(
        database,
        "UPDATE renamed_left JOIN rename_right ON renamed_left.k = rename_right.k "
        "SET renamed_left.v = 55 WHERE rename_right.id = 11",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM renamed_left ORDER BY id",
            .values = rename_update_values,
            .row_count = 4U,
            .column_count = 2U,
            .context = "joined update after table rename",
        }
    );

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "joined update preserves MyLite preamble"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM renamed_left ORDER BY id",
            .values = rename_update_values,
            .row_count = 4U,
            .column_count = 2U,
            .context = "joined update persists after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_joined_update_diagnostics(void) {
    static const char *const schema_qualified_values[] = {
        "1",
        "100",
        "2",
        "200",
        "3",
        "21",
        "4",
        "400",
    };
    static const char *const schema_qualified_assignment_values[] = {
        "1",
        "22",
        "2",
        "200",
        "3",
        "21",
        "4",
        "400",
    };
    mylite_db *database = NULL;
    mylite_db *missing_default_database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open(":memory:", &missing_default_database), MYLITE_OK, "open no db");
    failures += expect_statement_ok(missing_default_database, "CREATE DATABASE app");
    failures += create_join_tables(
        missing_default_database,
        (struct join_table_names){.left_name = "app.lefts", .right_name = "app.rights"}
    );
    failures += expect_update_ok(
        missing_default_database,
        "UPDATE app.lefts AS l JOIN app.rights AS r ON l.k = r.k SET l.v = 21 "
        "WHERE r.id = 11",
        1
    );
    failures += expect_query_values(
        missing_default_database,
        (struct expected_query){
            .sql = "SELECT id, v FROM app.lefts ORDER BY id",
            .values = schema_qualified_values,
            .row_count = 4U,
            .column_count = 2U,
            .context = "schema-qualified joined update without default schema",
        }
    );
    failures += expect_update_ok(
        missing_default_database,
        "UPDATE app.lefts JOIN app.rights ON lefts.k = rights.k SET app.lefts.v = 22 "
        "WHERE app.rights.id = 9",
        1
    );
    failures += expect_query_values(
        missing_default_database,
        (struct expected_query){
            .sql = "SELECT id, v FROM app.lefts ORDER BY id",
            .values = schema_qualified_assignment_values,
            .row_count = 4U,
            .column_count = 2U,
            .context = "schema-table-qualified joined update assignment target",
        }
    );
    failures += execute_error(
        missing_default_database,
        "UPDATE lefts JOIN app.rights ON lefts.k = rights.k SET lefts.v = 1",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    mylite_close(missing_default_database);

    failures +=
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open diagnostics memory");
    failures += seed_app_schema(database);
    failures += create_join_tables(
        database,
        (struct join_table_names){.left_name = "lefts", .right_name = "rights"}
    );

    failures += execute_error(
        database,
        "UPDATE information_schema.SCHEMATA JOIN lefts "
        "SET information_schema.SCHEMATA.SCHEMA_NAME = 'x' WHERE lefts.id = 1",
        (struct expected_sql_error){
            .code = mysql_error_database_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied for user",
        }
    );
    failures += execute_error(
        database,
        "UPDATE _mylite_private.lefts JOIN rights SET rights.w = 1",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE _mylite_private JOIN rights SET rights.w = 1",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE lefts JOIN missing ON lefts.k = missing.k SET lefts.v = 1",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "UPDATE lefts AS l JOIN rights AS l ON l.k = l.k SET l.v = 1",
        (struct expected_sql_error){
            .code = mysql_error_not_unique_table_alias,
            .sqlstate = "42000",
            .message_part = "Not unique table/alias: 'l'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE lefts JOIN rights ON lefts.nope = rights.k SET lefts.v = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'lefts.nope' in 'on clause'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE lefts JOIN rights ON lefts.k = rights.k SET nope = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE lefts JOIN rights ON lefts.k = rights.k SET id = 1",
        (struct expected_sql_error){
            .code = mysql_error_column_ambiguous,
            .sqlstate = "23000",
            .message_part = "Column 'id' in field list is ambiguous",
        }
    );
    failures += execute_error(
        database,
        "UPDATE lefts JOIN rights ON lefts.k = rights.k SET lefts.v = 1 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_column_ambiguous,
            .sqlstate = "23000",
            .message_part = "Column 'id' in where clause is ambiguous",
        }
    );
    failures += execute_error(
        database,
        "UPDATE lefts AS l JOIN rights AS r ON l.k = r.k SET lefts.v = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'lefts.v' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE lefts JOIN rights ON lefts.k = rights.k SET lefts.v = 1, rights.w = 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UPDATE multiple assignments",
        }
    );
    failures += execute_error(
        database,
        "UPDATE lefts JOIN rights ON lefts.k = rights.k SET lefts.v = (SELECT 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "joined UPDATE supports only constant assignment values",
        }
    );
    failures += execute_error(
        database,
        "UPDATE lefts JOIN rights ON lefts.k = rights.k SET lefts.v = 1 ORDER BY lefts.id",
        (struct expected_sql_error){
            .code = mysql_error_wrong_usage,
            .sqlstate = "HY000",
            .message_part = "Incorrect usage of UPDATE and ORDER BY",
        }
    );
    failures += execute_error(
        database,
        "UPDATE lefts JOIN rights ON lefts.k = rights.k SET lefts.v = 1 LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_wrong_usage,
            .sqlstate = "HY000",
            .message_part = "Incorrect usage of UPDATE and LIMIT",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE rowid_shadow (rowid INT, _rowid_ INT, oid INT, k INT)"
    );
    failures += expect_statement_ok(database, "CREATE TABLE rowid_other (id INT, k INT)");
    failures += expect_statement_ok(database, "INSERT INTO rowid_shadow VALUES (1,2,3,10)");
    failures += expect_statement_ok(database, "INSERT INTO rowid_other VALUES (1,10)");
    failures += execute_error(
        database,
        "UPDATE rowid_shadow JOIN rowid_other ON rowid_shadow.k = rowid_other.k "
        "SET rowid_shadow.k = 11",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "joined UPDATE requires an unshadowed SQLite rowid alias",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_joined_update_independent_handles(void) {
    static const char *const first_values[] = {
        "1",
        "44",
        "2",
        "200",
        "3",
        "44",
        "4",
        "400",
    };
    static const char *const second_values[] = {
        "1",
        "100",
        "2",
        "200",
        "3",
        "300",
        "4",
        "400",
    };
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
    failures += expect_update_ok(
        first,
        "UPDATE lefts JOIN rights ON lefts.k = rights.k SET lefts.v = 44",
        2
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, v FROM lefts ORDER BY id",
            .values = first_values,
            .row_count = 4U,
            .column_count = 2U,
            .context = "first handle joined update state",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, v FROM lefts ORDER BY id",
            .values = second_values,
            .row_count = 4U,
            .column_count = 2U,
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
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s (id INT, k INT, v INT, s INT)",
        names.left_name
    );
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "left join seed SQL is too long\n");
        return 1;
    }
    failures += expect_statement_ok(database, sql);

    written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s (id INT, k INT, w INT, s INT)",
        names.right_name
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "right join seed SQL is too long\n");
        return failures + 1;
    }
    failures += expect_statement_ok(database, sql);

    written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO %s VALUES (1,10,100,1),(2,20,200,2),(3,30,300,3),(4,NULL,400,4)",
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
        "INSERT INTO %s VALUES (9,10,900,9),(10,10,901,10),(11,30,902,11),(12,NULL,903,12)",
        names.right_name
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "right insert seed SQL is too long\n");
        return failures + 1;
    }
    failures += expect_statement_ok(database, sql);
    return failures;
}

static int create_fk_join_update_tables(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE TABLE marker_update (id INT PRIMARY KEY)");
    failures += expect_statement_ok(database, "INSERT INTO marker_update VALUES (2)");
    failures += expect_statement_ok(database, "CREATE TABLE pcascade (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ccascade (id INT PRIMARY KEY, parent_id INT NULL, "
        "CONSTRAINT fk_ccascade_parent FOREIGN KEY (parent_id) REFERENCES pcascade (id) "
        "ON UPDATE CASCADE)"
    );
    failures += expect_statement_ok(database, "INSERT INTO pcascade VALUES (1),(2),(3)");
    failures +=
        expect_statement_ok(database, "INSERT INTO ccascade VALUES (10,1),(11,2),(12,NULL),(13,3)");
    failures += expect_statement_ok(database, "CREATE TABLE psetnull (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE csetnull (id INT PRIMARY KEY, parent_id INT NULL, "
        "CONSTRAINT fk_csetnull_parent FOREIGN KEY (parent_id) REFERENCES psetnull (id) "
        "ON UPDATE SET NULL)"
    );
    failures += expect_statement_ok(database, "INSERT INTO psetnull VALUES (1),(2),(3)");
    failures +=
        expect_statement_ok(database, "INSERT INTO csetnull VALUES (20,1),(21,2),(22,NULL),(23,3)");
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

static int expect_update_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures != 0) {
        mylite_result_free(result);
        return failures;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, "update column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "update row count");
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, "update affected");
    failures += expect_size(mylite_result_warning_count(result), 0U, "update warning count");
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
        "%s/mylite_joined_update_lifecycle_%d_%s.mylite",
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
