#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_invalid_json_text = 3141,
    mysql_error_invalid_json_path = 3143,
    mysql_error_table_function_alias = 3667,
    metadata_column_count = 5,
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

static int test_json_table_literal_results(void);
static int test_json_table_literal_metadata(void);
static int test_json_table_literal_diagnostics(void);
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
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_json_table_literal_results();
    failures += test_json_table_literal_metadata();
    failures += test_json_table_literal_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_json_table_literal_results(void) {
    static const char *const columns[] = {"ord", "name", "price", "has_price", "payload"};
    static const char *const values[] = {
        "1",
        "apple",
        "3",
        "1",
        "{\"a\": 1}",
        "2",
        "pear",
        NULL,
        "0",
        NULL,
        "3",
        "42",
        "5",
        "1",
        "[1, 2]",
    };
    static const char *const root_columns[] = {"ord", "name"};
    static const char *const root_values[] = {"1", "root"};
    static const char *const filter_columns[] = {"ord", "name"};
    static const char *const filter_values[] = {"1", "apple"};
    static const char *const empty_columns[] = {"ord", "name"};
    mylite_db *database = NULL;
    int failures = expect_int(mylite_open_memory(&database), MYLITE_OK, "open memory database");

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT jt.ord, jt.name, jt.price, jt.has_price, jt.payload "
                   "FROM JSON_TABLE("
                   "'[{\"name\":\"apple\",\"price\":3,\"payload\":{\"a\":1}},"
                   "{\"name\":\"pear\"},{\"name\":42,\"price\":5,\"payload\":[1,2]}]', "
                   "'$[*]' COLUMNS ("
                   "ord FOR ORDINALITY, "
                   "name VARCHAR(20) PATH '$.name', "
                   "price INT PATH '$.price' NULL ON EMPTY NULL ON ERROR, "
                   "has_price INT EXISTS PATH '$.price', "
                   "payload JSON PATH '$.payload')) AS jt",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 3U,
            .context = "JSON_TABLE literal rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ord, name FROM JSON_TABLE('{\"name\":\"root\"}', "
                   "'$' COLUMNS (ord FOR ORDINALITY, name VARCHAR(20) PATH '$.name')) jt",
            .columns = root_columns,
            .column_count = sizeof(root_columns) / sizeof(root_columns[0]),
            .values = root_values,
            .row_count = 1U,
            .context = "JSON_TABLE root row path",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT jt.ord, jt.name FROM JSON_TABLE("
                   "'[{\"name\":\"apple\",\"price\":3},{\"name\":\"pear\"}]', "
                   "'$[*]' COLUMNS ("
                   "ord FOR ORDINALITY, name VARCHAR(20) PATH '$.name', "
                   "price INT PATH '$.price')) AS jt "
                   "WHERE jt.price IS NOT NULL ORDER BY jt.ord DESC",
            .columns = filter_columns,
            .column_count = sizeof(filter_columns) / sizeof(filter_columns[0]),
            .values = filter_values,
            .row_count = 1U,
            .context = "JSON_TABLE filter and order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM JSON_TABLE('[]', '$[*]' COLUMNS ("
                   "ord FOR ORDINALITY, name VARCHAR(20) PATH '$.name')) AS jt",
            .columns = empty_columns,
            .column_count = sizeof(empty_columns) / sizeof(empty_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "JSON_TABLE empty rowset",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_json_table_literal_metadata(void) {
    static const char *const query =
        "SELECT jt.ord, jt.name, jt.price, jt.has_price, jt.payload "
        "FROM JSON_TABLE('[{\"name\":\"apple\",\"price\":3,\"payload\":{\"a\":1}}]', "
        "'$[*]' COLUMNS ("
        "ord FOR ORDINALITY, "
        "name VARCHAR(20) PATH '$.name', "
        "price INT PATH '$.price', "
        "has_price INT EXISTS PATH '$.price', "
        "payload JSON PATH '$.payload')) AS jt";

    static const enum mylite_result_column_type expected_types[metadata_column_count] = {
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        MYLITE_RESULT_COLUMN_TYPE_LONG,
        MYLITE_RESULT_COLUMN_TYPE_LONG,
        MYLITE_RESULT_COLUMN_TYPE_JSON,
    };

    static const char *const expected_names[metadata_column_count] = {
        "ord",
        "name",
        "price",
        "has_price",
        "payload",
    };
    mylite_result *result = NULL;
    mylite_db *database = NULL;
    int failures = expect_int(mylite_open_memory(&database), MYLITE_OK, "open metadata database");

    failures += execute_ok(database, query, &result);
    if (failures == 0) {
        failures += expect_size(
            mylite_result_column_count(result),
            metadata_column_count,
            "JSON_TABLE metadata count"
        );
        for (size_t column = 0U; column < metadata_column_count; ++column) {
            failures += expect_text(
                mylite_result_column_name(result, column),
                expected_names[column],
                "JSON_TABLE column name"
            );
            failures += expect_text(
                mylite_result_column_table_name(result, column),
                "jt",
                "JSON_TABLE table alias metadata"
            );
            failures += expect_text(
                mylite_result_column_origin_name(result, column),
                expected_names[column],
                "JSON_TABLE origin name metadata"
            );
            failures += expect_int(
                (int)mylite_result_column_type(result, column),
                (int)expected_types[column],
                "JSON_TABLE column type"
            );
            failures += expect_int(
                mylite_result_column_nullable(result, column),
                1,
                "JSON_TABLE nullable metadata"
            );
        }
    }

    mylite_result_free(result);
    mylite_close(database);
    return failures;
}

static int test_json_table_literal_diagnostics(void) {
    mylite_db *database = NULL;
    int failures =
        expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostics database");

    failures += execute_error(
        database,
        "SELECT * FROM JSON_TABLE('[1]', '$[*]' COLUMNS (ord FOR ORDINALITY))",
        (struct expected_sql_error){
            .code = mysql_error_table_function_alias,
            .sqlstate = "42000",
            .message_part = "Every table function must have an alias",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM JSON_TABLE('[bad]', '$[*]' COLUMNS (ord FOR ORDINALITY)) AS jt",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM JSON_TABLE('[{\"a\":1}]', 'bad' COLUMNS (ord FOR ORDINALITY)) AS jt",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM JSON_TABLE('[{\"a\":1}]', '$.a' COLUMNS (ord FOR ORDINALITY)) AS jt",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "supports only '$' and '$[*]' row paths",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM JSON_TABLE('[{\"a\":1}]', '$[*]' COLUMNS ("
        "v INT PATH 'bad')) AS jt",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM JSON_TABLE(CONCAT('[1]'), '$[*]' COLUMNS (ord FOR ORDINALITY)) AS jt",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "supports only string literal JSON documents",
        }
    );

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
        mylite_result_free(result);
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
    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, expected.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", expected.sql, mylite_errmsg(database));
        mylite_result_free(result);
        return failures;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[(row * expected.column_count) + column],
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
    return expect_text(mylite_result_value_text(result, row, column), expected, context);
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
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
    if ((actual == NULL && expected != NULL) || (actual != NULL && expected == NULL) ||
        (actual != NULL && expected != NULL && strcmp(actual, expected) != 0)) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
        );
        return 1;
    }
    return 0;
}
