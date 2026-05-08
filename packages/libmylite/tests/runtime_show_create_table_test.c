#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include "sqlite3.h"

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
    show_create_column_count = 2,
    decimal_base = 10,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_unknown = 1105,
    mysql_error_table_does_not_exist = 1146,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_show_create_result {
    const char *table_name;
    const char *create_sql;
};

struct expected_single_row_result {
    const char *const *columns;
    const char *const *values;
    size_t column_count;
};

static const char *const show_create_columns[show_create_column_count] = {
    "Table",
    "Create Table",
};

static const char *const numbers_create_sql =
    "CREATE TABLE `numbers` (\n"
    "  `id` int NOT NULL,\n"
    "  `i` int DEFAULT NULL,\n"
    "  `iu` int unsigned DEFAULT NULL,\n"
    "  `iuu` int unsigned DEFAULT NULL,\n"
    "  `b` bigint DEFAULT NULL,\n"
    "  `bu` bigint unsigned DEFAULT NULL,\n"
    "  `nn` bigint unsigned NOT NULL\n"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci";

static const char *const renamed_create_sql =
    "CREATE TABLE `renamed_numbers` (\n"
    "  `id` int NOT NULL,\n"
    "  `i` int DEFAULT NULL,\n"
    "  `iu` int unsigned DEFAULT NULL,\n"
    "  `iuu` int unsigned DEFAULT NULL,\n"
    "  `b` bigint DEFAULT NULL,\n"
    "  `bu` bigint unsigned DEFAULT NULL,\n"
    "  `nn` bigint unsigned NOT NULL\n"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci";

static const char *const other_create_sql =
    "CREATE TABLE `numbers` (\n"
    "  `other_id` bigint DEFAULT NULL\n"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci";

static const char *const quoted_create_sql =
    "CREATE TABLE `a``b` (\n"
    "  `x``y` int DEFAULT NULL\n"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci";

static const char *const null_forms_create_sql =
    "CREATE TABLE `null_forms` (\n"
    "  `implicit_nullable` int DEFAULT NULL,\n"
    "  `explicit_nullable` int DEFAULT NULL,\n"
    "  `required_value` int NOT NULL\n"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci";

static const char *const single_column_create_sql =
    "CREATE TABLE `numbers` (\n"
    "  `id` int NOT NULL\n"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci";

static const char *const pair_column_create_sql =
    "CREATE TABLE `numbers` (\n"
    "  `id` int NOT NULL,\n"
    "  `value` bigint DEFAULT NULL\n"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci";

static int test_show_create_values_persistence_rename_and_drop(void);
static int test_show_create_diagnostics_and_unsupported_forms(void);
static int test_show_create_descriptor_failure_paths(void);
static int test_independent_show_create_handles(void);
static int create_numbers_schema(mylite_db *database);
static int expect_show_create_result(
    mylite_db *database,
    const char *sql,
    struct expected_show_create_result expected,
    const char *context
);
static int expect_single_row_result(
    mylite_db *database,
    const char *sql,
    struct expected_single_row_result expected,
    const char *context
);
static int expect_row_count(mylite_db *database, int64_t expected, const char *context);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_sql(sqlite3 *connection, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_show_create_values_persistence_rename_and_drop();
    failures += test_show_create_diagnostics_and_unsupported_forms();
    failures += test_show_create_descriptor_failure_paths();
    failures += test_independent_show_create_handles();

    return failures == 0 ? 0 : 1;
}

