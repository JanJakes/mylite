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

struct expected_statement {
    const char *sql;
    const char *context;
};

static int test_information_schema_innodb_tablestats_rows(void);
static int setup_tablestats_schema(mylite_db *database);
static int expect_statement_ok(mylite_db *database, struct expected_statement expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);

int main(void) {
    int failures = 0;

    failures += test_information_schema_innodb_tablestats_rows();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_innodb_tablestats_rows(void) {
    static const char *const table_columns[] = {
        "TABLE_ID",
        "NAME",
        "STATS_INITIALIZED",
        "NUM_ROWS",
        "CLUST_INDEX_SIZE",
        "OTHER_INDEX_SIZE",
        "MODIFIED_COUNTER",
        "AUTOINC",
        "REF_COUNT",
    };
    static const char *const table_values[] = {
        "3", "app/t_auto",    "Initialized", "2", "1", "0", "0", "3", "1",
        "2", "app/t_no_pk",   "Initialized", "2", "1", "0", "0", "0", "1",
        "1", "app/t_primary", "Initialized", "3", "1", "2", "0", "0", "1",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_three[] = {"3"};
    static const char *const count_one[] = {"1"};
    static const char *const alias_columns[] = {"NAME", "NUM_ROWS", "OTHER_INDEX_SIZE"};
    static const char *const alias_values[] = {"app/t_primary", "3", "2"};
    static const char *const updated_row_columns[] = {"NAME", "NUM_ROWS"};
    static const char *const updated_row_values[] = {"app/t_no_pk", "3"};
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
        "INNODB_TABLESTATS",
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
        "INNODB_TABLESTATS",
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
        "INNODB_TABLESTATS",
        "NAME",
        "2",
        "",
        "NO",
        "varchar",
        "64",
        "193",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(193)",
        "select",
        "INNODB_TABLESTATS",
        "STATS_INITIALIZED",
        "3",
        "",
        "NO",
        "varchar",
        "64",
        "193",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(193)",
        "select",
        "INNODB_TABLESTATS",
        "NUM_ROWS",
        "4",
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
        "INNODB_TABLESTATS",
        "CLUST_INDEX_SIZE",
        "5",
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
        "INNODB_TABLESTATS",
        "OTHER_INDEX_SIZE",
        "6",
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
        "INNODB_TABLESTATS",
        "MODIFIED_COUNTER",
        "7",
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
        "INNODB_TABLESTATS",
        "AUTOINC",
        "8",
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
        "INNODB_TABLESTATS",
        "REF_COUNT",
        "9",
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
        "int",
        "select",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "rows") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open innodb tablestats db");
    failures += setup_tablestats_schema(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_ID, NAME, STATS_INITIALIZED, NUM_ROWS, CLUST_INDEX_SIZE, "
                   "OTHER_INDEX_SIZE, MODIFIED_COUNTER, AUTOINC, REF_COUNT "
                   "FROM INFORMATION_SCHEMA.INNODB_TABLESTATS ORDER BY NAME",
            .column_names = table_columns,
            .column_count = sizeof(table_columns) / sizeof(table_columns[0]),
            .values = table_values,
            .row_count = sizeof(table_values) / sizeof(table_values[0]) /
                         (sizeof(table_columns) / sizeof(table_columns[0])),
            .context = "innodb tablestats rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TABLESTATS "
                   "WHERE NAME LIKE 'app/%'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_three,
            .row_count = 1U,
            .context = "innodb tablestats count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT s.NAME, s.NUM_ROWS, s.OTHER_INDEX_SIZE "
                   "FROM INFORMATION_SCHEMA.INNODB_TABLESTATS AS s "
                   "WHERE s.OTHER_INDEX_SIZE = 2",
            .column_names = alias_columns,
            .column_count = sizeof(alias_columns) / sizeof(alias_columns[0]),
            .values = alias_values,
            .row_count = 1U,
            .context = "innodb tablestats alias predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_tablestats "
                   "WHERE NAME LIKE 'app/%'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_three,
            .row_count = 1U,
            .context = "case-insensitive innodb tablestats table",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE information_schema",
            .context = "use information_schema for innodb tablestats",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INNODB_TABLESTATS WHERE NAME LIKE 'app/%'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_three,
            .row_count = 1U,
            .context = "unqualified innodb tablestats count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'INNODB_TABLESTATS'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .context = "innodb tablestats system table row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, "
                   "DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, "
                   "PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'INNODB_TABLESTATS' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .context = "innodb tablestats columns metadata",
        }
    );
    failures += expect_row_count_status(database, "innodb tablestats row count status");

    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE app",
            .context = "use app for innodb tablestats changes",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "INSERT INTO t_no_pk VALUES (5,6)",
            .context = "insert tablestats row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NAME, NUM_ROWS FROM INFORMATION_SCHEMA.INNODB_TABLESTATS "
                   "WHERE NAME = 'app/t_no_pk'",
            .column_names = updated_row_columns,
            .column_count = sizeof(updated_row_columns) / sizeof(updated_row_columns[0]),
            .values = updated_row_values,
            .row_count = 1U,
            .context = "innodb tablestats row count after insert",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen innodb tablestats db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TABLESTATS "
                   "WHERE NAME LIKE 'app/%'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_three,
            .row_count = 1U,
            .context = "reopened innodb tablestats count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TABLESTATS "
                   "WHERE NAME = 'app/t_no_pk' AND NUM_ROWS = 3",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_one,
            .row_count = 1U,
            .context = "reopened innodb tablestats row count",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int setup_tablestats_schema(mylite_db *database) {
    static const struct expected_statement statements[] = {
        {.sql = "CREATE DATABASE app", .context = "create app schema"},
        {.sql = "USE app", .context = "use app schema"},
        {.sql = "CREATE TABLE t_primary("
                "id INT PRIMARY KEY, v INT, KEY ix_v(v), KEY ix_v_id(v,id))",
         .context = "create primary indexed table"},
        {.sql = "CREATE TABLE t_no_pk(a INT, b INT)", .context = "create no-primary table"},
        {.sql = "CREATE TABLE t_auto(id INT AUTO_INCREMENT PRIMARY KEY, v INT)",
         .context = "create auto increment table"},
        {.sql = "INSERT INTO t_primary VALUES (1,10),(2,20),(3,20)",
         .context = "insert primary rows"},
        {.sql = "INSERT INTO t_no_pk VALUES (1,2),(3,4)", .context = "insert no-primary rows"},
        {.sql = "INSERT INTO t_auto(v) VALUES (7),(8)", .context = "insert auto rows"},
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_ok(database, statements[index]);
    }
    return failures;
}

static int expect_statement_ok(mylite_db *database, struct expected_statement expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            expected.context,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }

    mylite_result_free(result);
    return 0;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            expected.context,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (result == NULL) {
        fprintf(stderr, "%s: expected result object\n", expected.context);
        return 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            size_t value_index = (row_index * expected.column_count) + column_index;

            failures += expect_text_or_null(
                mylite_result_value_text(result, row_index, column_index),
                expected.values[value_index],
                expected.context
            );
        }
    }

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
        "/tmp/mylite_information_schema_innodb_tablestats_%s_%d.mylite",
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
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written > 0 && (size_t)written < sizeof(related)) {
        remove(related);
    }
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
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected text %s, got %s\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}
