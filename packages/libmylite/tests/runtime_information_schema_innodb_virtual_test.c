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

#ifndef P_tmpdir
#  define P_tmpdir "."
#endif

enum {
    test_path_capacity = 1024,
    test_path_suffix_capacity = 16,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

struct expected_statement {
    const char *sql;
    const char *context;
};

static int test_information_schema_innodb_virtual_rows(void);
static int setup_virtual_schema(mylite_db *database);
static int expect_statement_ok(mylite_db *database, struct expected_statement expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);

int main(void) {
    int failures = 0;

    failures += test_information_schema_innodb_virtual_rows();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_innodb_virtual_rows(void) {
    static const char *const table_columns[] = {
        "TABLE_ID",
        "POS",
        "BASE_POS",
    };
    static const char *const table_values[] = {
        "1", "65539",  "1", "1", "65539",  "2", "1", "196614", "1", "1", "327688", "1",
        "1", "327688", "2", "2", "65539",  "1", "2", "65539",  "2", "2", "131076", "1",
        "2", "196613", "0", "2", "196613", "1", "2", "196613", "2", "3", "65538",  "0",
        "3", "65538",  "1", "4", "65538",  "0", "4", "65538",  "1", "4", "131075", "1",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_sixteen[] = {"16"};
    static const char *const count_three[] = {"3"};
    static const char *const count_nine[] = {"9"};
    static const char *const alias_columns[] = {"TABLE_ID", "POS", "BASE_POS"};
    static const char *const alias_values[] = {"2", "131076", "1", "2", "196613", "1"};
    static const char *const persisted_values[] = {"1", "196614", "1", "4", "131075", "1"};
    static const char *const system_table_columns[] = {
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "VERSION",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "DATA_LENGTH",
        "AUTO_INCREMENT",
    };
    static const char *const system_table_values[] = {
        "INNODB_VIRTUAL",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
        NULL,
    };
    static const char *const columns_metadata_columns[] = {
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
    static const char *const columns_metadata_values[] = {
        "INNODB_VIRTUAL",
        "TABLE_ID",
        "1",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_VIRTUAL",
        "POS",
        "2",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int unsigned",
        "select",
        "INNODB_VIRTUAL",
        "BASE_POS",
        "3",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int unsigned",
        "select",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "rows") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open innodb virtual db");
    failures += setup_virtual_schema(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT v.TABLE_ID, v.POS, v.BASE_POS "
                   "FROM INFORMATION_SCHEMA.INNODB_VIRTUAL AS v "
                   "ORDER BY v.TABLE_ID",
            .column_names = table_columns,
            .column_count = sizeof(table_columns) / sizeof(table_columns[0]),
            .values = table_values,
            .row_count = sizeof(table_values) / sizeof(table_values[0]) /
                         (sizeof(table_columns) / sizeof(table_columns[0])),
            .context = "innodb virtual dependency rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_virtual",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_sixteen,
            .row_count = 1U,
            .context = "case-insensitive innodb virtual count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_VIRTUAL "
                   "WHERE TABLE_ID = '2' AND BASE_POS = '1'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_three,
            .row_count = 1U,
            .context = "innodb virtual predicate count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT v.TABLE_ID, v.POS, v.BASE_POS "
                   "FROM INFORMATION_SCHEMA.INNODB_VIRTUAL AS v "
                   "WHERE v.TABLE_ID = '2' AND v.BASE_POS = '1' "
                   "AND v.POS IN ('131076', '196613') "
                   "ORDER BY v.POS",
            .column_names = alias_columns,
            .column_count = sizeof(alias_columns) / sizeof(alias_columns[0]),
            .values = alias_values,
            .row_count = sizeof(alias_values) / sizeof(alias_values[0]) /
                         (sizeof(alias_columns) / sizeof(alias_columns[0])),
            .context = "innodb virtual alias predicate rows",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE information_schema",
            .context = "use information_schema for innodb virtual",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INNODB_VIRTUAL WHERE BASE_POS = '1'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_nine,
            .row_count = 1U,
            .context = "unqualified innodb virtual base-position count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'INNODB_VIRTUAL'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .context = "innodb virtual system table row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, "
                   "DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'INNODB_VIRTUAL' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .context = "innodb virtual columns metadata",
        }
    );
    failures += expect_row_count_status(database, "innodb virtual status");

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen innodb virtual db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT v.TABLE_ID, v.POS, v.BASE_POS "
                   "FROM INFORMATION_SCHEMA.INNODB_VIRTUAL "
                   "AS v WHERE v.POS IN ('196614', '131075') "
                   "ORDER BY v.TABLE_ID",
            .column_names = table_columns,
            .column_count = sizeof(table_columns) / sizeof(table_columns[0]),
            .values = persisted_values,
            .row_count = sizeof(persisted_values) / sizeof(persisted_values[0]) /
                         (sizeof(table_columns) / sizeof(table_columns[0])),
            .context = "innodb virtual dependency rows persist",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int setup_virtual_schema(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE SCHEMA app",
            .context = "create innodb virtual schema",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE app",
            .context = "use innodb virtual schema",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE TABLE generated_values ("
                   "id INT PRIMARY KEY, "
                   "a INT, "
                   "b INT, "
                   "c INT GENERATED ALWAYS AS (a + b) VIRTUAL, "
                   "d INT GENERATED ALWAYS AS (a * 2) STORED, "
                   "e INT GENERATED ALWAYS AS (5) VIRTUAL, "
                   "f INT GENERATED ALWAYS AS (a) VIRTUAL, "
                   "g INT GENERATED ALWAYS AS (NULL) VIRTUAL, "
                   "h INT GENERATED ALWAYS AS (-a + b) VIRTUAL)",
            .context = "create generated_values for innodb virtual",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE TABLE generated_order ("
                   "id INT PRIMARY KEY, "
                   "a INT, "
                   "b INT, "
                   "c INT GENERATED ALWAYS AS (b + a + b) VIRTUAL, "
                   "d INT GENERATED ALWAYS AS (a + a) VIRTUAL, "
                   "e INT GENERATED ALWAYS AS ((id + b) * (a - id)) VIRTUAL, "
                   "f INT GENERATED ALWAYS AS (5) VIRTUAL, "
                   "g INT GENERATED ALWAYS AS (NULL) VIRTUAL)",
            .context = "create generated_order for innodb virtual",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE TABLE first_virtual ("
                   "id INT, "
                   "a INT, "
                   "b INT GENERATED ALWAYS AS (a + id) VIRTUAL)",
            .context = "create first_virtual for innodb virtual",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE TABLE second_virtual ("
                   "id INT, "
                   "a INT, "
                   "b INT GENERATED ALWAYS AS (a + id) VIRTUAL, "
                   "c INT GENERATED ALWAYS AS (a) VIRTUAL)",
            .context = "create second_virtual for innodb virtual",
        }
    );

    return failures;
}

static int expect_statement_ok(mylite_db *database, struct expected_statement expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    failures += expect_int(rc, MYLITE_OK, expected.context);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    } else {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    failures += expect_int(rc, MYLITE_OK, expected.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
        mylite_result_free(result);
        return failures + 1;
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
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    mylite_result_free(result);
    return failures;
}

static int expect_row_count_status(mylite_db *database, const char *context) {
    static const char *const status_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const status_values[] = {"0", "-1"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .column_names = status_columns,
            .column_count = 2U,
            .values = status_values,
            .row_count = 1U,
            .context = context,
        }
    );
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "%s/mylite_information_schema_innodb_virtual_%d_%s.mylite",
        P_tmpdir,
        current_process_id(),
        name
    );

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
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
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written > 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
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
