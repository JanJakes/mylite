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
    related_file_suffix_capacity = 8,
    name_const_column_count = 6,
    xml_function_column_count = 11,
    row_scalar_column_count = 8,
    mysql_error_incorrect_arguments = 1210,
    mysql_error_unknown = 1105,
};

struct expected_result {
    const char *const *values;
    const char *const *column_names;
    size_t column_count;
    size_t row_count;
    size_t warning_count;
    const char *context;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_sleep_function(void);
static int test_name_const_function(void);
static int test_load_file_function(void);
static int test_xml_functions(void);
static int test_row_scalar_system_functions(void);
static int expect_query_result(
    mylite_db *database,
    const char *sql,
    struct expected_result expected
);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_result_values(const mylite_result *result, struct expected_result expected);
static void dump_result_on_failure(const mylite_result *result, const char *context);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_sleep_function();
    failures += test_name_const_function();
    failures += test_load_file_function();
    failures += test_xml_functions();
    failures += test_row_scalar_system_functions();

    return failures == 0 ? 0 : 1;
}

static int test_sleep_function(void) {
    static const char *const sleep_values[] = {"0", "0", "0", "0"};
    static const char *const truncated_values[] = {"0", "1"};
    static const char *const nonstrict_values[] = {"0", "0", "2"};
    static const char *const warning_values[] = {
        "Warning",
        "1210",
        "Incorrect arguments to sleep.",
        "Warning",
        "1210",
        "Incorrect arguments to sleep.",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open sleep db");
    failures += expect_query_result(
        database,
        "SELECT SLEEP(0), SLEEP(0.001), SLEEP(' 0 '), SLEEP('abc')",
        (struct expected_result){
            .values = sleep_values,
            .column_count = 4U,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "SLEEP basic values",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT SLEEP('0.001x'), @@warning_count",
        (struct expected_result){
            .values = truncated_values,
            .column_count = 2U,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "SLEEP string truncation warning",
        }
    );
    failures += execute_error(
        database,
        "SELECT SLEEP(NULL)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_arguments,
            .sqlstate = "HY000",
            .message_part = "Incorrect arguments to sleep",
        }
    );
    failures += execute_error(
        database,
        "SELECT SLEEP(-1)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_arguments,
            .sqlstate = "HY000",
            .message_part = "Incorrect arguments to sleep",
        }
    );
    failures += execute_statement_ok(database, "SET sql_mode=''");
    failures += expect_query_result(
        database,
        "SELECT SLEEP(NULL), SLEEP(-1), @@warning_count",
        (struct expected_result){
            .values = nonstrict_values,
            .column_count = 3U,
            .row_count = 1U,
            .warning_count = 2U,
            .context = "SLEEP non-strict invalid arguments",
        }
    );
    failures += expect_query_result(
        database,
        "SHOW WARNINGS",
        (struct expected_result){
            .values = warning_values,
            .column_count = 3U,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "SLEEP non-strict warning rows",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_name_const_function(void) {
    static const char *const values[] = {"42", "abc", NULL, "-1", "1.25", "A"};
    static const char *const columns[] = {"answer", "txt", "nil", "neg", "dec", "hex"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open NAME_CONST db");
    failures += expect_query_result(
        database,
        "SELECT NAME_CONST('answer', 42), NAME_CONST('txt', 'abc'), "
        "NAME_CONST('nil', NULL), NAME_CONST('neg', -1), NAME_CONST('dec', 1.25), "
        "NAME_CONST('hex', X'41')",
        (struct expected_result){
            .values = values,
            .column_names = columns,
            .column_count = name_const_column_count,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "NAME_CONST literal values and labels",
        }
    );
    failures += execute_error(
        database,
        "SELECT NAME_CONST('expr', 1 + 2)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_arguments,
            .sqlstate = "HY000",
            .message_part = "Incorrect arguments to NAME_CONST",
        }
    );
    failures += execute_error(
        database,
        "SELECT NAME_CONST(CONCAT('a','b'), 1)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_arguments,
            .sqlstate = "HY000",
            .message_part = "Incorrect arguments to NAME_CONST",
        }
    );
    failures += execute_error(
        database,
        "SELECT NAME_CONST('truth', TRUE)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_arguments,
            .sqlstate = "HY000",
            .message_part = "Incorrect arguments to NAME_CONST",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_load_file_function(void) {
    static const char *const values[] = {NULL, NULL};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open LOAD_FILE db");
    failures += expect_query_result(
        database,
        "SELECT LOAD_FILE(NULL), LOAD_FILE('/definitely/no/such/file')",
        (struct expected_result){
            .values = values,
            .column_count = 2U,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "LOAD_FILE placeholder values",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_xml_functions(void) {
    static const char *const xml_values[] = {
        "one",
        "one two",
        "",
        NULL,
        NULL,
        "<b>two</b>",
        "<a><b>one</b><b>two</b></a>",
        "<a>one</a>",
        NULL,
        NULL,
        NULL,
    };
    static const char *const xml_warning_values[] = {
        "Warning",
        "1525",
        "Incorrect XML value: 'parse error'",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open XML db");
    failures += expect_query_result(
        database,
        "SELECT ExtractValue('<a>one</a>', '/a'), "
        "ExtractValue('<a><b>one</b><b>two</b></a>', '/a/b'), "
        "ExtractValue('<a>one</a>', '/z'), ExtractValue(NULL, '/a'), "
        "ExtractValue('<a>one</a>', NULL), "
        "UpdateXML('<a>one</a>', '/a', '<b>two</b>'), "
        "UpdateXML('<a><b>one</b><b>two</b></a>', '/a/b', '<c>x</c>'), "
        "UpdateXML('<a>one</a>', '/z', '<b>two</b>'), UpdateXML(NULL, '/a', '<x/>'), "
        "UpdateXML('<a/>', NULL, '<x/>'), UpdateXML('<a/>', '/a', NULL)",
        (struct expected_result){
            .values = xml_values,
            .column_count = xml_function_column_count,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "XML function values",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT ExtractValue('<a>', '/a')",
        (struct expected_result){
            .values = &xml_values[3],
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "ExtractValue malformed XML warning",
        }
    );
    failures += expect_query_result(
        database,
        "SHOW WARNINGS",
        (struct expected_result){
            .values = xml_warning_values,
            .column_count = 3U,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "XML warning rows",
        }
    );
    failures += execute_error(
        database,
        "SELECT ExtractValue('<a>one</a>', '@@bad')",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "XPATH syntax error",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_row_scalar_system_functions(void) {
    static const char *const values[] = {
        "1",
        "0",
        NULL,
        "one",
        "one two",
        "<c>x</c>",
        "<a><b>one</b><b>two</b></a>",
        "7",
        "2",
        "0",
        NULL,
        "two",
        "three four",
        "<c>x</c>",
        "<a><b>three</b><b>four</b></a>",
        "7",
    };
    static const char *const columns[] = {
        "id",
        "SLEEP(0)",
        "LOAD_FILE(xml_root)",
        "ExtractValue(xml_root, '/a')",
        "ExtractValue(xml_child, '/a/b')",
        "UpdateXML(xml_root, '/a', '<c>x</c>')",
        "UpdateXML(xml_child, '/a/b', '<c>x</c>')",
        "constant",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "row_scalar") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open row-scalar db");
    failures += execute_statement_ok(database, "CREATE DATABASE wp");
    failures += execute_statement_ok(database, "USE wp");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE xml_rows(id INT PRIMARY KEY, xml_root TEXT, xml_child TEXT)"
    );
    failures += execute_statement_ok(
        database,
        "INSERT INTO xml_rows VALUES "
        "(1, '<a>one</a>', '<a><b>one</b><b>two</b></a>'), "
        "(2, '<a>two</a>', '<a><b>three</b><b>four</b></a>')"
    );
    failures += expect_query_result(
        database,
        "SELECT id, SLEEP(0), LOAD_FILE(xml_root), ExtractValue(xml_root, '/a'), "
        "ExtractValue(xml_child, '/a/b'), UpdateXML(xml_root, '/a', '<c>x</c>'), "
        "UpdateXML(xml_child, '/a/b', '<c>x</c>'), NAME_CONST('constant', 7) "
        "FROM xml_rows ORDER BY id",
        (struct expected_result){
            .values = values,
            .column_names = columns,
            .column_count = row_scalar_column_count,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "row-scalar residual system functions",
        }
    );
    failures += execute_error(
        database,
        "SELECT ExtractValue(xml_root, xml_root) FROM xml_rows",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "Only constant XPATH queries are supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT UpdateXML(xml_root, xml_root, '<x/>') FROM xml_rows",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "Only constant XPATH queries are supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT ExtractValue(xml_root, '@@bad') FROM xml_rows",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "XPATH syntax error",
        }
    );
    failures += execute_error(
        database,
        "SELECT UpdateXML(xml_root, '@@bad', '<x/>') FROM xml_rows",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "XPATH syntax error",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int expect_query_result(
    mylite_db *database,
    const char *sql,
    struct expected_result expected
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (expect_int(rc, MYLITE_OK, expected.context) != 0) {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
        mylite_result_free(result);
        return 1;
    }
    failures += expect_result_values(result, expected);
    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_result_values(const mylite_result *result, struct expected_result expected) {
    size_t value_index = 0U;
    int failures = 0;

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);
    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        if (expected.column_names != NULL) {
            failures += expect_text_or_null(
                mylite_result_column_name(result, column_index),
                expected.column_names[column_index],
                expected.context
            );
        }
    }
    for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            failures += expect_text_or_null(
                mylite_result_value_text(result, row_index, column_index),
                expected.values[value_index],
                expected.context
            );
            ++value_index;
        }
    }
    if (failures != 0) {
        dump_result_on_failure(result, expected.context);
    }
    return failures;
}

static void dump_result_on_failure(const mylite_result *result, const char *context) {
    size_t column_count = mylite_result_column_count(result);
    size_t row_count = mylite_result_row_count(result);

    fprintf(
        stderr,
        "%s: actual result has %zu columns, %zu rows, %zu warnings\n",
        context,
        column_count,
        row_count,
        mylite_result_warning_count(result)
    );
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        fprintf(
            stderr,
            "%s: column[%zu]=%s\n",
            context,
            column_index,
            mylite_result_column_name(result, column_index)
        );
    }
    for (size_t row_index = 0U; row_index < row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < column_count; ++column_index) {
            const char *value = mylite_result_value_text(result, row_index, column_index);

            fprintf(
                stderr,
                "%s: value[%zu,%zu]=%s\n",
                context,
                row_index,
                column_index,
                value == NULL ? "NULL" : value
            );
        }
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
        "%s: expected %s, got %s\n",
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
        "%s: expected text containing %s, got %s\n",
        context,
        needle == NULL ? "NULL" : needle,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_system_function_residuals_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path\n");
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
    char buffer[test_path_capacity + related_file_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}
