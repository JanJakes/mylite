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
    test_path_capacity = 1024,
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

static int test_information_schema_keywords(void);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_error(mylite_db *database, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_information_schema_keywords() == 0 ? 0 : 1;
}

static int test_information_schema_keywords(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const keyword_columns[] = {"WORD", "RESERVED"};
    static const char *const word_column[] = {"WORD"};
    static const char *const table_columns[] = {
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "VERSION",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "TABLE_COLLATION",
    };
    static const char *const columns_metadata_columns[] = {
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "COLUMN_DEFAULT",
        "IS_NULLABLE",
        "DATA_TYPE",
        "CHARACTER_MAXIMUM_LENGTH",
        "CHARACTER_OCTET_LENGTH",
        "NUMERIC_PRECISION",
        "NUMERIC_SCALE",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
        "COLUMN_TYPE",
        "PRIVILEGES",
        "SRS_ID",
    };
    static const char *const total_count[] = {"734"};
    static const char *const reserved_count[] = {"259"};
    static const char *const nonreserved_count[] = {"475"};
    static const char *const first_keyword_values[] = {
        "ACCESSIBLE",
        "1",
        "ACCOUNT",
        "0",
        "ACTION",
        "0",
    };
    static const char *const representative_values[] = {
        "DATABASE",
        "1",
        "KEY",
        "1",
        "RANDOM",
        "0",
        "SELECT",
        "1",
        "VALUE",
        "0",
        "WINDOW",
        "1",
    };
    static const char *const select_keyword[] = {"SELECT", "1"};
    static const char *const reserved_words[] = {
        "ACCESSIBLE",
        "ADD",
        "ALL",
        "ALTER",
        "ANALYZE",
    };
    static const char *const nonreserved_words[] = {
        "ACCOUNT",
        "ACTION",
        "ACTIVE",
        "ADMIN",
        "AFTER",
    };
    static const char *const last_words[] = {
        "ZONE",
        "ZEROFILL",
        "YEAR_MONTH",
    };
    static const char *const table_values[] = {
        "information_schema",
        "KEYWORDS",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        NULL,
    };
    static const char *const columns_metadata_values[] = {
        "WORD",
        "1",
        NULL,
        "YES",
        "varchar",
        "128",
        "512",
        NULL,
        NULL,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "varchar(128)",
        "select",
        NULL,
        "RESERVED",
        "2",
        NULL,
        "YES",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        "int",
        "select",
        NULL,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;
    int rc = MYLITE_OK;

    if (make_test_path(path, sizeof(path), "keywords") != 0) {
        return 1;
    }
    remove_related_files(path);

    rc = mylite_open(path, &database);

    failures += expect_int(rc, MYLITE_OK, "open file database");
    if (rc != MYLITE_OK) {
        return failures + 1;
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEYWORDS",
            .column_names = count_column,
            .column_count = 1U,
            .values = total_count,
            .row_count = 1U,
            .context = "keyword count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEYWORDS WHERE RESERVED = 1",
            .column_names = count_column,
            .column_count = 1U,
            .values = reserved_count,
            .row_count = 1U,
            .context = "reserved keyword count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEYWORDS WHERE RESERVED = 0",
            .column_names = count_column,
            .column_count = 1U,
            .values = nonreserved_count,
            .row_count = 1U,
            .context = "nonreserved keyword count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT WORD, RESERVED FROM INFORMATION_SCHEMA.KEYWORDS "
                   "ORDER BY WORD LIMIT 3",
            .column_names = keyword_columns,
            .column_count = 2U,
            .values = first_keyword_values,
            .row_count = 3U,
            .context = "first keyword rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT WORD, RESERVED FROM INFORMATION_SCHEMA.KEYWORDS "
                   "WHERE WORD = 'DATABASE' OR WORD = 'KEY' OR WORD = 'RANDOM' "
                   "OR WORD = 'SELECT' OR WORD = 'VALUE' OR WORD = 'WINDOW' "
                   "ORDER BY WORD",
            .column_names = keyword_columns,
            .column_count = 2U,
            .values = representative_values,
            .row_count = sizeof(representative_values) / sizeof(representative_values[0]) / 2U,
            .context = "representative keyword rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT k.WORD, k.RESERVED FROM INFORMATION_SCHEMA.KEYWORDS AS k "
                   "WHERE k.WORD = 'select'",
            .column_names = keyword_columns,
            .column_count = 2U,
            .values = select_keyword,
            .row_count = 1U,
            .context = "case-insensitive word predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT WORD FROM INFORMATION_SCHEMA.KEYWORDS "
                   "WHERE RESERVED = TRUE ORDER BY WORD LIMIT 5",
            .column_names = word_column,
            .column_count = 1U,
            .values = reserved_words,
            .row_count = sizeof(reserved_words) / sizeof(reserved_words[0]),
            .context = "reserved keyword ordering",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT WORD FROM INFORMATION_SCHEMA.KEYWORDS "
                   "WHERE RESERVED = FALSE ORDER BY WORD LIMIT 5",
            .column_names = word_column,
            .column_count = 1U,
            .values = nonreserved_words,
            .row_count = sizeof(nonreserved_words) / sizeof(nonreserved_words[0]),
            .context = "nonreserved keyword ordering",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT WORD FROM INFORMATION_SCHEMA.KEYWORDS ORDER BY WORD DESC LIMIT 3",
            .column_names = word_column,
            .column_count = 1U,
            .values = last_words,
            .row_count = 3U,
            .context = "descending keyword ordering",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, "
                   "ROW_FORMAT, TABLE_ROWS, TABLE_COLLATION FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'KEYWORDS'",
            .column_names = table_columns,
            .column_count = sizeof(table_columns) / sizeof(table_columns[0]),
            .values = table_values,
            .row_count = 1U,
            .context = "keywords system table row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, "
                   "DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "
                   "NUMERIC_PRECISION, NUMERIC_SCALE, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE, PRIVILEGES, SRS_ID FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'KEYWORDS' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = 2U,
            .context = "keywords system column rows",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT WORD FROM INFORMATION_SCHEMA.KEYWORDS WHERE MISSING = 1",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'MISSING' in 'where clause'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT WORD FROM INFORMATION_SCHEMA.KEYWORDS WHERE WORD < 'S'",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "INFORMATION_SCHEMA WHERE string predicates support only equality comparisons",
        }
    );
    mylite_close(database);

    rc = mylite_open(path, &database);
    failures += expect_int(rc, MYLITE_OK, "reopen file database");
    if (rc == MYLITE_OK) {
        failures += expect_query(
            database,
            (struct expected_query){
                .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEYWORDS WHERE RESERVED = 1",
                .column_names = count_column,
                .column_count = 1U,
                .values = reserved_count,
                .row_count = 1U,
                .context = "reopened file handle keyword rows",
            }
        );
        mylite_close(database);
    }
    remove_related_files(path);

    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    failures += expect_int(rc, MYLITE_OK, expected.context);
    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: errcode %d sqlstate %s message %s\n",
            expected.context,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return failures + 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.sql);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.sql);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.sql);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.sql);

    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.sql
        );
    }
    for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            size_t value_index = (row_index * expected.column_count) + column_index;

            failures += expect_text_or_null(
                mylite_result_value_text(result, row_index, column_index),
                expected.values[value_index],
                expected.sql
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
        "%s/mylite_information_schema_keywords_%d_%s.mylite",
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
            fprintf(stderr, "%s: expected %s, got %s\n", context, expected, actual);
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
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing '%s', got '%s'\n",
            context,
            needle,
            actual
        );
        return 1;
    }
    return 0;
}
