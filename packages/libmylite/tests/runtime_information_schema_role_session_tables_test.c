#include "mylite_test_support.h"

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

static int test_information_schema_role_session_tables_queries(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_error(mylite_db *database, struct expected_sql_error expected);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    return test_information_schema_role_session_tables_queries() == 0 ? 0 : 1;
}

static int test_information_schema_role_session_tables_queries(void) {
    static const char *const role_authorization_columns[] = {
        "USER",
        "HOST",
        "GRANTEE",
        "GRANTEE_HOST",
        "ROLE_NAME",
        "ROLE_HOST",
        "IS_GRANTABLE",
        "IS_DEFAULT",
        "IS_MANDATORY",
    };
    static const char *const enabled_roles_columns[] = {
        "ROLE_NAME",
        "ROLE_HOST",
        "IS_DEFAULT",
        "IS_MANDATORY",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const count_twenty_two[] = {"22"};
    static const char *const role_name_column[] = {"ROLE_NAME"};
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
        "ADMINISTRABLE_ROLE_AUTHORIZATIONS",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
        "APPLICABLE_ROLES",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
        "ENABLED_ROLES",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
    };
    static const char *const metadata_columns[] = {
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
    static const char *const administrable_role_metadata_values[] = {
        "ADMINISTRABLE_ROLE_AUTHORIZATIONS",
        "ROLE_NAME",
        "5",
        NULL,
        "YES",
        "varchar",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "varchar(255)",
        "ADMINISTRABLE_ROLE_AUTHORIZATIONS",
        "ROLE_HOST",
        "6",
        NULL,
        "YES",
        "varchar",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "varchar(256)",
        "ADMINISTRABLE_ROLE_AUTHORIZATIONS",
        "IS_MANDATORY",
        "9",
        "",
        "NO",
        "varchar",
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(3)",
    };
    static const char *const applicable_roles_metadata_values[] = {
        "APPLICABLE_ROLES",
        "ROLE_NAME",
        "5",
        NULL,
        "YES",
        "varchar",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "varchar(255)",
        "APPLICABLE_ROLES",
        "ROLE_HOST",
        "6",
        NULL,
        "YES",
        "varchar",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "varchar(256)",
        "APPLICABLE_ROLES",
        "IS_MANDATORY",
        "9",
        "",
        "NO",
        "varchar",
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(3)",
    };
    static const char *const enabled_roles_metadata_values[] = {
        "ENABLED_ROLES",
        "ROLE_NAME",
        "1",
        NULL,
        "YES",
        "varchar",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "varchar(255)",
        "ENABLED_ROLES",
        "ROLE_HOST",
        "2",
        NULL,
        "YES",
        "varchar",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "varchar(255)",
        "ENABLED_ROLES",
        "IS_MANDATORY",
        "4",
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

    if (mylite_test_make_default_path(path, sizeof(path)) != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open database");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS "
                   "LIMIT 1",
            .column_names = role_authorization_columns,
            .column_count =
                sizeof(role_authorization_columns) / sizeof(role_authorization_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "administrable role authorizations empty wildcard projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.APPLICABLE_ROLES LIMIT 1",
            .column_names = role_authorization_columns,
            .column_count =
                sizeof(role_authorization_columns) / sizeof(role_authorization_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "applicable roles empty wildcard projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.ENABLED_ROLES LIMIT 1",
            .column_names = enabled_roles_columns,
            .column_count = sizeof(enabled_roles_columns) / sizeof(enabled_roles_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "enabled roles empty wildcard projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM "
                   "INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "administrable role authorizations count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.APPLICABLE_ROLES",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "applicable roles count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ENABLED_ROLES",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "enabled roles count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROLE_NAME FROM INFORMATION_SCHEMA.ENABLED_ROLES "
                   "WHERE ROLE_NAME = 'mylite_role' OR IS_MANDATORY = 'YES' "
                   "ORDER BY ROLE_NAME LIMIT 1",
            .column_names = role_name_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "enabled roles predicates",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "DATA_LENGTH FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME IN ('ADMINISTRABLE_ROLE_AUTHORIZATIONS', "
                   "'APPLICABLE_ROLES', 'ENABLED_ROLES') ORDER BY TABLE_NAME",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = sizeof(system_table_values) / sizeof(system_table_values[0]) /
                         (sizeof(system_table_columns) / sizeof(system_table_columns[0])),
            .context = "role session system table metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME IN ('ADMINISTRABLE_ROLE_AUTHORIZATIONS', "
                   "'APPLICABLE_ROLES', 'ENABLED_ROLES')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_twenty_two,
            .row_count = 1U,
            .context = "role session columns metadata count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'ADMINISTRABLE_ROLE_AUTHORIZATIONS' "
                   "AND COLUMN_NAME IN ('ROLE_NAME', 'ROLE_HOST', 'IS_MANDATORY') "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = metadata_columns,
            .column_count = sizeof(metadata_columns) / sizeof(metadata_columns[0]),
            .values = administrable_role_metadata_values,
            .row_count = sizeof(administrable_role_metadata_values) /
                         sizeof(administrable_role_metadata_values[0]) /
                         (sizeof(metadata_columns) / sizeof(metadata_columns[0])),
            .context = "administrable role authorizations selected columns metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'APPLICABLE_ROLES' "
                   "AND COLUMN_NAME IN ('ROLE_NAME', 'ROLE_HOST', 'IS_MANDATORY') "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = metadata_columns,
            .column_count = sizeof(metadata_columns) / sizeof(metadata_columns[0]),
            .values = applicable_roles_metadata_values,
            .row_count = sizeof(applicable_roles_metadata_values) /
                         sizeof(applicable_roles_metadata_values[0]) /
                         (sizeof(metadata_columns) / sizeof(metadata_columns[0])),
            .context = "applicable roles selected columns metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'ENABLED_ROLES' "
                   "AND COLUMN_NAME IN ('ROLE_NAME', 'ROLE_HOST', 'IS_MANDATORY') "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = metadata_columns,
            .column_count = sizeof(metadata_columns) / sizeof(metadata_columns[0]),
            .values = enabled_roles_metadata_values,
            .row_count = sizeof(enabled_roles_metadata_values) /
                         sizeof(enabled_roles_metadata_values[0]) /
                         (sizeof(metadata_columns) / sizeof(metadata_columns[0])),
            .context = "enabled roles selected columns metadata",
        }
    );
    failures += expect_statement_ok(database, "USE information_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROLE_NAME FROM ENABLED_ROLES WHERE ROLE_NAME = 'mylite_role'",
            .column_names = role_name_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "selected information_schema enabled roles",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT ROLE_NAME FROM INFORMATION_SCHEMA.ENABLED_ROLES WHERE nope = 'x'",
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
    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
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

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column),
            expected.column_names[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += mylite_test_expect_text_or_null(
                mylite_result_value_text(result, row, column),
                expected.values[value_index],
                expected.context
            );
        }
    }
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
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

    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, expected.sql);
    failures +=
        mylite_test_expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, expected.sql);
    failures +=
        mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, expected.sql);
    mylite_result_free(result);
    return failures;
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
