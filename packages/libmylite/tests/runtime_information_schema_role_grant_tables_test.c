#include <mylite/mylite.h>

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
    mysql_error_unknown_column = 1054,
    test_path_capacity = 1024,
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

static int test_information_schema_role_grant_tables_queries(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_error(mylite_db *database, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_information_schema_role_grant_tables_queries() == 0 ? 0 : 1;
}

static int test_information_schema_role_grant_tables_queries(void) {
    static const char *const role_column_grants_columns[] = {
        "GRANTOR",
        "GRANTOR_HOST",
        "GRANTEE",
        "GRANTEE_HOST",
        "TABLE_CATALOG",
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "COLUMN_NAME",
        "PRIVILEGE_TYPE",
        "IS_GRANTABLE",
    };
    static const char *const role_routine_grants_columns[] = {
        "GRANTOR",
        "GRANTOR_HOST",
        "GRANTEE",
        "GRANTEE_HOST",
        "SPECIFIC_CATALOG",
        "SPECIFIC_SCHEMA",
        "SPECIFIC_NAME",
        "ROUTINE_CATALOG",
        "ROUTINE_SCHEMA",
        "ROUTINE_NAME",
        "PRIVILEGE_TYPE",
        "IS_GRANTABLE",
    };
    static const char *const role_table_grants_columns[] = {
        "GRANTOR",
        "GRANTOR_HOST",
        "GRANTEE",
        "GRANTEE_HOST",
        "TABLE_CATALOG",
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "PRIVILEGE_TYPE",
        "IS_GRANTABLE",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const count_thirty_one[] = {"31"};
    static const char *const table_name_column[] = {"TABLE_NAME"};
    static const char *const system_table_columns[] = {
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "VERSION",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "DATA_LENGTH",
    };
    static const char *const system_table_values[] = {
        "ROLE_COLUMN_GRANTS",  "SYSTEM VIEW", NULL, "10", NULL, "0", "0",
        "ROLE_ROUTINE_GRANTS", "SYSTEM VIEW", NULL, "10", NULL, "0", "0",
        "ROLE_TABLE_GRANTS",   "SYSTEM VIEW", NULL, "10", NULL, "0", "0",
    };
    static const char *const privilege_metadata_columns[] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "COLUMN_DEFAULT",
        "IS_NULLABLE",
        "DATA_TYPE",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
        "COLUMN_TYPE",
    };
    static const char *const role_column_grants_privilege_metadata_values[] = {
        "ROLE_COLUMN_GRANTS",
        "PRIVILEGE_TYPE",
        "9",
        "",
        "NO",
        "set",
        "utf8mb3",
        "utf8mb3_general_ci",
        "set('Select','Insert','Update','References')",
        "ROLE_COLUMN_GRANTS",
        "IS_GRANTABLE",
        "10",
        "",
        "NO",
        "varchar",
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(3)",
    };
    static const char *const role_routine_grants_privilege_metadata_values[] = {
        "ROLE_ROUTINE_GRANTS",
        "PRIVILEGE_TYPE",
        "11",
        "",
        "NO",
        "set",
        "utf8mb3",
        "utf8mb3_general_ci",
        "set('Execute','Alter Routine','Grant')",
        "ROLE_ROUTINE_GRANTS",
        "IS_GRANTABLE",
        "12",
        "",
        "NO",
        "varchar",
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(3)",
    };
    static const char *const role_table_grants_privilege_metadata_values[] = {
        "ROLE_TABLE_GRANTS",
        "PRIVILEGE_TYPE",
        "8",
        "",
        "NO",
        "set",
        "utf8mb3",
        "utf8mb3_general_ci",
        ("set('Select','Insert','Update','Delete','Create','Drop','Grant','References','Index','"
         "Alter','Create View','Show view','Trigger')"),
        "ROLE_TABLE_GRANTS",
        "IS_GRANTABLE",
        "9",
        "",
        "NO",
        "varchar",
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(3)",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path)) != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open database");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.ROLE_COLUMN_GRANTS LIMIT 1",
            .column_names = role_column_grants_columns,
            .column_count =
                sizeof(role_column_grants_columns) / sizeof(role_column_grants_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "role column grants empty wildcard projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.ROLE_ROUTINE_GRANTS LIMIT 1",
            .column_names = role_routine_grants_columns,
            .column_count =
                sizeof(role_routine_grants_columns) / sizeof(role_routine_grants_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "role routine grants empty wildcard projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.ROLE_TABLE_GRANTS LIMIT 1",
            .column_names = role_table_grants_columns,
            .column_count =
                sizeof(role_table_grants_columns) / sizeof(role_table_grants_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "role table grants empty wildcard projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ROLE_COLUMN_GRANTS",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "role column grants count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ROLE_ROUTINE_GRANTS",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "role routine grants count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ROLE_TABLE_GRANTS",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "role table grants count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GRANTEE FROM INFORMATION_SCHEMA.ROLE_TABLE_GRANTS "
                   "WHERE GRANTOR IS NULL OR PRIVILEGE_TYPE = 'Select' "
                   "ORDER BY GRANTEE LIMIT 1",
            .column_names = (const char *const[]){"GRANTEE"},
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "role table grants predicates",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "DATA_LENGTH FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME IN ('ROLE_COLUMN_GRANTS', 'ROLE_ROUTINE_GRANTS', "
                   "'ROLE_TABLE_GRANTS') ORDER BY TABLE_NAME",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = sizeof(system_table_values) / sizeof(system_table_values[0]) /
                         (sizeof(system_table_columns) / sizeof(system_table_columns[0])),
            .context = "role grant system table metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME IN ('ROLE_COLUMN_GRANTS', 'ROLE_ROUTINE_GRANTS', "
                   "'ROLE_TABLE_GRANTS')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_thirty_one,
            .row_count = 1U,
            .context = "role grant columns metadata count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'ROLE_COLUMN_GRANTS' "
                   "AND COLUMN_NAME IN ('PRIVILEGE_TYPE', 'IS_GRANTABLE') "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = privilege_metadata_columns,
            .column_count =
                sizeof(privilege_metadata_columns) / sizeof(privilege_metadata_columns[0]),
            .values = role_column_grants_privilege_metadata_values,
            .row_count =
                sizeof(role_column_grants_privilege_metadata_values) /
                sizeof(role_column_grants_privilege_metadata_values[0]) /
                (sizeof(privilege_metadata_columns) / sizeof(privilege_metadata_columns[0])),
            .context = "role column grants selected columns metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'ROLE_ROUTINE_GRANTS' "
                   "AND COLUMN_NAME IN ('PRIVILEGE_TYPE', 'IS_GRANTABLE') "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = privilege_metadata_columns,
            .column_count =
                sizeof(privilege_metadata_columns) / sizeof(privilege_metadata_columns[0]),
            .values = role_routine_grants_privilege_metadata_values,
            .row_count =
                sizeof(role_routine_grants_privilege_metadata_values) /
                sizeof(role_routine_grants_privilege_metadata_values[0]) /
                (sizeof(privilege_metadata_columns) / sizeof(privilege_metadata_columns[0])),
            .context = "role routine grants selected columns metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'ROLE_TABLE_GRANTS' "
                   "AND COLUMN_NAME IN ('PRIVILEGE_TYPE', 'IS_GRANTABLE') "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = privilege_metadata_columns,
            .column_count =
                sizeof(privilege_metadata_columns) / sizeof(privilege_metadata_columns[0]),
            .values = role_table_grants_privilege_metadata_values,
            .row_count =
                sizeof(role_table_grants_privilege_metadata_values) /
                sizeof(role_table_grants_privilege_metadata_values[0]) /
                (sizeof(privilege_metadata_columns) / sizeof(privilege_metadata_columns[0])),
            .context = "role table grants selected columns metadata",
        }
    );
    failures += expect_statement_ok(database, "USE information_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME FROM ROLE_TABLE_GRANTS WHERE GRANTEE = 'mylite_role'",
            .column_names = table_name_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "selected information_schema role table grants",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT GRANTEE FROM INFORMATION_SCHEMA.ROLE_TABLE_GRANTS WHERE nope = 'x'",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'where clause'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
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
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    mylite_result_free(result);
    return failures;
}

static int expect_error(mylite_db *database, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got OK\n", expected.sql);
        mylite_result_free(result);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, expected.sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, expected.sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, expected.sql);
    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size) {
    int written = snprintf(
        path,
        path_size,
        "runtime_information_schema_role_grant_tables_%d.mylite",
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path\n");
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
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    remove(related);
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
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "NULL" : expected,
                actual == NULL ? "NULL" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected [%s], got [%s]\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing [%s], got [%s]\n",
            context,
            needle,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}
