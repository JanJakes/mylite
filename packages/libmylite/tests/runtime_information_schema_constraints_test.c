#include <mylite/mylite.h>

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
    mysql_error_unknown_column = 1054,
    system_view_column_metadata_column_count = 13,
    key_column_usage_system_column_count = 12,
    table_constraints_system_column_count = 7,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

struct expected_sql_error {
    const char *sql;
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_information_schema_constraints_queries(void);
static int test_information_schema_constraints_independent_handles(void);
static int seed_database(mylite_db *database);
static int expect_statement_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_error(mylite_db *database, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_information_schema_constraints_queries();
    failures += test_information_schema_constraints_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_constraints_queries(void) {
    static const char *const table_constraint_columns[] = {
        "CONSTRAINT_CATALOG",
        "CONSTRAINT_SCHEMA",
        "CONSTRAINT_NAME",
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "CONSTRAINT_TYPE",
        "ENFORCED",
    };
    static const char *const table_constraint_values[] = {
        "def", "app", "a",       "app", "inline_unique", "UNIQUE",      "YES",
        "def", "app", "b",       "app", "inline_unique", "UNIQUE",      "YES",
        "def", "app", "PRIMARY", "app", "constrained",   "PRIMARY KEY", "YES",
        "def", "app", "u_n",     "app", "constrained",   "UNIQUE",      "YES",
        "def", "app", "u_v",     "app", "constrained",   "UNIQUE",      "YES",
    };
    static const char *const key_column_usage_columns[] = {
        "CONSTRAINT_CATALOG",
        "CONSTRAINT_SCHEMA",
        "CONSTRAINT_NAME",
        "TABLE_CATALOG",
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "POSITION_IN_UNIQUE_CONSTRAINT",
        "REFERENCED_TABLE_SCHEMA",
        "REFERENCED_TABLE_NAME",
        "REFERENCED_COLUMN_NAME",
    };
    static const char *const key_column_usage_values[] = {
        "def", "app", "a",       "def", "app", "inline_unique", "a",  "1", NULL, NULL, NULL, NULL,
        "def", "app", "b",       "def", "app", "inline_unique", "b",  "1", NULL, NULL, NULL, NULL,
        "def", "app", "PRIMARY", "def", "app", "constrained",   "id", "1", NULL, NULL, NULL, NULL,
        "def", "app", "u_n",     "def", "app", "constrained",   "n",  "1", NULL, NULL, NULL, NULL,
        "def", "app", "u_v",     "def", "app", "constrained",   "v",  "1", NULL, NULL, NULL, NULL,
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const clone_constraint_columns[] = {"CONSTRAINT_NAME", "CONSTRAINT_TYPE"};
    static const char *const clone_constraint_values[] = {
        "PRIMARY",
        "PRIMARY KEY",
        "u_n",
        "UNIQUE",
        "u_v",
        "UNIQUE",
    };
    static const char *const renamed_constraint_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "CONSTRAINT_TYPE",
    };
    static const char *const renamed_constraint_values[] = {
        "renamed_constrained",
        "PRIMARY",
        "PRIMARY KEY",
        "renamed_constrained",
        "u_n",
        "UNIQUE",
        "renamed_constrained",
        "u_v",
        "UNIQUE",
    };
    static const char *const system_table_columns[] = {
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "VERSION",
        "ROW_FORMAT",
        "TABLE_ROWS",
    };
    static const char *const system_table_values[] = {
        "information_schema",
        "KEY_COLUMN_USAGE",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "information_schema",
        "TABLE_CONSTRAINTS",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
    };
    static const char *const system_column_columns[] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "COLUMN_DEFAULT",
        "IS_NULLABLE",
        "DATA_TYPE",
        "CHARACTER_MAXIMUM_LENGTH",
        "CHARACTER_OCTET_LENGTH",
        "NUMERIC_PRECISION",
        "NUMERIC_SCALE",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
        "COLUMN_TYPE",
    };
    static const char *const system_column_values[] = {
        "KEY_COLUMN_USAGE",
        "CONSTRAINT_CATALOG",
        "1",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "KEY_COLUMN_USAGE",
        "CONSTRAINT_SCHEMA",
        "2",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "KEY_COLUMN_USAGE",
        "CONSTRAINT_NAME",
        "3",
        NULL,
        "YES",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_tolower_ci",
        "varchar(64)",
        "KEY_COLUMN_USAGE",
        "TABLE_CATALOG",
        "4",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "KEY_COLUMN_USAGE",
        "TABLE_SCHEMA",
        "5",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "KEY_COLUMN_USAGE",
        "TABLE_NAME",
        "6",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "KEY_COLUMN_USAGE",
        "COLUMN_NAME",
        "7",
        NULL,
        "YES",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_tolower_ci",
        "varchar(64)",
        "KEY_COLUMN_USAGE",
        "ORDINAL_POSITION",
        "8",
        "0",
        "NO",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        "int unsigned",
        "KEY_COLUMN_USAGE",
        "POSITION_IN_UNIQUE_CONSTRAINT",
        "9",
        NULL,
        "YES",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        "int unsigned",
        "KEY_COLUMN_USAGE",
        "REFERENCED_TABLE_SCHEMA",
        "10",
        NULL,
        "YES",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "KEY_COLUMN_USAGE",
        "REFERENCED_TABLE_NAME",
        "11",
        NULL,
        "YES",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "KEY_COLUMN_USAGE",
        "REFERENCED_COLUMN_NAME",
        "12",
        NULL,
        "YES",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_tolower_ci",
        "varchar(64)",
        "TABLE_CONSTRAINTS",
        "CONSTRAINT_CATALOG",
        "1",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "TABLE_CONSTRAINTS",
        "CONSTRAINT_SCHEMA",
        "2",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "TABLE_CONSTRAINTS",
        "CONSTRAINT_NAME",
        "3",
        NULL,
        "YES",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_tolower_ci",
        "varchar(64)",
        "TABLE_CONSTRAINTS",
        "TABLE_SCHEMA",
        "4",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "TABLE_CONSTRAINTS",
        "TABLE_NAME",
        "5",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "TABLE_CONSTRAINTS",
        "CONSTRAINT_TYPE",
        "6",
        "",
        "NO",
        "varchar",
        "11",
        "33",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(11)",
        "TABLE_CONSTRAINTS",
        "ENFORCED",
        "7",
        "",
        "NO",
        "varchar",
        "3",
        "9",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(3)",
    };
    static const char *const single_constraint_column[] = {"CONSTRAINT_NAME"};
    static const char *const alias_limit_values[] = {"PRIMARY", "u_n"};
    static const char *const single_table_column[] = {"TABLE_NAME"};
    static const char *const clone_value[] = {"clone"};
    static const char *const single_column_name[] = {"COLUMN_NAME"};
    static const char *const id_value[] = {"id"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "constraints") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open constraints db");
    failures += seed_database(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_CATALOG, CONSTRAINT_SCHEMA, CONSTRAINT_NAME, "
                   "TABLE_SCHEMA, TABLE_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE TABLE_SCHEMA = 'app' "
                   "AND (TABLE_NAME = 'constrained' OR TABLE_NAME = 'inline_unique') "
                   "ORDER BY CONSTRAINT_NAME",
            .column_names = table_constraint_columns,
            .column_count = sizeof(table_constraint_columns) / sizeof(table_constraint_columns[0]),
            .values = table_constraint_values,
            .row_count = sizeof(table_constraint_values) / sizeof(table_constraint_values[0]) /
                         (sizeof(table_constraint_columns) / sizeof(table_constraint_columns[0])),
            .context = "table constraints primary and unique rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_CATALOG, CONSTRAINT_SCHEMA, CONSTRAINT_NAME, "
                   "TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, "
                   "POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA, "
                   "REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME "
                   "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA = 'app' "
                   "AND (TABLE_NAME = 'constrained' OR TABLE_NAME = 'inline_unique') "
                   "ORDER BY CONSTRAINT_NAME",
            .column_names = key_column_usage_columns,
            .column_count = sizeof(key_column_usage_columns) / sizeof(key_column_usage_columns[0]),
            .values = key_column_usage_values,
            .row_count = sizeof(key_column_usage_values) / sizeof(key_column_usage_values[0]) /
                         (sizeof(key_column_usage_columns) / sizeof(key_column_usage_columns[0])),
            .context = "key column usage primary and unique rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'no_constraints'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "nonunique table constraints omitted",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'no_constraints'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "nonunique key column usage omitted",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE "
                   "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'clone' ORDER BY CONSTRAINT_NAME",
            .column_names = clone_constraint_columns,
            .column_count = 2U,
            .values = clone_constraint_values,
            .row_count = 3U,
            .context = "create table like clones constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'copied'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "create table select omits constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND (TABLE_NAME = 'KEY_COLUMN_USAGE' "
                   "OR TABLE_NAME = 'TABLE_CONSTRAINTS') ORDER BY TABLE_NAME",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 2U,
            .context = "system view table rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, "
                   "CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'KEY_COLUMN_USAGE' ORDER BY ORDINAL_POSITION",
            .column_names = system_column_columns,
            .column_count = sizeof(system_column_columns) / sizeof(system_column_columns[0]),
            .values = system_column_values,
            .row_count = key_column_usage_system_column_count,
            .context = "key column usage system view column rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, "
                   "CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'TABLE_CONSTRAINTS' ORDER BY ORDINAL_POSITION",
            .column_names = system_column_columns,
            .column_count = sizeof(system_column_columns) / sizeof(system_column_columns[0]),
            .values = system_column_values + ((size_t)key_column_usage_system_column_count *
                                              system_view_column_metadata_column_count),
            .row_count = table_constraints_system_column_count,
            .context = "table constraints system view column rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT tc.CONSTRAINT_NAME FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS AS tc "
                   "WHERE tc.TABLE_SCHEMA = 'app' AND tc.TABLE_NAME = 'clone' "
                   "ORDER BY tc.CONSTRAINT_NAME LIMIT 2",
            .column_names = single_constraint_column,
            .column_count = 1U,
            .values = alias_limit_values,
            .row_count = 2U,
            .context = "alias qualified ordered limited constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'clone' "
                   "AND CONSTRAINT_NAME = 'primary'",
            .column_names = single_table_column,
            .column_count = 1U,
            .values = clone_value,
            .row_count = 1U,
            .context = "constraint name metadata collation",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'clone' "
                   "AND ORDINAL_POSITION = '01' AND CONSTRAINT_NAME = 'PRIMARY'",
            .column_names = single_column_name,
            .column_count = 1U,
            .values = id_value,
            .row_count = 1U,
            .context = "numeric metadata string coercion",
        }
    );
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen constraints db");
    failures += expect_statement_ok(database, "USE app", -1);
    failures +=
        expect_statement_ok(database, "RENAME TABLE constrained TO renamed_constrained", -1);
    failures += expect_statement_ok(database, "TRUNCATE TABLE renamed_constrained", -1);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE "
                   "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'renamed_constrained' "
                   "ORDER BY CONSTRAINT_NAME",
            .column_names = renamed_constraint_columns,
            .column_count =
                sizeof(renamed_constraint_columns) / sizeof(renamed_constraint_columns[0]),
            .values = renamed_constraint_values,
            .row_count = 3U,
            .context = "rename and truncate preserve constraints",
        }
    );
    failures += expect_statement_ok(database, "DROP TABLE renamed_constrained", -1);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'renamed_constrained'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "drop removes constraint metadata",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT nope FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'field list'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT CONSTRAINT_NAME FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE nope = 'x'",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'where clause'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_information_schema_constraints_independent_handles(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const first_count[] = {"1"};
    static const char *const second_count[] = {"2"};
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
    failures += expect_statement_ok(first, "CREATE DATABASE app", -1);
    failures += expect_statement_ok(second, "CREATE DATABASE app", -1);
    failures += expect_statement_ok(first, "USE app", -1);
    failures += expect_statement_ok(second, "USE app", -1);
    failures += expect_statement_ok(first, "CREATE TABLE t (id INT PRIMARY KEY)", -1);
    failures +=
        expect_statement_ok(second, "CREATE TABLE t (id INT PRIMARY KEY, v INT UNIQUE)", -1);
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't'",
            .column_names = count_column,
            .column_count = 1U,
            .values = first_count,
            .row_count = 1U,
            .context = "first independent handle constraints",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't'",
            .column_names = count_column,
            .column_count = 1U,
            .values = second_count,
            .row_count = 1U,
            .context = "second independent handle constraints",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int seed_database(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE DATABASE app", -1);
    failures += expect_statement_ok(database, "USE app", -1);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE constrained (id INT NOT NULL, v INT, n INT NOT NULL, "
        "PRIMARY KEY (id), UNIQUE KEY u_v (v), UNIQUE KEY u_n (n), KEY k_v (v))",
        -1
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE inline_unique (a INT UNIQUE, b INT NOT NULL UNIQUE KEY)",
        -1
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE no_constraints (a INT, KEY k_a (a))", -1);
    failures += expect_statement_ok(database, "CREATE TABLE clone LIKE constrained", -1);
    failures +=
        expect_statement_ok(database, "CREATE TABLE copied AS SELECT id, v FROM constrained", 0);

    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    if (affected_rows >= 0) {
        failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
    }
    failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected query OK, got %d / %d %s %s\n",
            expected.context,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            expected.column_names[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_text_or_null(
                mylite_result_value_text(result, row, column),
                expected.values[value_index],
                expected.context
            );
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_error(mylite_db *database, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "%s: expected error, got %d\n", expected.sql, rc);
        failures += 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, expected.sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, expected.sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, expected.sql);
    failures += expect_size(mylite_result_column_count(result), 0U, expected.sql);
    mylite_result_free(result);
    return failures;
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
        "%s/mylite_information_schema_constraints_%d_%s.mylite",
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

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(stderr, "%s: expected %s, got %s\n", context, expected, actual);
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(stderr, "%s: expected '%s' to contain '%s'\n", context, actual, needle);
        return 1;
    }
    return 0;
}
