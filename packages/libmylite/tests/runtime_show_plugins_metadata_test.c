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
    test_path_capacity = 1024,
    show_plugins_column_count = 5,
    information_schema_plugins_column_count = 11,
    information_schema_plugins_metadata_row_count = 11,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
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

static int test_show_plugins_and_information_schema_plugins(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_error(mylite_db *database, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    return test_show_plugins_and_information_schema_plugins() == 0 ? 0 : 1;
}

static int test_show_plugins_and_information_schema_plugins(void) {
    static const char *const show_plugins_columns[show_plugins_column_count] = {
        "Name",
        "Status",
        "Type",
        "Library",
        "License",
    };
    static const char *const show_plugins_values[show_plugins_column_count] = {
        "InnoDB",
        "ACTIVE",
        "STORAGE ENGINE",
        NULL,
        "GPL",
    };
    static const char
        *const information_schema_plugins_columns[information_schema_plugins_column_count] = {
            "PLUGIN_NAME",
            "PLUGIN_VERSION",
            "PLUGIN_STATUS",
            "PLUGIN_TYPE",
            "PLUGIN_TYPE_VERSION",
            "PLUGIN_LIBRARY",
            "PLUGIN_LIBRARY_VERSION",
            "PLUGIN_AUTHOR",
            "PLUGIN_DESCRIPTION",
            "PLUGIN_LICENSE",
            "LOAD_OPTION",
        };
    static const char
        *const information_schema_plugins_values[information_schema_plugins_column_count] = {
            "InnoDB",
            "8.4",
            "ACTIVE",
            "STORAGE ENGINE",
            "80409.0",
            NULL,
            NULL,
            "Oracle Corporation",
            "Supports transactions, row-level locking, and foreign keys",
            "GPL",
            "FORCE",
        };
    static const char *const plugin_name_column[] = {"PLUGIN_NAME"};
    static const char *const plugin_name_value[] = {"InnoDB"};
    static const char *const show_row_count_value[] = {"-1"};
    static const char *const warning_count_value[] = {"0"};
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_one[] = {"1"};
    static const char *const count_zero[] = {"0"};
    static const char *const system_table_columns[] = {
        "TABLE_SCHEMA",
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
        "information_schema",
        "PLUGINS",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
        NULL,
    };
    static const char *const metadata_columns[] = {
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
        "COLUMN_KEY",
        "EXTRA",
        "PRIVILEGES",
        "COLUMN_COMMENT",
        "GENERATION_EXPRESSION",
        "SRS_ID",
    };
    static const char *const metadata_values[] = {
        "PLUGIN_NAME",
        "1",
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
        "",
        "",
        "select",
        "",
        "",
        NULL,
        "PLUGIN_VERSION",
        "2",
        "",
        "NO",
        "varchar",
        "6",
        "20",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(20)",
        "",
        "",
        "select",
        "",
        "",
        NULL,
        "PLUGIN_STATUS",
        "3",
        "",
        "NO",
        "varchar",
        "3",
        "10",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(10)",
        "",
        "",
        "select",
        "",
        "",
        NULL,
        "PLUGIN_TYPE",
        "4",
        "",
        "NO",
        "varchar",
        "26",
        "80",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(80)",
        "",
        "",
        "select",
        "",
        "",
        NULL,
        "PLUGIN_TYPE_VERSION",
        "5",
        "",
        "NO",
        "varchar",
        "6",
        "20",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(20)",
        "",
        "",
        "select",
        "",
        "",
        NULL,
        "PLUGIN_LIBRARY",
        "6",
        NULL,
        "YES",
        "varchar",
        "21",
        "64",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "",
        "",
        "select",
        "",
        "",
        NULL,
        "PLUGIN_LIBRARY_VERSION",
        "7",
        NULL,
        "YES",
        "varchar",
        "6",
        "20",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(20)",
        "",
        "",
        "select",
        "",
        "",
        NULL,
        "PLUGIN_AUTHOR",
        "8",
        NULL,
        "YES",
        "varchar",
        "21",
        "64",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "",
        "",
        "select",
        "",
        "",
        NULL,
        "PLUGIN_DESCRIPTION",
        "9",
        NULL,
        "YES",
        "varchar",
        "21845",
        "65535",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(65535)",
        "",
        "",
        "select",
        "",
        "",
        NULL,
        "PLUGIN_LICENSE",
        "10",
        NULL,
        "YES",
        "varchar",
        "26",
        "80",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(80)",
        "",
        "",
        "select",
        "",
        "",
        NULL,
        "LOAD_OPTION",
        "11",
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
        "",
        "",
        "select",
        "",
        "",
        NULL,
    };
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_db *second_database = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(first_path, &database), MYLITE_OK, "open first db");
    if (database == NULL) {
        remove_related_files(first_path);
        remove_related_files(second_path);
        return failures + 1;
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW PLUGINS",
            .column_names = show_plugins_columns,
            .column_count = show_plugins_column_count,
            .values = show_plugins_values,
            .row_count = 1U,
            .context = "show plugins row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .column_names = (const char *const[]){"ROW_COUNT()"},
            .column_count = 1U,
            .values = show_row_count_value,
            .row_count = 1U,
            .context = "row count after show plugins",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count",
            .column_names = (const char *const[]){"@@warning_count"},
            .column_count = 1U,
            .values = warning_count_value,
            .row_count = 1U,
            .context = "warning count after show plugins",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.PLUGINS",
            .column_names = information_schema_plugins_columns,
            .column_count = information_schema_plugins_column_count,
            .values = information_schema_plugins_values,
            .row_count = 1U,
            .context = "information schema plugins row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.PLUGIN_NAME FROM INFORMATION_SCHEMA.PLUGINS AS p "
                   "WHERE p.PLUGIN_NAME = 'innodb' AND p.PLUGIN_STATUS = 'active' "
                   "AND p.PLUGIN_TYPE = 'storage engine'",
            .column_names = plugin_name_column,
            .column_count = 1U,
            .values = plugin_name_value,
            .row_count = 1U,
            .context = "plugins alias case-insensitive predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT PLUGIN_NAME FROM INFORMATION_SCHEMA.PLUGINS "
                   "WHERE PLUGIN_NAME = 'InnoDB' ORDER BY PLUGIN_NAME DESC LIMIT 1",
            .column_names = plugin_name_column,
            .column_count = 1U,
            .values = plugin_name_value,
            .row_count = 1U,
            .context = "plugins ordered limited projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PLUGINS "
                   "WHERE PLUGIN_NAME = 'InnoDB'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_one,
            .row_count = 1U,
            .context = "plugins matching count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PLUGINS "
                   "WHERE PLUGIN_NAME = 'missing'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "plugins nonmatching count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'PLUGINS'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .context = "plugins system table row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, "
                   "DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "
                   "NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION, "
                   "CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, EXTRA, "
                   "PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION, SRS_ID "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'PLUGINS' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = metadata_columns,
            .column_count = sizeof(metadata_columns) / sizeof(metadata_columns[0]),
            .values = metadata_values,
            .row_count = information_schema_plugins_metadata_row_count,
            .context = "plugins system column metadata",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT nope FROM INFORMATION_SCHEMA.PLUGINS",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'field list'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT PLUGIN_NAME FROM INFORMATION_SCHEMA.PLUGINS WHERE nope = 'x'",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'where clause'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT PLUGIN_NAME FROM INFORMATION_SCHEMA.PLUGINS ORDER BY nope",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'order clause'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SHOW PLUGINS LIKE 'InnoDB'",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'LIKE'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SHOW PLUGINS WHERE Name = 'InnoDB'",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'WHERE'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SHOW FULL PLUGINS",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'PLUGINS'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SHOW PLUGINS FROM mysql",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'FROM'",
        }
    );
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    mylite_close(database);
    database = NULL;

    failures += expect_int(
        read_file_at(first_path, 0, actual_preamble, sizeof(actual_preamble)),
        0,
        "read preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "plugin metadata preamble"
    );
    failures += expect_int(mylite_open(first_path, &database), MYLITE_OK, "reopen first db");
    failures += expect_int(mylite_open(second_path, &second_database), MYLITE_OK, "open second db");
    if (database != NULL && second_database != NULL) {
        failures += expect_query(
            database,
            (struct expected_query){
                .sql = "SELECT PLUGIN_NAME FROM INFORMATION_SCHEMA.PLUGINS",
                .column_names = plugin_name_column,
                .column_count = 1U,
                .values = plugin_name_value,
                .row_count = 1U,
                .context = "reopened plugin metadata row",
            }
        );
        failures += expect_query(
            second_database,
            (struct expected_query){
                .sql = "SHOW PLUGINS",
                .column_names = show_plugins_columns,
                .column_count = show_plugins_column_count,
                .values = show_plugins_values,
                .row_count = 1U,
                .context = "second independent handle show plugins",
            }
        );
    }

    mylite_close(database);
    mylite_close(second_database);
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
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_size(mylite_result_warning_count(result), 0U, sql);
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
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
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

static int make_test_path(char *path, size_t path_size, const char *name) {
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
        "%s/mylite_show_plugins_metadata_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
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
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_size = 0U;

    if (file == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(file);
        return 1;
    }
    read_size = fread(buffer, 1U, size, file);
    if (read_size != size) {
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
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected != NULL ? expected : "(null)",
                actual != NULL ? actual : "(null)"
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected '%s' to contain '%s'\n",
            context,
            actual != NULL ? actual : "(null)",
            needle != NULL ? needle : "(null)"
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
