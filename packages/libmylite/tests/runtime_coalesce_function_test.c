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
    nonnull_column_count = 7,
    fallback_column_count = 6,
    label_column_count = 8,
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

static int test_coalesce_function_values_and_file_safety(void);
static int test_coalesce_function_unsupported_forms(void);
static int test_coalesce_function_independent_handles(void);
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

    failures += test_coalesce_function_values_and_file_safety();
    failures += test_coalesce_function_unsupported_forms();
    failures += test_coalesce_function_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_coalesce_function_values_and_file_safety(void) {
    static const char *const nonnull_columns[] = {
        "COALESCE(1)",
        "COALESCE(1,2)",
        "COALESCE(0,2)",
        "COALESCE(FALSE,2)",
        "COALESCE(TRUE,2)",
        "COALESCE(+0,2)",
        "COALESCE(-1,2)",
    };
    static const char *const nonnull_values[] = {"1", "1", "0", "0", "1", "0", "-1"};
    static const char *const fallback_columns[] = {
        "COALESCE(NULL)",
        "COALESCE(NULL,10)",
        "COALESCE(NULL,NULL)",
        "COALESCE(NULL,NULL,NULL)",
        "COALESCE(NULL,TRUE)",
        "COALESCE(NULL,FALSE)",
    };
    static const char *const fallback_values[] = {NULL, "10", NULL, NULL, "1", "0"};
    static const char *const label_columns[] = {
        "chosen",
        "fallback",
        "COALESCE (NULL,10)",
        "(COALESCE(NULL,10))",
        "COALESCE(NULL, COALESCE(NULL,1), 2)",
        "COALESCE(IF(0,NULL,4),5)",
        "COALESCE(NULL, IFNULL(COALESCE(NULL,NULL),9))",
        "COALESCE(NULL, IF(COALESCE(NULL,0),1,2))",
    };
    static const char *const label_values[] = {"1", "3", "10", "10", "1", "4", "9", "2"};
    static const char *const ifnull_coalesce_columns[] = {"IFNULL(COALESCE(NULL,NULL),9)"};
    static const char *const ifnull_coalesce_values[] = {"9"};
    static const char *const if_coalesce_columns[] = {"IF(COALESCE(NULL,0),1,2)"};
    static const char *const if_coalesce_values[] = {"2"};
    static const char *const boundary_columns[] = {
        "normalized",
        "fallback_negative",
        "negative",
        "max_value",
    };
    static const char *const boundary_values[] = {
        "1",
        "-2",
        "-1",
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
            .sql = "SELECT COALESCE(1), COALESCE(1,2), COALESCE(0,2), "
                   "COALESCE(FALSE,2), COALESCE(TRUE,2), COALESCE(+0,2), "
                   "COALESCE(-1,2)",
            .columns = nonnull_columns,
            .column_count = nonnull_column_count,
            .values = nonnull_values,
            .row_count = 1U,
            .context = "non-null first values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ALL COALESCE(NULL), COALESCE(NULL,10), COALESCE(NULL,NULL), "
                   "COALESCE(NULL,NULL,NULL), COALESCE(NULL,TRUE), COALESCE(NULL,FALSE) "
                   "FROM DUAL",
            .columns = fallback_columns,
            .column_count = fallback_column_count,
            .values = fallback_values,
            .row_count = 1U,
            .context = "fallback values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COALESCE(1,2) AS chosen, COALESCE(NULL,3) fallback, "
                   "COALESCE (NULL,10), (COALESCE(NULL,10)), "
                   "COALESCE(NULL, COALESCE(NULL,1), 2), COALESCE(IF(0,NULL,4),5), "
                   "COALESCE(NULL, IFNULL(COALESCE(NULL,NULL),9)), "
                   "COALESCE(NULL, IF(COALESCE(NULL,0),1,2))",
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
            .sql = "SELECT IFNULL(COALESCE(NULL,NULL),9)",
            .columns = ifnull_coalesce_columns,
            .column_count = 1U,
            .values = ifnull_coalesce_values,
            .row_count = 1U,
            .context = "nested COALESCE in IFNULL",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT IF(COALESCE(NULL,0),1,2)",
            .columns = if_coalesce_columns,
            .column_count = 1U,
            .values = if_coalesce_values,
            .row_count = 1U,
            .context = "nested COALESCE in IF",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COALESCE(0001,0002) normalized, COALESCE(NULL,-0002) "
                   "fallback_negative, COALESCE(-0001,0002) negative, "
                   "COALESCE(9223372036854775807,0) max_value",
            .columns = boundary_columns,
            .column_count = 4U,
            .values = boundary_values,
            .row_count = 1U,
            .context = "integer normalization and boundaries",
        }
    );

    failures += execute_ok(database, "SELECT COALESCE(NULL,10)", &result);
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
            .context = "row count after COALESCE select",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "COALESCE select leaves catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "COALESCE select leaves sqlite schema generation unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read COALESCE preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "COALESCE select leaves preamble unchanged"
    );

    remove_related_files(path);
    return failures;
}

