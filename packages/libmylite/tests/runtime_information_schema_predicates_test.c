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
    test_path_suffix_capacity = 16,
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

static int test_information_schema_predicates(void);
static int seed_database(mylite_db *database);
static int expect_statement_ok(mylite_db *database, const char *sql, int64_t affected_rows);
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
    return test_information_schema_predicates() == 0 ? 0 : 1;
}

static int test_information_schema_predicates(void) {
    static const char *const table_name_column[] = {"TABLE_NAME"};
    static const char *const column_name_column[] = {"COLUMN_NAME"};
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const literal_column[] = {"1"};
    static const char *const id_value[] = {"id"};
    static const char *const t_value[] = {"t"};
    static const char *const wp_options_value[] = {"wp_options"};
    static const char *const wp_wildcard_values[] = {"wp_options", "wpa"};
    static const char *const id_v_values[] = {"id", "v"};
    static const char *const n_value[] = {"n"};
    static const char *const id_n_values[] = {"id", "n"};
    static const char *const t_wp_options_values[] = {"t", "wp_options"};
    static const char *const after_reopen_t_values[] = {"after_reopen", "t"};
    static const char *const count_zero[] = {"0"};
    static const char *const count_one[] = {"1"};
    static const char *const literal_one[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "predicates") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open information schema db");
    failures += seed_database(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND COLUMN_NAME LIKE 'ID%'",
            .column_names = column_name_column,
            .column_count = 1U,
            .values = id_value,
            .row_count = 1U,
            .context = "like case-insensitive column name",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME LIKE 'wp\\\\_%' "
                   "ORDER BY TABLE_NAME",
            .column_names = table_name_column,
            .column_count = 1U,
            .values = wp_options_value,
            .row_count = 1U,
            .context = "like escaped wildcard",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME LIKE 'wp\\\\_%' "
                   "ESCAPE '\\\\' ORDER BY TABLE_NAME",
            .column_names = table_name_column,
            .column_count = 1U,
            .values = wp_options_value,
            .row_count = 1U,
            .context = "like explicit backslash escape",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME LIKE 'wp`_%' "
                   "ESCAPE '`' ORDER BY TABLE_NAME",
            .column_names = table_name_column,
            .column_count = 1U,
            .values = wp_options_value,
            .row_count = 1U,
            .context = "like explicit backtick escape",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME LIKE 'wp_%' "
                   "ORDER BY TABLE_NAME",
            .column_names = table_name_column,
            .column_count = 1U,
            .values = wp_wildcard_values,
            .row_count = 2U,
            .context = "like wildcard",
        }
    );
    failures += expect_statement_ok(database, "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'", -1);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME LIKE 'wp\\\\_%' "
                   "ORDER BY TABLE_NAME",
            .column_names = table_name_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "no backslash escapes like pattern",
        }
    );
    failures += expect_statement_ok(database, "SET SESSION sql_mode = ''", -1);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME NOT LIKE 'wp%' "
                   "ORDER BY TABLE_NAME",
            .column_names = table_name_column,
            .column_count = 1U,
            .values = t_value,
            .row_count = 1U,
            .context = "not like filters false and true values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT c.COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS AS c "
                   "WHERE c.TABLE_SCHEMA = 'app' AND c.TABLE_NAME = 't' "
                   "AND c.COLUMN_NAME LIKE 'ID%'",
            .column_names = column_name_column,
            .column_count = 1U,
            .values = id_value,
            .row_count = 1U,
            .context = "alias-qualified predicate columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' "
                   "AND COLUMN_NAME IN ('ID', 'v', 'missing') ORDER BY ORDINAL_POSITION",
            .column_names = column_name_column,
            .column_count = 1U,
            .values = id_v_values,
            .row_count = 2U,
            .context = "text in list",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' "
                   "AND COLUMN_NAME NOT IN ('id', 'v') ORDER BY ORDINAL_POSITION",
            .column_names = column_name_column,
            .column_count = 1U,
            .values = n_value,
            .row_count = 1U,
            .context = "text not in list",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' "
                   "AND COLUMN_NAME NOT IN ('missing', NULL) ORDER BY ORDINAL_POSITION",
            .column_names = column_name_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "not in null remains unknown",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME BETWEEN 't' AND 'wp_options' "
                   "ORDER BY TABLE_NAME",
            .column_names = table_name_column,
            .column_count = 1U,
            .values = t_wp_options_values,
            .row_count = 2U,
            .context = "text between uses metadata collation ordering",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' "
                   "AND ORDINAL_POSITION BETWEEN 1 AND 2 ORDER BY ORDINAL_POSITION",
            .column_names = column_name_column,
            .column_count = 1U,
            .values = id_v_values,
            .row_count = 2U,
            .context = "numeric between",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' "
                   "AND ORDINAL_POSITION IN ('01', 3) ORDER BY ORDINAL_POSITION",
            .column_names = column_name_column,
            .column_count = 1U,
            .values = id_n_values,
            .row_count = 2U,
            .context = "numeric in string coercion",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' "
                   "AND ORDINAL_POSITION NOT BETWEEN 1 AND 2 ORDER BY ORDINAL_POSITION",
            .column_names = column_name_column,
            .column_count = 1U,
            .values = n_value,
            .row_count = 1U,
            .context = "not between",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND NOT ENGINE = 'InnoDB'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "not over unknown stays unknown",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND ENGINE = NULL",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "equal null remains unknown",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND NOT ENGINE = NULL",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "not equal null remains unknown",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND ENGINE <=> NULL AND TABLE_NAME = 'TABLES'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_one,
            .row_count = 1U,
            .context = "null safe equal metadata null",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND (ENGINE = NULL OR TABLE_NAME = 'TABLES')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_one,
            .row_count = 1U,
            .context = "or true with unknown side",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 FROM information_schema.tables "
                   "WHERE table_schema = 'app' AND table_name = 't' "
                   "AND table_type = 'BASE TABLE'",
            .column_names = literal_column,
            .column_count = 1U,
            .values = literal_one,
            .row_count = 1U,
            .context = "integer literal projection",
        }
    );
    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen information schema db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't'",
            .column_names = table_name_column,
            .column_count = 1U,
            .values = t_value,
            .row_count = 1U,
            .context = "metadata predicates see reopened descriptors",
        }
    );
    failures += expect_statement_ok(database, "USE app", -1);
    failures += expect_statement_ok(database, "CREATE TABLE after_reopen (id INT)", -1);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME IN ('after_reopen', 't') "
                   "ORDER BY TABLE_NAME",
            .column_names = table_name_column,
            .column_count = 1U,
            .values = after_reopen_t_values,
            .row_count = 2U,
            .context = "metadata predicates see descriptor updates",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE nope IN ('x')",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'where clause'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_NAME IN (SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES)",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INFORMATION_SCHEMA WHERE does not support IN subqueries",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_NAME LIKE 't%' ESCAPE ''",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "LIKE ESCAPE supports one-character string literals",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA IN (DATABASE())",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INFORMATION_SCHEMA WHERE IN and BETWEEN support only literal values",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA BETWEEN DATABASE() AND SCHEMA()",
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int seed_database(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE DATABASE app", -1);
    failures += expect_statement_ok(database, "USE app", -1);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE t (id INT AUTO_INCREMENT PRIMARY KEY, v VARCHAR(20), n INT, KEY idx_v (v))",
        -1
    );
    failures += expect_statement_ok(database, "CREATE TABLE wp_options (id INT)", -1);
    failures += expect_statement_ok(database, "CREATE TABLE wpa (id INT)", -1);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': %d %s %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    if (affected_rows >= 0) {
        failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
    }
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
            "%s: expected query success, got %d %s %s\n",
            expected.context,
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
        "%s/mylite_information_schema_predicates_%d_%s.mylite",
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
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return;
    }
    (void)remove(buffer);
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
        if (actual == expected) {
            return 0;
        }
        fprintf(
            stderr,
            "%s: expected %s, got %s\n",
            context,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
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
            actual == NULL ? "NULL" : actual,
            needle == NULL ? "NULL" : needle
        );
        return 1;
    }
    return 0;
}
