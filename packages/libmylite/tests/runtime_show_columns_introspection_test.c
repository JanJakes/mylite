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
    show_columns_column_count = 6,
    decimal_base = 10,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_does_not_exist = 1146,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static const char *const show_columns_names[show_columns_column_count] = {
    "Field",
    "Type",
    "Null",
    "Key",
    "Default",
    "Extra",
};

static const char *const numbers_rows[][show_columns_column_count] = {
    {"id", "int", "NO", "", NULL, ""},
    {"i", "int", "YES", "", NULL, ""},
    {"iu", "int unsigned", "YES", "", NULL, ""},
    {"b", "bigint", "YES", "", NULL, ""},
    {"bu", "bigint unsigned", "YES", "", NULL, ""},
    {"nn", "bigint unsigned", "NO", "", NULL, ""},
};

static const char *const single_column_rows[][show_columns_column_count] = {
    {"id", "int", "NO", "", NULL, ""},
};

static const char *const pair_column_rows[][show_columns_column_count] = {
    {"id", "int", "NO", "", NULL, ""},
    {"value", "bigint", "YES", "", NULL, ""},
};

static const char *const other_numbers_rows[][show_columns_column_count] = {
    {"other_id", "bigint", "YES", "", NULL, ""},
};

static const char *const bigint_rows[][show_columns_column_count] = {
    {"b", "bigint", "YES", "", NULL, ""},
    {"bu", "bigint unsigned", "YES", "", NULL, ""},
    {"nn", "bigint unsigned", "NO", "", NULL, ""},
};

static const char *const id_and_nn_rows[][show_columns_column_count] = {
    {"id", "int", "NO", "", NULL, ""},
    {"nn", "bigint unsigned", "NO", "", NULL, ""},
};

static const char *const nn_row[][show_columns_column_count] = {
    {"nn", "bigint unsigned", "NO", "", NULL, ""},
};

static int test_show_columns_values_persistence_rename_and_drop(void);
static int test_show_columns_where_filters(void);
static int test_show_columns_diagnostics_and_unsupported_forms(void);
static int test_independent_show_columns_handles(void);
static int create_numbers_schema(mylite_db *database);
static int expect_show_columns_result(
    mylite_db *database,
    const char *sql,
    const char *const expected_rows[][show_columns_column_count],
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

    failures += test_show_columns_values_persistence_rename_and_drop();
    failures += test_show_columns_where_filters();
    failures += test_show_columns_diagnostics_and_unsupported_forms();
    failures += test_independent_show_columns_handles();

    return failures == 0 ? 0 : 1;
}