static int test_coalesce_function_unsupported_forms(void) {
    static const char *const identifier_columns[] = {"coalesce"};
    static const char *const identifier_values[] = {"7"};
    static const char *const row_expression_columns[] = {"COALESCE(1+2,3)"};
    static const char *const row_expression_values[] = {"3"};
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
    failures += execute_ok(database, "CREATE TABLE coalesce(coalesce INT)", NULL);
    failures += execute_ok(database, "INSERT INTO coalesce VALUES (7)", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT coalesce FROM coalesce",
            .columns = identifier_columns,
            .column_count = 1U,
            .values = identifier_values,
            .row_count = 1U,
            .context = "COALESCE keyword identifiers",
        }
    );

    failures += execute_error(
        database,
        "SELECT COALESCE()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COALESCE(NULL, COALESCE())",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COALESCE(1,,2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COALESCE('x',2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT COALESCE() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT COALESCE(1,2+3)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT COALESCE() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT COALESCE(id,2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT COALESCE() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT COALESCE(@@warning_count,2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT COALESCE() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT COALESCE(?,2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COALESCE(@v,2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT COALESCE() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT COALESCE((SELECT 1),2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT COALESCE() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT COALESCE(1,1.0)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT COALESCE() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT COALESCE(1,1e0)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT COALESCE() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT COALESCE(1,0x1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT COALESCE() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT COALESCE(1,b'1')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT COALESCE() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT COALESCE(9223372036854775808,0)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT COALESCE() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT COALESCE(1,9223372036854775808)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT COALESCE() supports only signed 64-bit integer",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COALESCE(1+2,3) FROM t",
            .columns = row_expression_columns,
            .column_count = 1U,
            .values = row_expression_values,
            .row_count = 1U,
            .context = "COALESCE row expression projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t WHERE COALESCE(1,0)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t WHERE id = COALESCE(1,0)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t ORDER BY COALESCE(id,0)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, COUNT(*) FROM t GROUP BY COALESCE(id,0)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, COUNT(*) FROM t GROUP BY id HAVING COALESCE(1,0)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT COALESCE(1,2) LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'LIMIT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COALESCE(1,2) ORDER BY 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'ORDER'",
        }
    );
    failures += execute_ok(database, "UPDATE t SET id = COALESCE(1,2)", NULL);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_coalesce_function_independent_handles(void) {
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
            .sql = "SELECT COALESCE(NULL,10) AS first_result",
            .columns = first_columns,
            .column_count = 1U,
            .values = first_values,
            .row_count = 1U,
            .context = "first handle COALESCE",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT COALESCE(20,0) AS second_result",
            .columns = second_columns,
            .column_count = 1U,
            .values = second_values,
            .row_count = 1U,
            .context = "second handle COALESCE",
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
        "/tmp/mylite-coalesce-function-%s-%d.mylite",
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
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }
    return 0;
}
