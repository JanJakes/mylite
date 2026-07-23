#include "mylite_test_support.h"

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
    column_privileges_metadata_offset = 0,
    column_privileges_metadata_row_count = 7,
    mysql_error_unknown_column = 1054,
    root_global_privilege_count = 70,
    schema_privileges_metadata_offset = 7,
    schema_privileges_metadata_row_count = 5,
    table_privileges_metadata_offset = 12,
    table_privileges_metadata_row_count = 6,
    test_path_capacity = 1024,
    user_privileges_metadata_offset = 18,
    user_privileges_metadata_row_count = 4,
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

static int test_information_schema_privileges_queries(void);
static int test_information_schema_privileges_reopen_preamble_and_handles(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_error(mylite_db *database, struct expected_sql_error expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_information_schema_privileges_queries();
    failures += test_information_schema_privileges_reopen_preamble_and_handles();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_privileges_queries(void) {
    static const char *const user_privilege_columns[] = {
        "GRANTEE",
        "TABLE_CATALOG",
        "PRIVILEGE_TYPE",
        "IS_GRANTABLE",
    };
    static const char *const schema_privilege_columns[] = {
        "GRANTEE",
        "TABLE_CATALOG",
        "TABLE_SCHEMA",
        "PRIVILEGE_TYPE",
        "IS_GRANTABLE",
    };
    static const char *const table_privilege_columns[] = {
        "GRANTEE",
        "TABLE_CATALOG",
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "PRIVILEGE_TYPE",
        "IS_GRANTABLE",
    };
    static const char *const column_privilege_columns[] = {
        "GRANTEE",
        "TABLE_CATALOG",
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "COLUMN_NAME",
        "PRIVILEGE_TYPE",
        "IS_GRANTABLE",
    };
    static const char *const privilege_type_column[] = {"PRIVILEGE_TYPE"};
    static const char *const root_privilege_types[] = {
        "ALLOW_NONEXISTENT_DEFINER",
        "ALTER",
        "ALTER ROUTINE",
        "APPLICATION_PASSWORD_ADMIN",
        "AUDIT_ABORT_EXEMPT",
        "AUDIT_ADMIN",
        "AUTHENTICATION_POLICY_ADMIN",
        "BACKUP_ADMIN",
        "BINLOG_ADMIN",
        "BINLOG_ENCRYPTION_ADMIN",
        "CLONE_ADMIN",
        "CONNECTION_ADMIN",
        "CREATE",
        "CREATE ROLE",
        "CREATE ROUTINE",
        "CREATE TABLESPACE",
        "CREATE TEMPORARY TABLES",
        "CREATE USER",
        "CREATE VIEW",
        "DELETE",
        "DROP",
        "DROP ROLE",
        "ENCRYPTION_KEY_ADMIN",
        "EVENT",
        "EXECUTE",
        "FILE",
        "FIREWALL_EXEMPT",
        "FLUSH_OPTIMIZER_COSTS",
        "FLUSH_PRIVILEGES",
        "FLUSH_STATUS",
        "FLUSH_TABLES",
        "FLUSH_USER_RESOURCES",
        "GROUP_REPLICATION_ADMIN",
        "GROUP_REPLICATION_STREAM",
        "INDEX",
        "INNODB_REDO_LOG_ARCHIVE",
        "INNODB_REDO_LOG_ENABLE",
        "INSERT",
        "LOCK TABLES",
        "OPTIMIZE_LOCAL_TABLE",
        "PASSWORDLESS_USER_ADMIN",
        "PERSIST_RO_VARIABLES_ADMIN",
        "PROCESS",
        "REFERENCES",
        "RELOAD",
        "REPLICATION CLIENT",
        "REPLICATION SLAVE",
        "REPLICATION_APPLIER",
        "REPLICATION_SLAVE_ADMIN",
        "RESOURCE_GROUP_ADMIN",
        "RESOURCE_GROUP_USER",
        "ROLE_ADMIN",
        "SELECT",
        "SENSITIVE_VARIABLES_OBSERVER",
        "SERVICE_CONNECTION_ADMIN",
        "SESSION_VARIABLES_ADMIN",
        "SET_ANY_DEFINER",
        "SHOW DATABASES",
        "SHOW VIEW",
        "SHOW_ROUTINE",
        "SHUTDOWN",
        "SUPER",
        "SYSTEM_USER",
        "SYSTEM_VARIABLES_ADMIN",
        "TABLE_ENCRYPTION_ADMIN",
        "TELEMETRY_LOG_ADMIN",
        "TRANSACTION_GTID_TAG",
        "TRIGGER",
        "UPDATE",
        "XA_RECOVER_ADMIN",
    };
    static const char *const selected_root_privilege[] = {"'root'@'%'", "def", "SELECT", "YES"};
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const count_seventy[] = {"70"};
    static const char *const system_table_columns[] = {
        "TABLE_NAME",
        "TABLE_SCHEMA",
        "TABLE_TYPE",
        "ENGINE",
        "VERSION",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "DATA_LENGTH",
        "AUTO_INCREMENT",
    };
    static const char *const system_table_values[] = {
        "COLUMN_PRIVILEGES", "information_schema", "SYSTEM VIEW", NULL, "10", NULL, "0", "0", NULL,
        "SCHEMA_PRIVILEGES", "information_schema", "SYSTEM VIEW", NULL, "10", NULL, "0", "0", NULL,
        "TABLE_PRIVILEGES",  "information_schema", "SYSTEM VIEW", NULL, "10", NULL, "0", "0", NULL,
        "USER_PRIVILEGES",   "information_schema", "SYSTEM VIEW", NULL, "10", NULL, "0", "0", NULL,
    };
    static const char *const metadata_columns[] = {
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
        "DATETIME_PRECISION",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
        "COLUMN_TYPE",
        "PRIVILEGES",
    };
    static const char *const metadata_values[] = {
        "COLUMN_PRIVILEGES",
        "GRANTEE",
        "1",
        "",
        "NO",
        "varchar",
        "97",
        "292",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(292)",
        "select",
        "COLUMN_PRIVILEGES",
        "TABLE_CATALOG",
        "2",
        "",
        "NO",
        "varchar",
        "170",
        "512",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(512)",
        "select",
        "COLUMN_PRIVILEGES",
        "TABLE_SCHEMA",
        "3",
        "",
        "NO",
        "varchar",
        "21",
        "64",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        "COLUMN_PRIVILEGES",
        "TABLE_NAME",
        "4",
        "",
        "NO",
        "varchar",
        "21",
        "64",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        "COLUMN_PRIVILEGES",
        "COLUMN_NAME",
        "5",
        "",
        "NO",
        "varchar",
        "21",
        "64",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        "COLUMN_PRIVILEGES",
        "PRIVILEGE_TYPE",
        "6",
        "",
        "NO",
        "varchar",
        "21",
        "64",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        "COLUMN_PRIVILEGES",
        "IS_GRANTABLE",
        "7",
        "",
        "NO",
        "varchar",
        "1",
        "3",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(3)",
        "select",
        "SCHEMA_PRIVILEGES",
        "GRANTEE",
        "1",
        "",
        "NO",
        "varchar",
        "97",
        "292",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(292)",
        "select",
        "SCHEMA_PRIVILEGES",
        "TABLE_CATALOG",
        "2",
        "",
        "NO",
        "varchar",
        "170",
        "512",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(512)",
        "select",
        "SCHEMA_PRIVILEGES",
        "TABLE_SCHEMA",
        "3",
        "",
        "NO",
        "varchar",
        "21",
        "64",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        "SCHEMA_PRIVILEGES",
        "PRIVILEGE_TYPE",
        "4",
        "",
        "NO",
        "varchar",
        "21",
        "64",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        "SCHEMA_PRIVILEGES",
        "IS_GRANTABLE",
        "5",
        "",
        "NO",
        "varchar",
        "1",
        "3",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(3)",
        "select",
        "TABLE_PRIVILEGES",
        "GRANTEE",
        "1",
        "",
        "NO",
        "varchar",
        "97",
        "292",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(292)",
        "select",
        "TABLE_PRIVILEGES",
        "TABLE_CATALOG",
        "2",
        "",
        "NO",
        "varchar",
        "170",
        "512",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(512)",
        "select",
        "TABLE_PRIVILEGES",
        "TABLE_SCHEMA",
        "3",
        "",
        "NO",
        "varchar",
        "21",
        "64",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        "TABLE_PRIVILEGES",
        "TABLE_NAME",
        "4",
        "",
        "NO",
        "varchar",
        "21",
        "64",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        "TABLE_PRIVILEGES",
        "PRIVILEGE_TYPE",
        "5",
        "",
        "NO",
        "varchar",
        "21",
        "64",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        "TABLE_PRIVILEGES",
        "IS_GRANTABLE",
        "6",
        "",
        "NO",
        "varchar",
        "1",
        "3",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(3)",
        "select",
        "USER_PRIVILEGES",
        "GRANTEE",
        "1",
        "",
        "NO",
        "varchar",
        "97",
        "292",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(292)",
        "select",
        "USER_PRIVILEGES",
        "TABLE_CATALOG",
        "2",
        "",
        "NO",
        "varchar",
        "170",
        "512",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(512)",
        "select",
        "USER_PRIVILEGES",
        "PRIVILEGE_TYPE",
        "3",
        "",
        "NO",
        "varchar",
        "21",
        "64",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        "USER_PRIVILEGES",
        "IS_GRANTABLE",
        "4",
        "",
        "NO",
        "varchar",
        "1",
        "3",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(3)",
        "select",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "queries") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open privilege db");
    if (database == NULL) {
        remove_related_files(path);
        return failures + 1;
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT PRIVILEGE_TYPE FROM INFORMATION_SCHEMA.USER_PRIVILEGES "
                   "WHERE GRANTEE = '''root''@''%''' ORDER BY PRIVILEGE_TYPE",
            .column_names = privilege_type_column,
            .column_count = 1U,
            .values = root_privilege_types,
            .row_count = root_global_privilege_count,
            .context = "root global privilege rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.USER_PRIVILEGES "
                   "WHERE PRIVILEGE_TYPE = 'SELECT'",
            .column_names = user_privilege_columns,
            .column_count = sizeof(user_privilege_columns) / sizeof(user_privilege_columns[0]),
            .values = selected_root_privilege,
            .row_count = 1U,
            .context = "selected root privilege row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.USER_PRIVILEGES",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_seventy,
            .row_count = 1U,
            .context = "user privilege count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT up.PRIVILEGE_TYPE FROM INFORMATION_SCHEMA.USER_PRIVILEGES AS up "
                   "WHERE up.GRANTEE = '''root''@''%''' ORDER BY up.PRIVILEGE_TYPE LIMIT 1",
            .column_names = privilege_type_column,
            .column_count = 1U,
            .values = root_privilege_types,
            .row_count = 1U,
            .context = "user privilege alias order limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.SCHEMA_PRIVILEGES",
            .column_names = schema_privilege_columns,
            .column_count = sizeof(schema_privilege_columns) / sizeof(schema_privilege_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "empty schema privileges",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.TABLE_PRIVILEGES",
            .column_names = table_privilege_columns,
            .column_count = sizeof(table_privilege_columns) / sizeof(table_privilege_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "empty table privileges",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.COLUMN_PRIVILEGES",
            .column_names = column_privilege_columns,
            .column_count = sizeof(column_privilege_columns) / sizeof(column_privilege_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "empty column privileges",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.SCHEMA_PRIVILEGES",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "schema privilege count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_SCHEMA, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND (TABLE_NAME = 'COLUMN_PRIVILEGES' OR TABLE_NAME = 'SCHEMA_PRIVILEGES' "
                   "OR TABLE_NAME = 'TABLE_PRIVILEGES' OR TABLE_NAME = 'USER_PRIVILEGES') "
                   "ORDER BY TABLE_NAME",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 4U,
            .context = "privilege system table rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "
                   "NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, "
                   "COLLATION_NAME, COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'COLUMN_PRIVILEGES' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = metadata_columns,
            .column_count = sizeof(metadata_columns) / sizeof(metadata_columns[0]),
            .values = metadata_values,
            .row_count = column_privileges_metadata_row_count,
            .context = "column privileges system column rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "
                   "NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, "
                   "COLLATION_NAME, COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'SCHEMA_PRIVILEGES' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = metadata_columns,
            .column_count = sizeof(metadata_columns) / sizeof(metadata_columns[0]),
            .values = metadata_values + (schema_privileges_metadata_offset *
                                         (sizeof(metadata_columns) / sizeof(metadata_columns[0]))),
            .row_count = schema_privileges_metadata_row_count,
            .context = "schema privileges system column rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "
                   "NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, "
                   "COLLATION_NAME, COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'TABLE_PRIVILEGES' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = metadata_columns,
            .column_count = sizeof(metadata_columns) / sizeof(metadata_columns[0]),
            .values = metadata_values + (table_privileges_metadata_offset *
                                         (sizeof(metadata_columns) / sizeof(metadata_columns[0]))),
            .row_count = table_privileges_metadata_row_count,
            .context = "table privileges system column rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "
                   "NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, "
                   "COLLATION_NAME, COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'USER_PRIVILEGES' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = metadata_columns,
            .column_count = sizeof(metadata_columns) / sizeof(metadata_columns[0]),
            .values = metadata_values + (user_privileges_metadata_offset *
                                         (sizeof(metadata_columns) / sizeof(metadata_columns[0]))),
            .row_count = user_privileges_metadata_row_count,
            .context = "user privileges system column rows",
        }
    );
    failures += expect_row_count_status(database, "privilege select row count status");
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT nope FROM INFORMATION_SCHEMA.USER_PRIVILEGES",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'field list'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT GRANTEE FROM INFORMATION_SCHEMA.USER_PRIVILEGES WHERE nope = 'x'",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'where clause'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT GRANTEE FROM INFORMATION_SCHEMA.USER_PRIVILEGES ORDER BY nope",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'order clause'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_information_schema_privileges_reopen_preamble_and_handles(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const count_seventy[] = {"70"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (mylite_test_make_path(first_path, sizeof(first_path), "first") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(
        mylite_open(first_path, &first),
        MYLITE_OK,
        "open first privilege db"
    );
    failures += mylite_test_expect_int(
        mylite_open(second_path, &second),
        MYLITE_OK,
        "open second privilege db"
    );
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE other");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.USER_PRIVILEGES",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_seventy,
            .row_count = 1U,
            .context = "first handle user privilege count",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMN_PRIVILEGES",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "second handle column privilege count",
        }
    );
    failures += read_file_at(first_path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after privilege metadata query"
    );

    mylite_close(first);
    first = NULL;
    mylite_close(second);
    second = NULL;

    failures += mylite_test_expect_int(
        mylite_open(first_path, &first),
        MYLITE_OK,
        "reopen first privilege db"
    );
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.USER_PRIVILEGES",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_seventy,
            .row_count = 1U,
            .context = "reopened user privilege count",
        }
    );

    mylite_close(first);
    remove_related_files(first_path);
    remove_related_files(second_path);
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
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, sql);
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
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);
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
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, expected.sql);
    failures +=
        mylite_test_expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, expected.sql);
    failures +=
        mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, expected.sql);
    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, expected.sql);
    mylite_result_free(result);
    return failures;
}

static int expect_row_count_status(mylite_db *database, const char *context) {
    static const char *const status_columns[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const status_values[] = {"-1", "0"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .column_names = status_columns,
            .column_count = sizeof(status_columns) / sizeof(status_columns[0]),
            .values = status_values,
            .row_count = 1U,
            .context = context,
        }
    );
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
    size_t bytes_read = 0U;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    if (bytes_read != size) {
        fprintf(stderr, "%s: expected to read %zu bytes, got %zu\n", path, size, bytes_read);
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