static int test_show_create_values_persistence_rename_and_drop(void) {
    static const char *const select_columns[] = {"id", "i"};
    static const char *const select_values[] = {"1", "10"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    failures += create_numbers_schema(database);
    failures += execute_statement_ok(database, "CREATE TABLE other.numbers (other_id BIGINT NULL)");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "INSERT INTO numbers (id, i, iu, iuu, b, bu, nn) VALUES (1, 10, 11, 12, 13, 14, 15)"
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE null_forms ("
        "implicit_nullable INT, "
        "explicit_nullable INT NULL, "
        "required_value INT NOT NULL)"
    );

    failures += expect_show_create_result(
        database,
        "SHOW CREATE TABLE numbers",
        (struct expected_show_create_result){
            .table_name = "numbers",
            .create_sql = numbers_create_sql,
        },
        "show create numbers"
    );
    failures += expect_show_create_result(
        database,
        "SHOW CREATE TABLE app.numbers",
        (struct expected_show_create_result){
            .table_name = "numbers",
            .create_sql = numbers_create_sql,
        },
        "qualified show create numbers"
    );
    failures += expect_show_create_result(
        database,
        "SHOW CREATE TABLE other.numbers",
        (struct expected_show_create_result){
            .table_name = "numbers",
            .create_sql = other_create_sql,
        },
        "other schema show create numbers"
    );
    failures += execute_statement_ok(database, "CREATE TABLE `a``b` (`x``y` INT NULL)");
    failures += expect_show_create_result(
        database,
        "SHOW CREATE TABLE `a``b`",
        (struct expected_show_create_result){
            .table_name = "a`b",
            .create_sql = quoted_create_sql,
        },
        "quoted show create"
    );
    failures += expect_show_create_result(
        database,
        "SHOW CREATE TABLE null_forms",
        (struct expected_show_create_result){
            .table_name = "null_forms",
            .create_sql = null_forms_create_sql,
        },
        "implicit and explicit nullable show create"
    );
    failures += expect_row_count(database, -1, "row count after show create");
    failures += expect_single_row_result(
        database,
        "SELECT id, i FROM numbers ORDER BY id",
        (struct expected_single_row_result){
            .columns = select_columns,
            .values = select_values,
            .column_count = sizeof(select_columns) / sizeof(select_columns[0]),
        },
        "rows after show create"
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after show create"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_show_create_result(
        database,
        "SHOW CREATE TABLE numbers",
        (struct expected_show_create_result){
            .table_name = "numbers",
            .create_sql = numbers_create_sql,
        },
        "reopened show create numbers"
    );

    failures += execute_statement_ok(database, "RENAME TABLE numbers TO renamed_numbers");
    failures += execute_error(
        database,
        "SHOW CREATE TABLE numbers",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.numbers' doesn't exist",
        }
    );
    failures += expect_show_create_result(
        database,
        "SHOW CREATE TABLE renamed_numbers",
        (struct expected_show_create_result){
            .table_name = "renamed_numbers",
            .create_sql = renamed_create_sql,
        },
        "renamed show create"
    );

    failures += execute_statement_ok(database, "DROP TABLE renamed_numbers");
    failures += execute_error(
        database,
        "SHOW CREATE TABLE renamed_numbers",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_numbers' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_show_create_diagnostics_and_unsupported_forms(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics database");
    failures += create_numbers_schema(database);

    failures += execute_error(
        database,
        "SHOW CREATE TABLE numbers",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "SHOW CREATE TABLE missing_schema.numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "SHOW CREATE TABLE _mylite_catalog.numbers",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_catalog'",
        }
    );

    failures += execute_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "SHOW CREATE TABLE missing_table",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SHOW CREATE TABLE _mylite_numbers",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_numbers'",
        }
    );
    failures += execute_error(
        database,
        "SHOW CREATE TABLE numbers LIKE 'numbers'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW CREATE TABLE numbers WHERE Table = 'numbers'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW CREATE TABLE numbers FROM app",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW CREATE TEMPORARY TABLE numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW FULL CREATE TABLE numbers",
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

static int test_show_create_descriptor_failure_paths(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "descriptor-failures") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open descriptor database");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE future_type (id INT NOT NULL)");
    failures += execute_statement_ok(database, "CREATE TABLE zero_columns (id INT NOT NULL)");

    sqlite = mylite_connection_sqlite_for_test(database);
    failures += execute_sql(
        sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET logical_type = 'FUTURE_INT' "
        "WHERE table_id = ("
        "SELECT table_id FROM _mylite_catalog_tables "
        "WHERE name = 'future_type' "
        "AND schema_id = (SELECT schema_id FROM _mylite_catalog_schemas WHERE name = 'app')) "
        "AND name = 'id'"
    );
    failures += execute_sql(
        sqlite,
        "DELETE FROM _mylite_catalog_columns "
        "WHERE table_id = ("
        "SELECT table_id FROM _mylite_catalog_tables "
        "WHERE name = 'zero_columns' "
        "AND schema_id = (SELECT schema_id FROM _mylite_catalog_schemas WHERE name = 'app'))"
    );

    failures += execute_error(
        database,
        "SHOW CREATE TABLE future_type",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW CREATE TABLE supports only integer column descriptors",
        }
    );
    failures += execute_error(
        database,
        "SHOW CREATE TABLE zero_columns",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "table descriptor has no columns",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_show_create_handles(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent-a") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent-b") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");

    failures += execute_statement_ok(first, "CREATE DATABASE app");
    failures += execute_statement_ok(first, "USE app");
    failures += execute_statement_ok(first, "CREATE TABLE numbers (id INT NOT NULL)");

    failures += execute_statement_ok(second, "CREATE DATABASE app");
    failures += execute_statement_ok(second, "USE app");
    failures +=
        execute_statement_ok(second, "CREATE TABLE numbers (id INT NOT NULL, value BIGINT NULL)");

    failures += expect_show_create_result(
        first,
        "SHOW CREATE TABLE numbers",
        (struct expected_show_create_result){
            .table_name = "numbers",
            .create_sql = single_column_create_sql,
        },
        "first handle show create"
    );
    failures += expect_show_create_result(
        second,
        "SHOW CREATE TABLE numbers",
        (struct expected_show_create_result){
            .table_name = "numbers",
            .create_sql = pair_column_create_sql,
        },
        "second handle show create"
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int create_numbers_schema(mylite_db *database) {
    int failures = 0;

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "CREATE DATABASE other");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE app.numbers ("
        "id INT NOT NULL, "
        "i INTEGER NULL, "
        "iu INT UNSIGNED NULL, "
        "iuu INTEGER UNSIGNED NULL, "
        "b BIGINT NULL, "
        "bu BIGINT UNSIGNED NULL, "
        "nn BIGINT UNSIGNED NOT NULL)"
    );

    return failures;
}

static int expect_show_create_result(
    mylite_db *database,
    const char *sql,
    struct expected_show_create_result expected,
    const char *context
) {
    const char *const expected_values[show_create_column_count] = {
        expected.table_name,
        expected.create_sql,
    };

    return expect_single_row_result(
        database,
        sql,
        (struct expected_single_row_result){
            .columns = show_create_columns,
            .values = expected_values,
            .column_count = show_create_column_count,
        },
        context
    );
}

static int expect_single_row_result(
    mylite_db *database,
    const char *sql,
    struct expected_single_row_result expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), expected.column_count, context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.columns[column_index],
            context
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, column_index),
            expected.values[column_index],
            context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int expect_row_count(mylite_db *database, int64_t expected, const char *context) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, "SELECT ROW_COUNT()", &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    if (mylite_result_value_text(result, 0U, 0U) == NULL) {
        fprintf(stderr, "%s: expected row count value\n", context);
        failures += 1;
    } else {
        failures += expect_int64(
            strtoll(mylite_result_value_text(result, 0U, 0U), NULL, decimal_base),
            expected,
            context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_sql(sqlite3 *connection, const char *sql) {
    char *message = NULL;
    int rc = sqlite3_exec(connection, sql, NULL, NULL, &message);

    if (rc != SQLITE_OK) {
        fprintf(
            stderr,
            "execute SQLite SQL \"%s\": error %d: %s\n",
            sql,
            rc,
            message == NULL ? "(no message)" : message
        );
        sqlite3_free(message);
        return 1;
    }

    return 0;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result == NULL || *out_result == NULL) {
        fprintf(stderr, "%s: expected result object\n", sql);
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected error %d/%s, got success\n",
            sql,
            expected.code,
            expected.sqlstate
        );
        mylite_result_free(result);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);

    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_show_create_%s_%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path for %s\n", name);
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
        fprintf(stderr, "%s: failed to open file\n", path);
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

    fprintf(stderr, "%s: expected %" PRId64 ", got %" PRId64 "\n", context, expected, actual);
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "NULL" : actual,
        needle == NULL ? "NULL" : needle
    );
    return 1;
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

    fprintf(stderr, "%s: byte comparison failed\n", context);
    return 1;
}
