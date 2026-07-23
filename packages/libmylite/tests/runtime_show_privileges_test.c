#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
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
    mysql_error_parse = 1064,
    show_privileges_column_count = 3,
    show_privileges_row_count = 73,
    status_column_count = 2,
    test_path_capacity = 1024,
};

struct expected_privilege_row {
    const char *privilege;
    const char *context;
    const char *comment;
};

struct expected_sql_error {
    const char *sql;
    int code;
    const char *sqlstate;
    const char *message_part;
};

static const char *const show_privileges_columns[show_privileges_column_count] = {
    "Privilege",
    "Context",
    "Comment",
};

static const struct expected_privilege_row show_privileges_rows[show_privileges_row_count] = {
    {"Alter", "Tables", "To alter the table"},
    {"Alter routine", "Functions,Procedures", "To alter or drop stored functions/procedures"},
    {"Create", "Databases,Tables,Indexes", "To create new databases and tables"},
    {"Create routine", "Databases", "To use CREATE FUNCTION/PROCEDURE"},
    {"Create role", "Server Admin", "To create new roles"},
    {"Create temporary tables", "Databases", "To use CREATE TEMPORARY TABLE"},
    {"Create view", "Tables", "To create new views"},
    {"Create user", "Server Admin", "To create new users"},
    {"Delete", "Tables", "To delete existing rows"},
    {"Drop", "Databases,Tables", "To drop databases, tables, and views"},
    {"Drop role", "Server Admin", "To drop roles"},
    {"Event", "Server Admin", "To create, alter, drop and execute events"},
    {"Execute", "Functions,Procedures", "To execute stored routines"},
    {"File", "File access on server", "To read and write files on the server"},
    {"Grant option",
     "Databases,Tables,Functions,Procedures",
     "To give to other users those privileges you possess"},
    {"Index", "Tables", "To create or drop indexes"},
    {"Insert", "Tables", "To insert data into tables"},
    {"Lock tables", "Databases", "To use LOCK TABLES (together with SELECT privilege)"},
    {"Process", "Server Admin", "To view the plain text of currently executing queries"},
    {"Proxy", "Server Admin", "To make proxy user possible"},
    {"References", "Databases,Tables", "To have references on tables"},
    {"Reload", "Server Admin", "To reload or refresh tables, logs and privileges"},
    {"Replication client", "Server Admin", "To ask where the slave or master servers are"},
    {"Replication slave", "Server Admin", "To read binary log events from the master"},
    {"Select", "Tables", "To retrieve rows from table"},
    {"Show databases", "Server Admin", "To see all databases with SHOW DATABASES"},
    {"Show view", "Tables", "To see views with SHOW CREATE VIEW"},
    {"Shutdown", "Server Admin", "To shut down the server"},
    {"Super", "Server Admin", "To use KILL thread, SET GLOBAL, CHANGE REPLICATION SOURCE, etc."},
    {"Trigger", "Tables", "To use triggers"},
    {"Create tablespace", "Server Admin", "To create/alter/drop tablespaces"},
    {"Update", "Tables", "To update existing rows"},
    {"Usage", "Server Admin", "No privileges - allow connect only"},
    {"AUDIT_ABORT_EXEMPT", "Server Admin", ""},
    {"FIREWALL_EXEMPT", "Server Admin", ""},
    {"OPTIMIZE_LOCAL_TABLE", "Server Admin", ""},
    {"ALLOW_NONEXISTENT_DEFINER", "Server Admin", ""},
    {"SET_ANY_DEFINER", "Server Admin", ""},
    {"SENSITIVE_VARIABLES_OBSERVER", "Server Admin", ""},
    {"AUTHENTICATION_POLICY_ADMIN", "Server Admin", ""},
    {"GROUP_REPLICATION_STREAM", "Server Admin", ""},
    {"FLUSH_PRIVILEGES", "Server Admin", ""},
    {"XA_RECOVER_ADMIN", "Server Admin", ""},
    {"CONNECTION_ADMIN", "Server Admin", ""},
    {"CLONE_ADMIN", "Server Admin", ""},
    {"ENCRYPTION_KEY_ADMIN", "Server Admin", ""},
    {"INNODB_REDO_LOG_ARCHIVE", "Server Admin", ""},
    {"SESSION_VARIABLES_ADMIN", "Server Admin", ""},
    {"APPLICATION_PASSWORD_ADMIN", "Server Admin", ""},
    {"REPLICATION_SLAVE_ADMIN", "Server Admin", ""},
    {"BACKUP_ADMIN", "Server Admin", ""},
    {"GROUP_REPLICATION_ADMIN", "Server Admin", ""},
    {"SYSTEM_VARIABLES_ADMIN", "Server Admin", ""},
    {"BINLOG_ADMIN", "Server Admin", ""},
    {"PERSIST_RO_VARIABLES_ADMIN", "Server Admin", ""},
    {"TRANSACTION_GTID_TAG", "Server Admin", ""},
    {"PASSWORDLESS_USER_ADMIN", "Server Admin", ""},
    {"ROLE_ADMIN", "Server Admin", ""},
    {"INNODB_REDO_LOG_ENABLE", "Server Admin", ""},
    {"RESOURCE_GROUP_USER", "Server Admin", ""},
    {"BINLOG_ENCRYPTION_ADMIN", "Server Admin", ""},
    {"SERVICE_CONNECTION_ADMIN", "Server Admin", ""},
    {"SHOW_ROUTINE", "Server Admin", ""},
    {"RESOURCE_GROUP_ADMIN", "Server Admin", ""},
    {"SYSTEM_USER", "Server Admin", ""},
    {"TABLE_ENCRYPTION_ADMIN", "Server Admin", ""},
    {"TELEMETRY_LOG_ADMIN", "Server Admin", ""},
    {"FLUSH_STATUS", "Server Admin", ""},
    {"REPLICATION_APPLIER", "Server Admin", ""},
    {"FLUSH_OPTIMIZER_COSTS", "Server Admin", ""},
    {"AUDIT_ADMIN", "Server Admin", ""},
    {"FLUSH_USER_RESOURCES", "Server Admin", ""},
    {"FLUSH_TABLES", "Server Admin", ""},
};

