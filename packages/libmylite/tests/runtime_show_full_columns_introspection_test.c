#include <mylite/mylite.h>

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
    show_full_columns_column_count = 9,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_does_not_exist = 1146,
    decimal_base = 10,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static const char *const show_full_columns_names[show_full_columns_column_count] = {
    "Field",
    "Type",
    "Collation",
    "Null",
    "Key",
    "Default",
    "Extra",
    "Privileges",
    "Comment",
};

static const char privileges[] = "select,insert,update,references";

static const char *const full_numbers_rows[][show_full_columns_column_count] = {
    {"id", "int", NULL, "NO", "PRI", NULL, "auto_increment", privileges, ""},
    {"i", "int", NULL, "YES", "", NULL, "", privileges, ""},
    {"dec_col", "decimal(5,2)", NULL, "YES", "", NULL, "", privileges, ""},
    {"f", "float", NULL, "YES", "", NULL, "", privileges, ""},
    {"y", "year", NULL, "YES", "", NULL, "", privileges, ""},
    {"d", "date", NULL, "YES", "", NULL, "", privileges, ""},
    {"tm", "time", NULL, "YES", "", NULL, "", privileges, ""},
    {
        "dt",
        "datetime",
        NULL,
        "YES",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
        privileges,
        "",
    },
    {
        "ts",
        "timestamp",
        NULL,
        "YES",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        privileges,
        "",
    },
    {"c", "char(3)", "utf8mb4_0900_ai_ci", "NO", "", NULL, "", privileges, ""},
    {"v", "varchar(10)", "utf8mb4_0900_ai_ci", "YES", "MUL", "x", "", privileges, ""},
    {"txt", "text", "utf8mb4_0900_ai_ci", "YES", "", NULL, "", privileges, ""},
    {"b", "binary(2)", NULL, "YES", "", NULL, "", privileges, ""},
    {"vb", "varbinary(3)", NULL, "YES", "", NULL, "", privileges, ""},
    {"bits", "bit(3)", NULL, "YES", "", NULL, "", privileges, ""},
    {"e", "enum('a','b')", "utf8mb4_0900_ai_ci", "YES", "", NULL, "", privileges, ""},
    {"s", "set('a','b')", "utf8mb4_0900_ai_ci", "YES", "", NULL, "", privileges, ""},
    {"j", "json", NULL, "YES", "", NULL, "", privileges, ""},
    {"inv", "int", NULL, "YES", "", NULL, "INVISIBLE", privileges, ""},
};

static const char *const like_rows[][show_full_columns_column_count] = {
    {"v", "varchar(10)", "utf8mb4_0900_ai_ci", "YES", "MUL", "x", "", privileges, ""},
    {"vb", "varbinary(3)", NULL, "YES", "", NULL, "", privileges, ""},
};

static const char *const other_rows[][show_full_columns_column_count] = {
    {"other_id", "bigint", NULL, "YES", "", NULL, "", privileges, ""},
};

static const char *const temp_rows[][show_full_columns_column_count] = {
    {"tmp_id", "int", NULL, "NO", "", NULL, "", privileges, ""},
};

static const char *const binary_collation_rows[][show_full_columns_column_count] = {
    {"v", "varchar(4)", "utf8mb4_bin", "YES", "", NULL, "", privileges, ""},
};

