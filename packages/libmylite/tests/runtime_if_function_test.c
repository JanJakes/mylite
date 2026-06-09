#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
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
    path_suffix_capacity = 16,
    truth_column_count = 7,
    branch_column_count = 5,
    label_column_count = 5,
    boundary_column_count = 4,
    mysql_error_parse = 1064,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static int test_if_function_values_and_file_safety(void);
static int test_if_function_unsupported_forms(void);
static int test_if_function_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_if_function_values_and_file_safety();
    failures += test_if_function_unsupported_forms();
    failures += test_if_function_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_if_function_values_and_file_safety(void) {
    static const char *const truth_columns[] = {
        "IF(1,2,3)",
        "IF(0,2,3)",
        "IF(NULL,2,3)",
        "IF(-1,2,3)",
        "IF(+0,2,3)",
        "IF(TRUE,2,3)",
        "IF(FALSE,2,3)",
    };
    static const char *const truth_values[] = {"2", "3", "3", "2", "3", "2", "3"};
    static const char *const branch_columns[] = {
        "IF(1,NULL,3)",
        "IF(0,2,NULL)",
        "IF(NULL,NULL,3)",
        "IF(1,TRUE,FALSE)",
        "IF(0,TRUE,FALSE)",
    };
    static const char *const branch_values[] = {NULL, NULL, "3", "1", "0"};
    static const char *const label_columns[] = {
        "chosen",
        "fallback",
        "IF (1,2,3)",
        "(IF(1,2,3))",
        "IF(IF(1,1,0),2,3)",
    };
    static const char *const label_values[] = {"2", "3", "2", "2", "2"};
    static const char *const boundary_columns[] = {
        "normalized",
        "zero_false",
        "negative",
        "max_value",
    };
    static const char *const boundary_values[] = {
        "2",
        "3",
        "-2",
        "9223372036854775807",
    };
    static const char *const row_count_columns[] = {"ROW_COUNT()"};
    static const char *const row_count_values[] = {"-1"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT IF(1,2,3), IF(0,2,3), IF(NULL,2,3), IF(-1,2,3), "
                   "IF(+0,2,3), IF(TRUE,2,3), IF(FALSE,2,3)",
            .columns = truth_columns,
            .column_count = truth_column_count,
            .values = truth_values,
            .row_count = 1U,
            .context = "truthiness values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ALL IF(1,NULL,3), IF(0,2,NULL), IF(NULL,NULL,3), "
                   "IF(1,TRUE,FALSE), IF(0,TRUE,FALSE) FROM DUAL",
            .columns = branch_columns,
            .column_count = branch_column_count,
            .values = branch_values,
            .row_count = 1U,
            .context = "branch values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT IF(1,2,3) AS chosen, IF(0,NULL,3) fallback, IF (1,2,3), "
                   "(IF(1,2,3)), IF(IF(1,1,0),2,3)",
            .columns = label_columns,
            .column_count = label_column_count,
            .values = label_values,
            .row_count = 1U,
            .context = "labels aliases and nesting",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT IF(0001,0002,0003) normalized, IF(0000,0002,0003) "
                   "zero_false, IF(-0001,-0002,-0003) negative, "
                   "IF(-9223372036854775807,9223372036854775807,"
                   "-9223372036854775807) max_value",
            .columns = boundary_columns,
            .column_count = boundary_column_count,
            .values = boundary_values,
            .row_count = 1U,
            .context = "integer normalization and boundaries",
        }
    );

    failures += execute_ok(database, "SELECT IF(1,2,3)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .columns = row_count_columns,
            .column_count = 1U,
            .values = row_count_values,
            .row_count = 1U,
            .context = "row count after IF select",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "IF select leaves catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "IF select leaves sqlite schema generation unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read IF preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "IF select leaves preamble unchanged"
    );

    remove_related_files(path);
    return failures;
}

static int test_if_function_unsupported_forms(void) {
    static const char *const row_expression_columns[] = {"IF(id,2+3,4)"};
    static const char *const row_expression_values[] = {"5"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open unsupported file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE t(id INT NOT NULL)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1)", NULL);

    failures += execute_error(
        database,
        "SELECT IF()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(1,2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(1,2,3,4)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF('x',2,3)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(1,2+3,4)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(id,2,3)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(@@warning_count,2,3)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(?,2,3)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(@v,2,3)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF((SELECT 1),2,3)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(1,1.0,2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(1,1e0,2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(1,0x1,2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(1,b'1',2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(1,9223372036854775808,0)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(0,9223372036854775808,1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF('x',2,3) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "IF() row conditions support only integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t WHERE IF(1,TRUE,FALSE)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t WHERE id = IF(1,1,0)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t ORDER BY IF(1,id,id)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, COUNT(*) FROM t GROUP BY IF(1,id,id)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, COUNT(*) FROM t GROUP BY id HAVING IF(1,1,0)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(1,2,3) LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'LIMIT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(1,2,3) ORDER BY 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'ORDER'",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT IF(id,2+3,4) FROM t",
            .columns = row_expression_columns,
            .column_count = 1U,
            .values = row_expression_values,
            .row_count = 1U,
            .context = "IF row expression projection",
        }
    );
    failures += execute_ok(database, "UPDATE t SET id = IF(1,2,3)", NULL);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_if_function_independent_handles(void) {
    static const char *const first_columns[] = {"first_result"};
    static const char *const first_values[] = {"10"};
    static const char *const second_columns[] = {"second_result"};
    static const char *const second_values[] = {"20"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first memory handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second memory handle");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT IF(1,10,0) AS first_result",
            .columns = first_columns,
            .column_count = 1U,
            .values = first_values,
            .row_count = 1U,
            .context = "first handle IF",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT IF(0,0,20) AS second_result",
            .columns = second_columns,
            .column_count = 1U,
            .values = second_values,
            .row_count = 1U,
            .context = "second handle IF",
        }
    );

    mylite_close(second);
    mylite_close(first);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[value_index],
                expected.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-if-function-%s-%d.mylite",
        name,
        current_process_id()
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
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity + path_suffix_capacity];
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
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    read_size = fread(buffer, 1U, size, file);
    fclose(file);

    return read_size == size ? 0 : 1;
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "(null)" : expected,
                actual == NULL ? "(null)" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected %s, got %s\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing %s, got %s\n",
            context,
            needle == NULL ? "(null)" : needle,
            actual == NULL ? "(null)" : actual
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
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }
    return 0;
}
