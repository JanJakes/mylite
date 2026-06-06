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
    system_columns_metadata_column_count = 8,
    columns_extensions_metadata_row_count = 6,
    tables_extensions_metadata_row_count = 5,
    table_constraints_extensions_metadata_row_count = 6,
    tables_extensions_metadata_offset =
        columns_extensions_metadata_row_count * system_columns_metadata_column_count,
    table_constraints_extensions_metadata_offset =
        (columns_extensions_metadata_row_count + tables_extensions_metadata_row_count) *
        system_columns_metadata_column_count,
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

static int test_information_schema_extension_attribute_tables_queries(void);
static int seed_database(mylite_db *database);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_error(mylite_db *database, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_information_schema_extension_attribute_tables_queries() == 0 ? 0 : 1;
}

static int test_information_schema_extension_attribute_tables_queries(void) {
    static const char *const extension_columns[] = {
        "TABLE_CATALOG",
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "ENGINE_ATTRIBUTE",
        "SECONDARY_ENGINE_ATTRIBUTE",
    };
    static const char *const table_values[] = {
        "def",
        "app",
        "child",
        NULL,
        NULL,
        "def",
        "app",
        "parent",
        NULL,
        NULL,
        "def",
        "app",
        "v_parent",
        NULL,
        NULL,
    };
    static const char *const column_extension_columns[] = {
        "TABLE_CATALOG",
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "COLUMN_NAME",
        "ENGINE_ATTRIBUTE",
        "SECONDARY_ENGINE_ATTRIBUTE",
    };
    static const char *const column_values[] = {
        "def",
        "app",
        "parent",
        "id",
        NULL,
        NULL,
        "def",
        "app",
        "parent",
        "v",
        NULL,
        NULL,
    };
    static const char *const constraint_extension_columns[] = {
        "CONSTRAINT_CATALOG",
        "CONSTRAINT_SCHEMA",
        "CONSTRAINT_NAME",
        "TABLE_NAME",
        "ENGINE_ATTRIBUTE",
        "SECONDARY_ENGINE_ATTRIBUTE",
    };
    static const char *const constraint_values[] = {
        "def",
        "app",
        "PRIMARY",
        "parent",
        NULL,
        NULL,
        "def",
        "app",
        "uq_parent_v",
        "parent",
        NULL,
        NULL,
    };
    static const char *const foreign_key_constraint_values[] = {
        "def",
        "app",
        "fk_child_parent",
        "child",
        NULL,
        NULL,
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const count_two[] = {"2"};
    static const char *const count_seventeen[] = {"17"};
    static const char *const builtin_table_columns[] = {"TABLE_SCHEMA", "TABLE_NAME"};
    static const char *const builtin_table_values[] = {
        "information_schema",
        "COLUMNS_EXTENSIONS",
        "information_schema",
        "TABLES_EXTENSIONS",
        "information_schema",
        "TABLE_CONSTRAINTS_EXTENSIONS",
    };
    static const char *const system_column_extension_columns[] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "ENGINE_ATTRIBUTE",
        "SECONDARY_ENGINE_ATTRIBUTE",
    };
    static const char *const system_column_extension_values[] = {
        "TABLES_EXTENSIONS",
        "ENGINE_ATTRIBUTE",
        NULL,
        NULL,
    };
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
        "COLUMNS_EXTENSIONS",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
        "TABLES_EXTENSIONS",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
        "TABLE_CONSTRAINTS_EXTENSIONS",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
    };
    static const char *const columns_metadata_columns[] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "IS_NULLABLE",
        "DATA_TYPE",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
        "COLUMN_TYPE",
    };
    static const char *const system_columns_metadata_values[] = {
        "COLUMNS_EXTENSIONS",
        "TABLE_CATALOG",
        "1",
        "NO",
        "varchar",
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "COLUMNS_EXTENSIONS",
        "TABLE_SCHEMA",
        "2",
        "NO",
        "varchar",
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "COLUMNS_EXTENSIONS",
        "TABLE_NAME",
        "3",
        "NO",
        "varchar",
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "COLUMNS_EXTENSIONS",
        "COLUMN_NAME",
        "4",
        "YES",
        "varchar",
        "utf8mb3",
        "utf8mb3_tolower_ci",
        "varchar(64)",
        "COLUMNS_EXTENSIONS",
        "ENGINE_ATTRIBUTE",
        "5",
        "YES",
        "json",
        NULL,
        NULL,
        "json",
        "COLUMNS_EXTENSIONS",
        "SECONDARY_ENGINE_ATTRIBUTE",
        "6",
        "YES",
        "json",
        NULL,
        NULL,
        "json",
        "TABLES_EXTENSIONS",
        "TABLE_CATALOG",
        "1",
        "NO",
        "varchar",
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "TABLES_EXTENSIONS",
        "TABLE_SCHEMA",
        "2",
        "NO",
        "varchar",
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "TABLES_EXTENSIONS",
        "TABLE_NAME",
        "3",
        "NO",
        "varchar",
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "TABLES_EXTENSIONS",
        "ENGINE_ATTRIBUTE",
        "4",
        "YES",
        "json",
        NULL,
        NULL,
        "json",
        "TABLES_EXTENSIONS",
        "SECONDARY_ENGINE_ATTRIBUTE",
        "5",
        "YES",
        "json",
        NULL,
        NULL,
        "json",
        "TABLE_CONSTRAINTS_EXTENSIONS",
        "CONSTRAINT_CATALOG",
        "1",
        "NO",
        "varchar",
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "TABLE_CONSTRAINTS_EXTENSIONS",
        "CONSTRAINT_SCHEMA",
        "2",
        "NO",
        "varchar",
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "TABLE_CONSTRAINTS_EXTENSIONS",
        "CONSTRAINT_NAME",
        "3",
        "NO",
        "varchar",
        "utf8mb3",
        "utf8mb3_tolower_ci",
        "varchar(64)",
        "TABLE_CONSTRAINTS_EXTENSIONS",
        "TABLE_NAME",
        "4",
        "NO",
        "varchar",
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "TABLE_CONSTRAINTS_EXTENSIONS",
        "ENGINE_ATTRIBUTE",
        "5",
        "YES",
        "json",
        NULL,
        NULL,
        "json",
        "TABLE_CONSTRAINTS_EXTENSIONS",
        "SECONDARY_ENGINE_ATTRIBUTE",
        "6",
        "YES",
        "json",
        NULL,
        NULL,
        "json",
    };
    static const char *const table_name_column[] = {"TABLE_NAME"};
    static const char *const parent_table_value[] = {"parent"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path)) != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open database");
    failures += seed_database(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, ENGINE_ATTRIBUTE, "
                   "SECONDARY_ENGINE_ATTRIBUTE FROM INFORMATION_SCHEMA.TABLES_EXTENSIONS "
                   "WHERE TABLE_SCHEMA = 'app' ORDER BY TABLE_NAME",
            .column_names = extension_columns,
            .column_count = sizeof(extension_columns) / sizeof(extension_columns[0]),
            .values = table_values,
            .row_count = sizeof(table_values) / sizeof(table_values[0]) /
                         (sizeof(extension_columns) / sizeof(extension_columns[0])),
            .context = "tables extensions app rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, "
                   "ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE "
                   "FROM INFORMATION_SCHEMA.COLUMNS_EXTENSIONS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'parent' ORDER BY COLUMN_NAME",
            .column_names = column_extension_columns,
            .column_count = sizeof(column_extension_columns) / sizeof(column_extension_columns[0]),
            .values = column_values,
            .row_count = sizeof(column_values) / sizeof(column_values[0]) /
                         (sizeof(column_extension_columns) / sizeof(column_extension_columns[0])),
            .context = "columns extensions app rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_CATALOG, CONSTRAINT_SCHEMA, CONSTRAINT_NAME, "
                   "TABLE_NAME, ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE "
                   "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "
                   "WHERE CONSTRAINT_SCHEMA = 'app' AND TABLE_NAME = 'parent' "
                   "ORDER BY CONSTRAINT_NAME",
            .column_names = constraint_extension_columns,
            .column_count =
                sizeof(constraint_extension_columns) / sizeof(constraint_extension_columns[0]),
            .values = constraint_values,
            .row_count =
                sizeof(constraint_values) / sizeof(constraint_values[0]) /
                (sizeof(constraint_extension_columns) / sizeof(constraint_extension_columns[0])),
            .context = "table constraints extensions app rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS_EXTENSIONS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'v_parent'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_two,
            .row_count = 1U,
            .context = "columns extensions view row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_CATALOG, CONSTRAINT_SCHEMA, CONSTRAINT_NAME, "
                   "TABLE_NAME, ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE "
                   "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "
                   "WHERE CONSTRAINT_SCHEMA = 'app' AND CONSTRAINT_NAME = 'fk_child_parent'",
            .column_names = constraint_extension_columns,
            .column_count =
                sizeof(constraint_extension_columns) / sizeof(constraint_extension_columns[0]),
            .values = foreign_key_constraint_values,
            .row_count = 1U,
            .context = "table constraints extensions foreign key row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "
                   "WHERE CONSTRAINT_SCHEMA = 'app' AND CONSTRAINT_NAME = 'chk_parent_v'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "table constraints extensions omit checks",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME FROM INFORMATION_SCHEMA.TABLES_EXTENSIONS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME IN ('COLUMNS_EXTENSIONS', 'TABLES_EXTENSIONS', "
                   "'TABLE_CONSTRAINTS_EXTENSIONS') ORDER BY TABLE_NAME",
            .column_names = builtin_table_columns,
            .column_count = sizeof(builtin_table_columns) / sizeof(builtin_table_columns[0]),
            .values = builtin_table_values,
            .row_count = sizeof(builtin_table_values) / sizeof(builtin_table_values[0]) /
                         (sizeof(builtin_table_columns) / sizeof(builtin_table_columns[0])),
            .context = "tables extensions built-in directory rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS_EXTENSIONS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME IN ('COLUMNS_EXTENSIONS', 'TABLES_EXTENSIONS', "
                   "'TABLE_CONSTRAINTS_EXTENSIONS')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_seventeen,
            .row_count = 1U,
            .context = "columns extensions system row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ENGINE_ATTRIBUTE, "
                   "SECONDARY_ENGINE_ATTRIBUTE FROM INFORMATION_SCHEMA.COLUMNS_EXTENSIONS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'TABLES_EXTENSIONS' "
                   "AND COLUMN_NAME = 'ENGINE_ATTRIBUTE'",
            .column_names = system_column_extension_columns,
            .column_count = sizeof(system_column_extension_columns) /
                            sizeof(system_column_extension_columns[0]),
            .values = system_column_extension_values,
            .row_count = 1U,
            .context = "columns extensions system column row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "DATA_LENGTH FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME IN ('COLUMNS_EXTENSIONS', 'TABLES_EXTENSIONS', "
                   "'TABLE_CONSTRAINTS_EXTENSIONS') ORDER BY TABLE_NAME",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = sizeof(system_table_values) / sizeof(system_table_values[0]) /
                         (sizeof(system_table_columns) / sizeof(system_table_columns[0])),
            .context = "extension attribute system table metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, DATA_TYPE, "
                   "CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'COLUMNS_EXTENSIONS' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = system_columns_metadata_values,
            .row_count = (size_t)columns_extensions_metadata_row_count,
            .context = "columns extensions system columns metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, DATA_TYPE, "
                   "CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'TABLES_EXTENSIONS' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = &system_columns_metadata_values[(size_t)tables_extensions_metadata_offset],
            .row_count = (size_t)tables_extensions_metadata_row_count,
            .context = "tables extensions system columns metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, DATA_TYPE, "
                   "CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'TABLE_CONSTRAINTS_EXTENSIONS' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = &system_columns_metadata_values[(size_t
            )table_constraints_extensions_metadata_offset],
            .row_count = (size_t)table_constraints_extensions_metadata_row_count,
            .context = "table constraints extensions system columns metadata",
        }
    );
    failures += expect_statement_ok(database, "USE information_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME FROM TABLES_EXTENSIONS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'parent'",
            .column_names = table_name_column,
            .column_count = 1U,
            .values = parent_table_value,
            .row_count = 1U,
            .context = "selected information_schema tables extensions",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES_EXTENSIONS "
                   "WHERE nope = 'x'",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'where clause'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int seed_database(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE parent (id INT PRIMARY KEY, v INT, UNIQUE KEY uq_parent_v (v), "
        "CONSTRAINT chk_parent_v CHECK (v > 0))"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE child (id INT PRIMARY KEY, parent_id INT, "
        "CONSTRAINT fk_child_parent FOREIGN KEY (parent_id) REFERENCES parent(id))"
    );
    failures += expect_statement_ok(database, "CREATE VIEW v_parent AS SELECT id, v FROM parent");

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
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
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

    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
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
    failures += expect_int(mylite_errcode(database), expected.code, expected.sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, expected.sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, expected.sql);
    failures += expect_size(mylite_result_column_count(result), 0U, expected.sql);
    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }
    written = snprintf(
        path,
        path_size,
        "%s/mylite_information_schema_extension_attribute_tables_%d.mylite",
        directory,
        current_process_id()
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long\n");
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
    (void)remove(related);
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

static int expect_contains(const char *actual, const char *needle, const char *context) {
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