static int test_show_full_columns_values_persistence_rename_and_drop(void);
static int test_show_full_columns_temporary_shadow_and_collation(void);
static int test_show_full_columns_diagnostics_and_independent_handles(void);
static int create_full_columns_schema(mylite_db *database);
static int expect_show_full_columns_result(
    mylite_db *database,
    const char *sql,
    const char *const expected_rows[][show_full_columns_column_count],
    size_t expected_row_count,
    const char *context
);
static int expect_row_count(mylite_db *database, int64_t expected, const char *context);
static int execute_statement_ok(mylite_db *database, const char *sql);
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

    failures += test_show_full_columns_values_persistence_rename_and_drop();
    failures += test_show_full_columns_temporary_shadow_and_collation();
    failures += test_show_full_columns_diagnostics_and_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_show_full_columns_values_persistence_rename_and_drop(void) {
    static const char *const forms[] = {
        "SHOW FULL COLUMNS FROM numbers",
        "SHOW FULL COLUMNS IN numbers",
        "SHOW FULL FIELDS FROM numbers",
        "SHOW FULL FIELDS IN numbers",
        "SHOW FULL COLUMNS FROM app.numbers",
        "SHOW FULL COLUMNS FROM numbers FROM app",
        "SHOW FULL COLUMNS FROM numbers IN app",
        "SHOW FULL COLUMNS IN numbers FROM app",
        "SHOW FULL COLUMNS IN numbers IN app",
        "SHOW FULL FIELDS FROM numbers FROM app",
        "SHOW FULL FIELDS FROM numbers IN app",
        "SHOW FULL FIELDS IN numbers FROM app",
        "SHOW FULL FIELDS IN numbers IN app",
    };
    static const char *const trailing_schema_forms[] = {
        "SHOW FULL COLUMNS FROM app.numbers FROM other",
        "SHOW FULL COLUMNS FROM app.numbers IN other",
        "SHOW FULL COLUMNS IN app.numbers FROM other",
        "SHOW FULL COLUMNS IN app.numbers IN other",
        "SHOW FULL FIELDS FROM app.numbers FROM other",
        "SHOW FULL FIELDS FROM app.numbers IN other",
        "SHOW FULL FIELDS IN app.numbers FROM other",
        "SHOW FULL FIELDS IN app.numbers IN other",
    };
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
    failures += create_full_columns_schema(database);
    failures += execute_statement_ok(database, "CREATE TABLE other.numbers(other_id BIGINT NULL)");

    failures += expect_show_full_columns_result(
        database,
        "SHOW FULL COLUMNS FROM app.numbers",
        full_numbers_rows,
        sizeof(full_numbers_rows) / sizeof(full_numbers_rows[0]),
        "qualified show full columns without default schema"
    );
    failures += execute_statement_ok(database, "USE app");

    for (size_t form_index = 0U; form_index < sizeof(forms) / sizeof(forms[0]); ++form_index) {
        failures += expect_show_full_columns_result(
            database,
            forms[form_index],
            full_numbers_rows,
            sizeof(full_numbers_rows) / sizeof(full_numbers_rows[0]),
            forms[form_index]
        );
    }
    failures += expect_show_full_columns_result(
        database,
        "SHOW FULL COLUMNS FROM numbers LIKE 'v%'",
        like_rows,
        sizeof(like_rows) / sizeof(like_rows[0]),
        "show full columns like"
    );
    for (size_t form_index = 0U;
         form_index < sizeof(trailing_schema_forms) / sizeof(trailing_schema_forms[0]);
         ++form_index) {
        failures += expect_show_full_columns_result(
            database,
            trailing_schema_forms[form_index],
            other_rows,
            sizeof(other_rows) / sizeof(other_rows[0]),
            trailing_schema_forms[form_index]
        );
    }
    failures += expect_row_count(database, -1, "row count after show full columns");
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after show full columns"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_show_full_columns_result(
        database,
        "SHOW FULL COLUMNS FROM numbers",
        full_numbers_rows,
        sizeof(full_numbers_rows) / sizeof(full_numbers_rows[0]),
        "reopened show full columns"
    );

    failures += execute_statement_ok(database, "RENAME TABLE numbers TO renamed_numbers");
    failures += execute_error(
        database,
        "SHOW FULL COLUMNS FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.numbers' doesn't exist",
        }
    );
    failures += expect_show_full_columns_result(
        database,
        "SHOW FULL COLUMNS FROM renamed_numbers",
        full_numbers_rows,
        sizeof(full_numbers_rows) / sizeof(full_numbers_rows[0]),
        "renamed show full columns"
    );

    failures += execute_statement_ok(database, "DROP TABLE renamed_numbers");
    failures += execute_error(
        database,
        "SHOW FULL COLUMNS FROM renamed_numbers",
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

static int test_show_full_columns_temporary_shadow_and_collation(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "temporary") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open temporary file");
    failures += create_full_columns_schema(database);
    failures += execute_statement_ok(database, "USE app");
    failures +=
        execute_statement_ok(database, "CREATE TEMPORARY TABLE numbers(tmp_id INT NOT NULL)");
    failures += expect_show_full_columns_result(
        database,
        "SHOW FULL COLUMNS FROM numbers",
        temp_rows,
        sizeof(temp_rows) / sizeof(temp_rows[0]),
        "temporary table shadows persistent show full columns"
    );
    failures += execute_statement_ok(database, "DROP TEMPORARY TABLE numbers");
    failures += expect_show_full_columns_result(
        database,
        "SHOW FULL COLUMNS FROM numbers LIKE 'v'",
        like_rows,
        1U,
        "persistent table visible after temporary drop"
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE binary_collation(v VARCHAR(4)) COLLATE=utf8mb4_bin"
    );
    failures += expect_show_full_columns_result(
        database,
        "SHOW FULL COLUMNS FROM binary_collation",
        binary_collation_rows,
        sizeof(binary_collation_rows) / sizeof(binary_collation_rows[0]),
        "table default collation is visible"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_show_full_columns_diagnostics_and_independent_handles(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "diagnostics-a") != 0 ||
        make_test_path(second_path, sizeof(second_path), "diagnostics-b") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += create_full_columns_schema(first);

    failures += execute_error(
        first,
        "SHOW FULL COLUMNS FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        first,
        "SHOW FULL COLUMNS FROM missing_schema.numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        first,
        "SHOW FULL COLUMNS FROM numbers FROM missing_schema",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        first,
        "SHOW FULL COLUMNS FROM _mylite_catalog.numbers",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_catalog'",
        }
    );

    failures += execute_statement_ok(first, "USE app");
    failures += execute_error(
        first,
        "SHOW FULL COLUMNS FROM missing_table",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        first,
        "SHOW FULL COLUMNS FROM _mylite_numbers",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_numbers'",
        }
    );
    failures += execute_error(
        first,
        "SHOW FULL COLUMNS FROM numbers WHERE Field = 'id'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        first,
        "SHOW EXTENDED FULL COLUMNS FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    failures += execute_statement_ok(second, "CREATE DATABASE app");
    failures += execute_statement_ok(second, "USE app");
    failures += execute_statement_ok(second, "CREATE TABLE numbers(other_id BIGINT NULL)");
    failures += expect_show_full_columns_result(
        first,
        "SHOW FULL COLUMNS FROM numbers LIKE 'id'",
        full_numbers_rows,
        1U,
        "first handle full columns"
    );
    failures += expect_show_full_columns_result(
        second,
        "SHOW FULL COLUMNS FROM numbers",
        other_rows,
        sizeof(other_rows) / sizeof(other_rows[0]),
        "second handle full columns"
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int create_full_columns_schema(mylite_db *database) {
    int failures = 0;

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "CREATE DATABASE other");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE app.numbers("
        "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
        "i INTEGER NULL, "
        "dec_col DECIMAL(5,2) NULL, "
        "f FLOAT NULL, "
        "y YEAR NULL, "
        "d DATE NULL, "
        "tm TIME NULL, "
        "dt DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
        "ts TIMESTAMP NULL DEFAULT CURRENT_TIMESTAMP, "
        "c CHAR(3) NOT NULL, "
        "v VARCHAR(10) DEFAULT 'x', "
        "txt TEXT, "
        "b BINARY(2), "
        "vb VARBINARY(3), "
        "bits BIT(3), "
        "e ENUM('a','b'), "
        "s SET('a','b'), "
        "j JSON, "
        "inv INT, "
        "KEY idx_v (v(3)))"
    );
    failures +=
        execute_statement_ok(database, "ALTER TABLE app.numbers ALTER COLUMN inv SET INVISIBLE");

    return failures;
}

static int expect_show_full_columns_result(
    mylite_db *database,
    const char *sql,
    const char *const expected_rows[][show_full_columns_column_count],
    size_t expected_row_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), show_full_columns_column_count, context);
    failures += expect_size(mylite_result_row_count(result), expected_row_count, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    for (size_t column_index = 0U; column_index < show_full_columns_column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            show_full_columns_names[column_index],
            context
        );
    }
    for (size_t row_index = 0U; row_index < expected_row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < show_full_columns_column_count;
             ++column_index) {
            failures += expect_text_or_null(
                mylite_result_value_text(result, row_index, column_index),
                expected_rows[row_index][column_index],
                context
            );
        }
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
        "/tmp/mylite_show_full_columns_%s_%d.mylite",
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
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %s\n", path);
        fclose(file);
        return 1;
    }

    fclose(file);
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
        fprintf(stderr, "%s: expected %" PRId64 ", got %" PRId64 "\n", context, expected, actual);
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
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "NULL" : actual,
            needle == NULL ? "NULL" : needle
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
