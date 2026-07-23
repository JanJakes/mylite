#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include <inttypes.h>
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
    show_create_database_column_count = 2,
    row_count_text_capacity = 32,
    mysql_error_parse = 1064,
    mysql_error_unknown_database = 1049,
    mysql_error_incorrect_database_name = 1102,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_show_create_database_result {
    const char *database_name;
    const char *create_sql;
};

static const char *const show_create_database_columns[show_create_database_column_count] = {
    "Database",
    "Create Database",
};

static const char *const app_create_sql =
    "CREATE DATABASE `app` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE "
    "utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */";

static const char *const archive_create_sql =
    "CREATE DATABASE `archive` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE "
    "utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */";

static const char *const quoted_create_sql =
    "CREATE DATABASE `a``b` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE "
    "utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */";

static const char *const first_create_sql =
    "CREATE DATABASE `first_app` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE "
    "utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */";

static const char *const second_create_sql =
    "CREATE DATABASE `second_app` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE "
    "utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */";

static int test_show_create_database_values_persistence_and_drop(void);
static int test_show_create_database_diagnostics_and_unsupported_forms(void);
static int test_independent_show_create_database_handles(void);
static int expect_show_create_database_result(
    mylite_db *database,
    const char *sql,
    struct expected_show_create_database_result expected,
    const char *context
);
static int expect_row_count(mylite_db *database, int64_t expected, const char *context);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
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

    failures += test_show_create_database_values_persistence_and_drop();
    failures += test_show_create_database_diagnostics_and_unsupported_forms();
    failures += test_independent_show_create_database_handles();

    return failures == 0 ? 0 : 1;
}

static int test_show_create_database_values_persistence_and_drop(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "CREATE SCHEMA archive");
    failures += execute_statement_ok(database, "CREATE DATABASE `a``b`");

    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;
    failures += expect_show_create_database_result(
        database,
        "SHOW CREATE DATABASE app",
        (struct expected_show_create_database_result){
            .database_name = "app",
            .create_sql = app_create_sql,
        },
        "show create database app"
    );
    failures += expect_show_create_database_result(
        database,
        "SHOW CREATE SCHEMA archive",
        (struct expected_show_create_database_result){
            .database_name = "archive",
            .create_sql = archive_create_sql,
        },
        "show create schema archive"
    );
    failures += expect_show_create_database_result(
        database,
        "SHOW CREATE DATABASE `a``b`",
        (struct expected_show_create_database_result){
            .database_name = "a`b",
            .create_sql = quoted_create_sql,
        },
        "show create database quoted"
    );
    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_uint64(
        session->catalog_generation,
        catalog_generation,
        "catalog generation after show create database"
    );
    failures += mylite_test_expect_uint64(
        session->sqlite_schema_generation,
        sqlite_schema_generation,
        "sqlite schema generation after show create database"
    );
    failures += expect_row_count(database, -1, "row count after show create database");
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after show create database"
    );

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values file");
    failures += expect_show_create_database_result(
        database,
        "SHOW CREATE DATABASE app",
        (struct expected_show_create_database_result){
            .database_name = "app",
            .create_sql = app_create_sql,
        },
        "reopened show create database"
    );

    failures += execute_statement_ok(database, "DROP DATABASE app");
    failures += execute_error(
        database,
        "SHOW CREATE DATABASE app",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'app'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_show_create_database_diagnostics_and_unsupported_forms(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_statement_ok(database, "CREATE DATABASE app");

    failures += execute_error(
        database,
        "SHOW CREATE DATABASE missing_schema",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "SHOW CREATE SCHEMA missing_schema",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "SHOW CREATE DATABASE _mylite_catalog",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_catalog'",
        }
    );
    failures += execute_error(
        database,
        "SHOW CREATE DATABASE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW CREATE DATABASE IF NOT EXISTS app",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW CREATE DATABASE IF EXISTS app",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW CREATE DATABASE app LIKE 'a%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW CREATE DATABASE app DEFAULT CHARACTER SET utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_show_create_database_handles(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (mylite_test_make_path(first_path, sizeof(first_path), "first") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures +=
        mylite_test_expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first database");
    failures += mylite_test_expect_int(
        mylite_open(second_path, &second),
        MYLITE_OK,
        "open second database"
    );
    failures += execute_statement_ok(first, "CREATE DATABASE first_app");
    failures += execute_statement_ok(second, "CREATE DATABASE second_app");

    failures += expect_show_create_database_result(
        first,
        "SHOW CREATE DATABASE first_app",
        (struct expected_show_create_database_result){
            .database_name = "first_app",
            .create_sql = first_create_sql,
        },
        "first handle show create database"
    );
    failures += expect_show_create_database_result(
        second,
        "SHOW CREATE DATABASE second_app",
        (struct expected_show_create_database_result){
            .database_name = "second_app",
            .create_sql = second_create_sql,
        },
        "second handle show create database"
    );
    failures += execute_error(
        first,
        "SHOW CREATE DATABASE second_app",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'second_app'",
        }
    );
    failures += execute_error(
        second,
        "SHOW CREATE DATABASE first_app",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'first_app'",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int expect_show_create_database_result(
    mylite_db *database,
    const char *sql,
    struct expected_show_create_database_result expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        show_create_database_column_count,
        context
    );
    for (size_t index = 0U; index < show_create_database_column_count; ++index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, index),
            show_create_database_columns[index],
            context
        );
    }
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, 0U, 0U),
        expected.database_name,
        context
    );
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, 0U, 1U),
        expected.create_sql,
        context
    );
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, context);
    mylite_result_free(result);

    return failures;
}

static int expect_row_count(mylite_db *database, int64_t expected, const char *context) {
    char expected_text[row_count_text_capacity];
    mylite_result *result = NULL;
    int written = snprintf(expected_text, sizeof(expected_text), "%" PRId64, expected);
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(expected_text)) {
        fprintf(stderr, "failed to format row count expectation for %s\n", context);
        return 1;
    }

    failures += execute_ok(database, "SELECT ROW_COUNT()", &result);
    failures += mylite_test_expect_size(mylite_result_column_count(result), 1U, context);
    failures += mylite_test_expect_text_or_null(
        mylite_result_column_name(result, 0U),
        "ROW_COUNT()",
        context
    );
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, 0U, 0U),
        expected_text,
        context
    );
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, context);
    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result != NULL) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected success for %s, got rc=%d code=%d state=%s message=%s\n",
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
        fprintf(stderr, "expected error for %s, got rc=%d\n", sql, rc);
        mylite_result_free(result);
        return 1;
    }

    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
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
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    int failures = 0;

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        failures += 1;
    } else if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        failures += 1;
    }
    if (fclose(file) != 0) {
        failures += 1;
    }

    return failures;
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