static int test_show_privileges_result(void);
static int test_show_privileges_file_reopen_and_preamble(void);
static int test_independent_show_privileges_handles(void);
static int test_show_privileges_unsupported_diagnostics(void);
static int expect_show_privileges(mylite_db *database, const char *sql);
static int expect_show_privileges_columns(mylite_result *result);
static int expect_show_privileges_rows(mylite_result *result);
static int expect_row_count_status(mylite_db *database, const char *context);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, struct expected_sql_error expected);
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

    mylite_result_free(NULL);

    failures += test_show_privileges_result();
    failures += test_show_privileges_file_reopen_and_preamble();
    failures += test_independent_show_privileges_handles();
    failures += test_show_privileges_unsupported_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_show_privileges_result(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open memory show privileges"
    );
    failures += expect_show_privileges(database, "SHOW PRIVILEGES");
    failures += expect_show_privileges(database, "show privileges");

    mylite_close(database);
    return failures;
}

static int test_show_privileges_file_reopen_and_preamble(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "reopen") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open show privileges file"
    );
    if (database == NULL) {
        remove_related_files(path);
        return failures;
    }
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    session = mylite_connection_session_state(database);
    if (session == NULL) {
        fprintf(stderr, "show privileges file: expected session state\n");
        mylite_close(database);
        remove_related_files(path);
        return failures + 1;
    }
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;
    failures += expect_show_privileges(database, "SHOW PRIVILEGES");
    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation after show privileges"
    );
    failures += mylite_test_expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation after show privileges"
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after show privileges"
    );
    mylite_close(database);
    database = NULL;

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "reopen show privileges file"
    );
    if (database == NULL) {
        remove_related_files(path);
        return failures;
    }
    failures += expect_show_privileges(database, "SHOW PRIVILEGES");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_show_privileges_handles(void) {
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_open_memory(&first),
        MYLITE_OK,
        "open first privileges handle"
    );
    failures += mylite_test_expect_int(
        mylite_open_memory(&second),
        MYLITE_OK,
        "open second privileges handle"
    );
    failures += expect_show_privileges(first, "SHOW PRIVILEGES");
    failures += expect_show_privileges(second, "SHOW PRIVILEGES");

    mylite_close(first);
    mylite_close(second);
    return failures;
}

static int test_show_privileges_unsupported_diagnostics(void) {
    static const struct expected_sql_error errors[] = {
        {
            .sql = "SHOW PRIVILEGES LIKE 'Select'",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        },
        {
            .sql = "SHOW PRIVILEGES WHERE Privilege = 'Select'",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        },
        {
            .sql = "SHOW FULL PRIVILEGES",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        },
        {
            .sql = "SHOW PRIVILEGES FROM mysql",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        },
        {
            .sql = "SHOW PRIVILEGES ORDER BY Privilege",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        },
        {
            .sql = "SHOW PRIVILEGES LIMIT 1",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        },
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open show privileges diagnostics"
    );
    for (size_t index = 0U; index < sizeof(errors) / sizeof(errors[0]); ++index) {
        failures += execute_error(database, errors[index]);
    }

    mylite_close(database);
    return failures;
}

static int expect_show_privileges(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        show_privileges_column_count,
        "show privileges column count"
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        show_privileges_row_count,
        "show privileges row count"
    );
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        0,
        "show privileges affected rows"
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        0U,
        "show privileges warning count"
    );
    failures += expect_show_privileges_columns(result);
    failures += expect_show_privileges_rows(result);

    mylite_result_free(result);
    failures += expect_row_count_status(database, sql);
    return failures;
}

static int expect_show_privileges_columns(mylite_result *result) {
    int failures = 0;

    for (size_t column_index = 0U; column_index < show_privileges_column_count; ++column_index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column_index),
            show_privileges_columns[column_index],
            "show privileges column label"
        );
    }
    return failures;
}

static int expect_show_privileges_rows(mylite_result *result) {
    int failures = 0;

    for (size_t row_index = 0U; row_index < show_privileges_row_count; ++row_index) {
        const struct expected_privilege_row *expected = &show_privileges_rows[row_index];

        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, row_index, 0U),
            expected->privilege,
            expected->privilege
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, row_index, 1U),
            expected->context,
            expected->privilege
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, row_index, 2U),
            expected->comment,
            expected->privilege
        );
    }
    return failures;
}

static int expect_row_count_status(mylite_db *database, const char *context) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, "SELECT @@warning_count, ROW_COUNT()", &result);
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        status_column_count,
        "show privileges status column count"
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        1U,
        "show privileges status row count"
    );
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, 0U, 0U),
        "0",
        "show privileges warnings"
    );
    failures +=
        mylite_test_expect_text_or_null(mylite_result_value_text(result, 0U, 1U), "-1", context);

    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

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
        return 1;
    }
    return 0;
}

static int execute_error(mylite_db *database, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

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
        fprintf(stderr, "%s: failed to open file for read\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read file\n", path);
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
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
    fprintf(stderr, "%s: bytes differ\n", context);
    return 1;
}
