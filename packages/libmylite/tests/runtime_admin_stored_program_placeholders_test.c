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
    mysql_error_parse = 1064,
    test_path_capacity = 1024,
    test_path_suffix_capacity = 8,
    warning_column_count = 3,
};

struct expected_error {
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

static int test_admin_noop_warning_surface(void);
static int test_admin_noop_preserves_user_transaction(void);
static int test_stored_program_placeholder_errors(void);
static int test_existing_limited_call_still_runs(void);
static int expect_admin_noop(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_admin_noop_warning_surface();
    failures += test_admin_noop_preserves_user_transaction();
    failures += test_stored_program_placeholder_errors();
    failures += test_existing_limited_call_still_runs();

    return failures == 0 ? 0 : 1;
}

static int test_admin_noop_warning_surface(void) {
    static const char *const admin_statements[] = {
        "CREATE USER 'u'@'%' IDENTIFIED BY 'p'",
        "GRANT SELECT ON *.* TO 'u'@'%'",
        "SET ROLE DEFAULT",
        "SET PERSIST max_connections = 200",
        "SET @@PERSIST.max_connections = 200",
        "RESET MASTER",
        "RESET BINARY LOGS AND GTIDS",
        "RESET REPLICA ALL FOR CHANNEL ''",
        "FLUSH PRIVILEGES",
        "FLUSH TABLES",
        "PURGE BINARY LOGS TO 'bin.000001'",
        "PURGE BINARY LOGS BEFORE '2000-01-01 00:00:00'",
        "BINLOG 'AAAA'",
        "CHANGE REPLICATION FILTER REPLICATE_DO_DB = (wp)",
        "START REPLICA IO_THREAD, SQL_THREAD FOR CHANNEL ''",
        "STOP REPLICA SQL_THREAD FOR CHANNEL ''",
        ("START GROUP_REPLICATION USER='u', PASSWORD='p', "
         "DEFAULT_AUTH='mysql_native_password'"),
        "STOP GROUP_REPLICATION",
        "KILL QUERY @thread_id",
        "CACHE INDEX t USE key_cache",
        "LOAD INDEX INTO CACHE t",
        "ALTER INSTANCE RELOAD TLS",
    };
    static const char *const warning_rows[] = {
        "Warning",
        "1105",
        "MyLite accepted this server-only statement as an embedded no-op",
    };
    static const char *const row_count_rows[] = {"0"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open admin no-op memory");
    for (size_t index = 0U; index < sizeof(admin_statements) / sizeof(admin_statements[0]);
         ++index) {
        failures += expect_admin_noop(database, admin_statements[index]);
    }
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .values = row_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "admin no-op row count",
        }
    );
    failures += expect_admin_noop(database, "FLUSH PRIVILEGES");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = warning_rows,
            .column_count = warning_column_count,
            .row_count = 1U,
            .context = "admin no-op warning",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_admin_noop_preserves_user_transaction(void) {
    static const char *const count_rows[] = {"0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "admin_noop_transaction") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open transaction no-op");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE t (id INT)");
    failures += execute_statement_ok(database, "START TRANSACTION");
    failures += execute_statement_ok(database, "INSERT INTO t VALUES (1)");
    failures += expect_admin_noop(database, "FLUSH PRIVILEGES");
    failures += execute_statement_ok(database, "ROLLBACK");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t",
            .values = count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "admin no-op transaction rollback",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_stored_program_placeholder_errors(void) {
    static const char *const stored_program_statements[] = {
        "CREATE FUNCTION f() RETURNS INT RETURN 1",
        "CREATE TRIGGER tr BEFORE INSERT ON t FOR EACH ROW SET NEW.id = 1",
        "CREATE EVENT e ON SCHEDULE EVERY 1 DAY DO SELECT 1",
        "SET sql_mode = default; CREATE PROCEDURE p() BEGIN DECLARE y INT; END",
        "ALTER DEFINER=mysqltest_u1@localhost EVENT e1 ON SCHEDULE EVERY 1 HOUR",
        "SIGNAL SQLSTATE '01000'",
        "RESIGNAL",
        "DROP FUNCTION IF EXISTS f",
        "CALL missing_proc('x')",
        "CALL mtr.p(OUT @arg)",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open stored placeholders");
    for (size_t index = 0U;
         index < sizeof(stored_program_statements) / sizeof(stored_program_statements[0]);
         ++index) {
        failures += execute_error(
            database,
            stored_program_statements[index],
            (struct expected_error){
                .code = mysql_error_parse,
                .sqlstate = "42000",
                .message_part = "not supported",
            }
        );
    }

    mylite_close(database);
    return failures;
}

static int test_existing_limited_call_still_runs(void) {
    static const char *const call_rows[] = {"42"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "limited_procedure") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open limited procedure");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE posts (id INT PRIMARY KEY)");
    failures += execute_statement_ok(database, "INSERT INTO posts VALUES (42)");
    failures +=
        execute_statement_ok(database, "CREATE PROCEDURE p() BEGIN SELECT id FROM posts; END");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "CALL p()",
            .values = call_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "limited stored procedure call",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int expect_admin_noop(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), 0, sql);
    failures += expect_size(mylite_result_warning_count(result), 1U, sql);
    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return 0;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    return execute_ok(database, sql, NULL);
}

static int execute_error(mylite_db *database, const char *sql, struct expected_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

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

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
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

    return expect_text(actual, expected, context);
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    fprintf(stderr, "%s: expected `%s`, got `%s`\n", context, expected, actual);
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(stderr, "%s: expected `%s` to contain `%s`\n", context, actual, needle);
    return 1;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-admin-stored-program-%s-%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path too long\n");
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
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return;
    }
    (void)remove(buffer);
}