static int test_show_columns_values_persistence_rename_and_drop(void) {
    static const char *const forms[] = {
        "SHOW COLUMNS FROM numbers",
        "SHOW COLUMNS IN numbers",
        "SHOW FIELDS FROM numbers",
        "SHOW FIELDS IN numbers",
        "SHOW COLUMNS FROM app.numbers",
        "SHOW COLUMNS FROM numbers FROM app",
        "SHOW COLUMNS FROM numbers IN app",
        "SHOW COLUMNS IN numbers FROM app",
        "SHOW COLUMNS IN numbers IN app",
        "SHOW FIELDS FROM numbers FROM app",
        "SHOW FIELDS FROM numbers IN app",
        "SHOW FIELDS IN numbers FROM app",
        "SHOW FIELDS IN numbers IN app",
        "DESCRIBE numbers",
        "DESC numbers",
        "DESCRIBE app.numbers",
        "DESC app.numbers",
    };
    static const char *const trailing_schema_forms[] = {
        "SHOW COLUMNS FROM app.numbers FROM other",
        "SHOW COLUMNS FROM app.numbers IN other",
        "SHOW COLUMNS IN app.numbers FROM other",
        "SHOW COLUMNS IN app.numbers IN other",
        "SHOW FIELDS FROM app.numbers FROM other",
        "SHOW FIELDS FROM app.numbers IN other",
        "SHOW FIELDS IN app.numbers FROM other",
        "SHOW FIELDS IN app.numbers IN other",
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
    failures += create_numbers_schema(database);
    failures += execute_statement_ok(database, "CREATE TABLE other.numbers (other_id BIGINT NULL)");

    failures += expect_show_columns_result(
        database,
        "SHOW COLUMNS FROM app.numbers",
        numbers_rows,
        sizeof(numbers_rows) / sizeof(numbers_rows[0]),
        "qualified show columns without default schema"
    );
    failures += execute_statement_ok(database, "USE app");

    for (size_t form_index = 0U; form_index < sizeof(forms) / sizeof(forms[0]); ++form_index) {
        failures += expect_show_columns_result(
            database,
            forms[form_index],
            numbers_rows,
            sizeof(numbers_rows) / sizeof(numbers_rows[0]),
            forms[form_index]
        );
    }
    for (size_t form_index = 0U;
         form_index < sizeof(trailing_schema_forms) / sizeof(trailing_schema_forms[0]);
         ++form_index) {
        failures += expect_show_columns_result(
            database,
            trailing_schema_forms[form_index],
            other_numbers_rows,
            sizeof(other_numbers_rows) / sizeof(other_numbers_rows[0]),
            trailing_schema_forms[form_index]
        );
    }
    failures += expect_row_count(database, -1, "row count after introspection");
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after introspection"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_show_columns_result(
        database,
        "DESC numbers",
        numbers_rows,
        sizeof(numbers_rows) / sizeof(numbers_rows[0]),
        "reopened desc numbers"
    );

    failures += execute_statement_ok(database, "RENAME TABLE numbers TO renamed_numbers");
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.numbers' doesn't exist",
        }
    );
    failures += expect_show_columns_result(
        database,
        "SHOW COLUMNS FROM renamed_numbers",
        numbers_rows,
        sizeof(numbers_rows) / sizeof(numbers_rows[0]),
        "renamed show columns"
    );

    failures += execute_statement_ok(database, "DROP TABLE renamed_numbers");
    failures += execute_error(
        database,
        "DESC renamed_numbers",
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

static int test_show_columns_where_filters(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "where") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open where database");
    failures += create_numbers_schema(database);
    failures += execute_statement_ok(database, "USE app");

    failures += expect_show_columns_result(
        database,
        "SHOW COLUMNS FROM numbers WHERE Field = 'ID'",
        single_column_rows,
        sizeof(single_column_rows) / sizeof(single_column_rows[0]),
        "show columns where field"
    );
    failures += expect_show_columns_result(
        database,
        "SHOW FIELDS FROM numbers WHERE Type LIKE 'bigint%'",
        bigint_rows,
        sizeof(bigint_rows) / sizeof(bigint_rows[0]),
        "show fields where type like"
    );
    failures += expect_show_columns_result(
        database,
        "SHOW COLUMNS FROM numbers WHERE `Default` <=> NULL AND Field IN ('id','nn')",
        id_and_nn_rows,
        sizeof(id_and_nn_rows) / sizeof(id_and_nn_rows[0]),
        "show columns where default null-safe and in"
    );
    failures += expect_show_columns_result(
        database,
        "SHOW COLUMNS FROM numbers WHERE Field NOT IN (NULL, 'id')",
        numbers_rows,
        0U,
        "show columns where not in with null"
    );
    failures += expect_row_count(database, -1, "row count after where introspection");

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen where database");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "RENAME TABLE numbers TO renamed_numbers");
    failures += expect_show_columns_result(
        database,
        "SHOW COLUMNS FROM renamed_numbers WHERE Field = 'nn'",
        nn_row,
        sizeof(nn_row) / sizeof(nn_row[0]),
        "renamed show columns where"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_show_columns_diagnostics_and_unsupported_forms(void) {
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
        "SHOW COLUMNS FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "DESCRIBE numbers",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM missing_schema.numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM numbers FROM missing_schema",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM _mylite_catalog.numbers",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_catalog'",
        }
    );

    failures += execute_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM missing_table",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM numbers FROM other",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'other.numbers' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM _mylite_numbers",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_numbers'",
        }
    );

    failures += execute_error(
        database,
        "SHOW EXTENDED COLUMNS FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM numbers LIKE 'i%' WHERE Field = 'id'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM numbers WHERE missing = 'id'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM numbers WHERE Collation IS NULL",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'Collation' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM numbers WHERE numbers.Field = 'id'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'numbers.Field' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM numbers WHERE Field = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW COLUMNS WHERE supports only string literal predicates",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM numbers WHERE Field IN ('id', 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW COLUMNS WHERE IN supports only string and NULL literals",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM numbers WHERE Field REGEXP 'id'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW COLUMNS WHERE does not support REGEXP predicates",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM numbers WHERE Field BETWEEN 'a' AND 'z'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW COLUMNS WHERE supports output-column predicates",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM numbers WHERE Field = 'id' XOR Type = 'int'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW COLUMNS WHERE does not support XOR predicates",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLUMNS FROM numbers LIKE N'i%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "DESCRIBE numbers id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "DESCRIBE SELECT 1",
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

static int test_independent_show_columns_handles(void) {
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

    failures += expect_show_columns_result(
        first,
        "SHOW COLUMNS FROM numbers",
        single_column_rows,
        sizeof(single_column_rows) / sizeof(single_column_rows[0]),
        "first handle columns"
    );
    failures += expect_show_columns_result(
        second,
        "SHOW COLUMNS FROM numbers",
        pair_column_rows,
        sizeof(pair_column_rows) / sizeof(pair_column_rows[0]),
        "second handle columns"
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
        "b BIGINT NULL, "
        "bu BIGINT UNSIGNED NULL, "
        "nn BIGINT UNSIGNED NOT NULL)"
    );

    return failures;
}

static int expect_show_columns_result(
    mylite_db *database,
    const char *sql,
    const char *const expected_rows[][show_columns_column_count],
    size_t expected_row_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), show_columns_column_count, context);
    failures += expect_size(mylite_result_row_count(result), expected_row_count, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    for (size_t column_index = 0U; column_index < show_columns_column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            show_columns_names[column_index],
            context
        );
    }
    for (size_t row_index = 0U; row_index < expected_row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < show_columns_column_count; ++column_index) {
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
        "/tmp/mylite_show_columns_%s_%d.mylite",
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
