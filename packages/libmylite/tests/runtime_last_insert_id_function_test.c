#include <mylite/mylite.h>

#include "runtime/mylite_mysql_server_identity.h"

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include <inttypes.h>
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
    connection_id_text_capacity = 32,
    mixed_scalar_column_count = 9,
    ordered_expression_column_count = 9,
    mysql_error_parse = 1064,
    mysql_error_native_function_parameter_count = 1582,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_scalar_result {
    const char *const *columns;
    const char *const *values;
    size_t count;
    const char *context;
};

struct expected_last_insert_id {
    const char *value;
    const char *context;
};

static int test_last_insert_id_values_and_statement_interactions(void);
static int test_last_insert_id_expression_values(void);
static int test_last_insert_id_memory_handle(void);
static int test_last_insert_id_reopen_and_independent_handles(void);
static int test_last_insert_id_unsupported_forms(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_scalar_result(
    const mylite_result *result,
    struct expected_scalar_result expected
);
static int expect_non_query_result(
    const mylite_result *result,
    int64_t affected_rows,
    const char *context
);
static int expect_last_insert_id_value(
    mylite_db *database,
    struct expected_last_insert_id expected
);
static int expect_last_insert_id(mylite_db *database, const char *context);
static int expect_single_column_rows(
    const mylite_result *result,
    const char *const *values,
    size_t row_count,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int expect_true(int condition, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_last_insert_id_values_and_statement_interactions();
    failures += test_last_insert_id_expression_values();
    failures += test_last_insert_id_memory_handle();
    failures += test_last_insert_id_reopen_and_independent_handles();
    failures += test_last_insert_id_unsupported_forms();

    return failures == 0 ? 0 : 1;
}

static int test_last_insert_id_values_and_statement_interactions(void) {
    static const char *const last_insert_id_columns[] = {"LAST_INSERT_ID()"};
    static const char *const last_insert_id_values[] = {"0"};
    static const char *const label_columns[] = {
        "last_insert_id()",
        "Last_Insert_Id()",
        "LAST_INSERT_ID ()",
        "(LAST_INSERT_ID())",
    };
    static const char *const label_values[] = {"0", "0", "0", "0"};
    static const char *const insert_interaction_columns[] = {"LAST_INSERT_ID()", "ROW_COUNT()"};
    static const char *const insert_interaction_values[] = {"0", "2"};
    static const char *const update_interaction_values[] = {"0", "1"};
    static const char *const row_count_columns[] = {"ROW_COUNT()"};
    static const char *const row_count_after_select_values[] = {"-1"};
    static const char *const mixed_columns[] = {
        "LAST_INSERT_ID()",
        "ROW_COUNT()",
        "DATABASE()",
        "USER()",
        "CURRENT_USER",
        "CURRENT_ROLE()",
        "CONNECTION_ID()",
        "VERSION()",
        "@@warning_count",
    };
    char connection_id_text[connection_id_text_capacity];
    const char *mixed_values[mixed_scalar_column_count] = {
        "0",
        "0",
        "app",
        "root@%",
        "root@%",
        "NONE",
        connection_id_text,
        MYLITE_MYSQL_SERVER_VERSION_STRING,
        "0",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;
    int written = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open last insert id file");
    session = mylite_connection_session_state(database);
    written = snprintf(
        connection_id_text,
        sizeof(connection_id_text),
        "%" PRIu64,
        session->connection_id
    );
    if (written < 0 || (size_t)written >= sizeof(connection_id_text)) {
        (void)fprintf(stderr, "failed to format connection id\n");
        mylite_close(database);
        remove_related_files(path);
        return failures + 1;
    }
    failures += expect_uint64(session->last_insert_id, 0U, "initial session last insert id");

    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;
    failures += execute_ok(database, "SELECT LAST_INSERT_ID()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = last_insert_id_columns,
            .values = last_insert_id_values,
            .count = 1U,
            .context = "initial last insert id",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_uint64(session->last_insert_id, 0U, "scalar read leaves session value");
    failures += expect_uint64(
        session->catalog_generation,
        catalog_generation,
        "scalar read leaves catalog generation"
    );
    failures += expect_uint64(
        session->sqlite_schema_generation,
        sqlite_schema_generation,
        "scalar read leaves sqlite schema generation"
    );

    failures += execute_ok(
        database,
        "SELECT last_insert_id(), Last_Insert_Id(), LAST_INSERT_ID (), (LAST_INSERT_ID())",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = label_columns,
            .values = label_values,
            .count = 4U,
            .context = "last insert id labels",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT LAST_INSERT_ID() FROM DUAL", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = last_insert_id_columns,
            .values = last_insert_id_values,
            .count = 1U,
            .context = "last insert id from dual",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_ok(
        database,
        "SELECT LAST_INSERT_ID(), ROW_COUNT(), DATABASE(), USER(), CURRENT_USER, CURRENT_ROLE(), "
        "CONNECTION_ID(), VERSION(), @@warning_count",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = mixed_columns,
            .values = mixed_values,
            .count = mixed_scalar_column_count,
            .context = "mixed last insert id scalar functions",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "CREATE TABLE t (id INT NOT NULL, v INT NULL)");
    failures += expect_last_insert_id(database, "create table leaves last insert id");
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 10), (2, 20)", &result);
    failures += expect_non_query_result(result, 2, "non-auto insert result");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT LAST_INSERT_ID(), ROW_COUNT()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = insert_interaction_columns,
            .values = insert_interaction_values,
            .count = 2U,
            .context = "non-auto insert leaves last insert id",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT ROW_COUNT()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = row_count_columns,
            .values = row_count_after_select_values,
            .count = 1U,
            .context = "last insert id select stores result-set row count",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "UPDATE t SET v = 11 WHERE id = 1", &result);
    failures += expect_non_query_result(result, 1, "update result");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT LAST_INSERT_ID(), ROW_COUNT()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = insert_interaction_columns,
            .values = update_interaction_values,
            .count = 2U,
            .context = "update leaves last insert id",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "DELETE FROM t WHERE id = 2");
    failures += expect_last_insert_id(database, "delete leaves last insert id");
    failures += execute_statement_ok(database, "ALTER TABLE t ADD COLUMN extra INT DEFAULT 5");
    failures += expect_last_insert_id(database, "alter table leaves last insert id");
    failures += execute_statement_ok(database, "RENAME TABLE t TO renamed");
    failures += expect_last_insert_id(database, "rename leaves last insert id");
    failures += execute_ok(database, "SHOW TABLES", &result);
    failures += expect_size(mylite_result_row_count(result), 1U, "show tables row count");
    mylite_result_free(result);
    result = NULL;
    failures += expect_last_insert_id(database, "show leaves last insert id");
    failures += execute_statement_ok(database, "TRUNCATE TABLE renamed");
    failures += expect_last_insert_id(database, "truncate leaves last insert id");
    failures += execute_statement_ok(database, "DROP TABLE renamed");
    failures += expect_last_insert_id(database, "drop table leaves last insert id");
    failures += execute_statement_ok(database, "DROP DATABASE app");
    failures += expect_last_insert_id(database, "drop database leaves last insert id");

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "last insert id preserves MyLite preamble"
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_last_insert_id_expression_values(void) {
    static const char *const ordered_columns[] = {
        "LAST_INSERT_ID(7)",
        "LAST_INSERT_ID()",
        "LAST_INSERT_ID(NULL)",
        "LAST_INSERT_ID()",
        "LAST_INSERT_ID(-1)",
        "LAST_INSERT_ID()",
        "LAST_INSERT_ID(TRUE)",
        "LAST_INSERT_ID(FALSE)",
        "LAST_INSERT_ID()",
    };
    static const char *const ordered_values[] = {
        "7",
        "7",
        NULL,
        "0",
        "18446744073709551615",
        "18446744073709551615",
        "1",
        "0",
        "0",
    };
    static const char *const state_columns[] = {
        "LAST_INSERT_ID()",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const state_after_select_values[] = {"0", "0", "-1"};
    static const char *const unsigned_boundary_columns[] = {
        "LAST_INSERT_ID(18446744073709551615)",
        "LAST_INSERT_ID()",
    };
    static const char *const unsigned_boundary_values[] = {
        "18446744073709551615",
        "18446744073709551615",
    };
    static const char *const signed_boundary_columns[] = {
        "LAST_INSERT_ID(-9223372036854775808)",
        "LAST_INSERT_ID()",
    };
    static const char *const signed_boundary_values[] = {
        "9223372036854775808",
        "9223372036854775808",
    };
    static const char *const from_dual_columns[] = {"manual"};
    static const char *const from_dual_values[] = {"1"};
    static const char *const after_do_columns[] = {
        "ROW_COUNT()",
        "LAST_INSERT_ID()",
        "@@warning_count",
    };
    static const char *const after_do_values[] = {"0", "5", "0"};
    static const char *const last_id_alias_columns[] = {"last_id"};
    static const char *const explicit_insert_values[] = {"99"};
    static const char *const generated_insert_values[] = {"11"};
    static const char *const explicit_after_generated_values[] = {"11"};
    static const char *const explicit_insert_rows[] = {"10"};
    static const char *const generated_insert_rows[] = {"10", "11", "12"};
    static const char *const explicit_after_generated_rows[] = {"10", "11", "12", "20"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "expression") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open expression database");

    failures += execute_ok(
        database,
        "SELECT LAST_INSERT_ID(7), LAST_INSERT_ID(), LAST_INSERT_ID(NULL), LAST_INSERT_ID(), "
        "LAST_INSERT_ID(-1), LAST_INSERT_ID(), LAST_INSERT_ID(TRUE), LAST_INSERT_ID(FALSE), "
        "LAST_INSERT_ID()",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = ordered_columns,
            .values = ordered_values,
            .count = ordered_expression_column_count,
            .context = "ordered last insert id expression evaluation",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures +=
        execute_ok(database, "SELECT LAST_INSERT_ID(), @@warning_count, ROW_COUNT()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = state_columns,
            .values = state_after_select_values,
            .count = 3U,
            .context = "last insert id expression state after select",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT LAST_INSERT_ID(18446744073709551615), LAST_INSERT_ID()",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = unsigned_boundary_columns,
            .values = unsigned_boundary_values,
            .count = 2U,
            .context = "last insert id unsigned boundary",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT LAST_INSERT_ID(-9223372036854775808), LAST_INSERT_ID()",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = signed_boundary_columns,
            .values = signed_boundary_values,
            .count = 2U,
            .context = "last insert id signed boundary",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT LAST_INSERT_ID(+1) AS manual FROM DUAL", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = from_dual_columns,
            .values = from_dual_values,
            .count = 1U,
            .context = "last insert id expression from dual alias",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "DO LAST_INSERT_ID(42), LAST_INSERT_ID(NULL), LAST_INSERT_ID(5)",
        &result
    );
    failures += expect_non_query_result(result, 0, "last insert id do result");
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = after_do_columns,
            .values = after_do_values,
            .count = 3U,
            .context = "last insert id do state",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE ai (id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += execute_statement_ok(database, "SELECT LAST_INSERT_ID(99)");
    failures += execute_statement_ok(database, "INSERT INTO ai (id, v) VALUES (10, 10)");
    failures += execute_ok(database, "SELECT LAST_INSERT_ID() AS last_id", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = last_id_alias_columns,
            .values = explicit_insert_values,
            .count = 1U,
            .context = "explicit insert preserves manual last insert id",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT id FROM ai ORDER BY id", &result);
    failures +=
        expect_single_column_rows(result, explicit_insert_rows, 1U, "explicit insert row id");
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "INSERT INTO ai (v) VALUES (20), (30)");
    failures += execute_ok(database, "SELECT LAST_INSERT_ID() AS last_id", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = last_id_alias_columns,
            .values = generated_insert_values,
            .count = 1U,
            .context = "generated insert overwrites manual last insert id",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT id FROM ai ORDER BY id", &result);
    failures +=
        expect_single_column_rows(result, generated_insert_rows, 3U, "generated insert row ids");
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "INSERT INTO ai (id, v) VALUES (20, 40)");
    failures += execute_ok(database, "SELECT LAST_INSERT_ID() AS last_id", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = last_id_alias_columns,
            .values = explicit_after_generated_values,
            .count = 1U,
            .context = "explicit insert preserves generated last insert id",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT id FROM ai ORDER BY id", &result);
    failures += expect_single_column_rows(
        result,
        explicit_after_generated_rows,
        4U,
        "explicit after generated row ids"
    );
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_last_insert_id_memory_handle(void) {
    mylite_db *database = NULL;
    const struct mylite_session_state *session = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open memory database");
    session = mylite_connection_session_state(database);
    failures += expect_uint64(session->last_insert_id, 0U, "memory initial last insert id");
    failures += expect_last_insert_id(database, "memory scalar last insert id");

    mylite_close(database);

    return failures;
}

static int test_last_insert_id_reopen_and_independent_handles(void) {
    static const char *const selected_rows[] = {"1", "2"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "reopen") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open file database");
    failures += execute_statement_ok(first, "CREATE DATABASE app");
    failures += execute_statement_ok(first, "USE app");
    failures += execute_statement_ok(first, "CREATE TABLE t (id INT)");
    failures += execute_statement_ok(first, "INSERT INTO t VALUES (1), (2)");
    failures += expect_last_insert_id(first, "file insert leaves last insert id");
    failures += execute_statement_ok(first, "SELECT LAST_INSERT_ID(123)");
    failures += expect_last_insert_id_value(
        first,
        (struct expected_last_insert_id){
            .value = "123",
            .context = "manual value before reopen",
        }
    );
    mylite_close(first);
    first = NULL;

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "reopen file database");
    failures += expect_last_insert_id(first, "reopened handle initial last insert id");
    failures += execute_statement_ok(first, "USE app");
    failures += execute_ok(first, "SELECT id FROM t ORDER BY id", &result);
    failures += expect_single_column_rows(result, selected_rows, 2U, "reopened stored rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_last_insert_id(first, "reopened select leaves last insert id");

    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open independent handle");
    failures += expect_last_insert_id(second, "second handle initial last insert id");
    failures += execute_statement_ok(second, "CREATE DATABASE second_app");
    failures += expect_last_insert_id(second, "second handle create leaves last insert id");
    failures += expect_last_insert_id(first, "first handle remains independent");

    mylite_close(second);
    mylite_close(first);
    remove_related_files(first_path);
    remove_related_files(second_path);

    return failures;
}

static int test_last_insert_id_unsupported_forms(void) {
    static const char *const last_insert_id_alias_columns[] = {"id"};
    static const char *const last_insert_id_alias_values[] = {"33"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open unsupported database");
    failures += execute_statement_ok(database, "SELECT LAST_INSERT_ID(33)");
    failures += expect_last_insert_id_value(
        database,
        (struct expected_last_insert_id){
            .value = "33",
            .context = "manual value before errors",
        }
    );

    failures += execute_error(
        database,
        "SELECT LAST_INSERT_ID('abc')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "supports only integer",
        }
    );
    failures += expect_last_insert_id_value(
        database,
        (struct expected_last_insert_id){
            .value = "33",
            .context = "string error leaves last insert id",
        }
    );

    failures += execute_error(
        database,
        "SELECT LAST_INSERT_ID(1 + 2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "supports only integer",
        }
    );
    failures += expect_last_insert_id_value(
        database,
        (struct expected_last_insert_id){
            .value = "33",
            .context = "arithmetic error leaves last insert id",
        }
    );

    failures += execute_error(
        database,
        "SELECT LAST_INSERT_ID(44), LAST_INSERT_ID('abc')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "supports only integer",
        }
    );
    failures += expect_last_insert_id_value(
        database,
        (struct expected_last_insert_id){
            .value = "33",
            .context = "mixed select error leaves last insert id",
        }
    );

    failures += execute_error(
        database,
        "DO LAST_INSERT_ID(44), LAST_INSERT_ID(1 + 2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "supports only integer",
        }
    );
    failures += expect_last_insert_id_value(
        database,
        (struct expected_last_insert_id){
            .value = "33",
            .context = "mixed do error leaves last insert id",
        }
    );

    failures += execute_error(
        database,
        "SELECT LAST_INSERT_ID(18446744073709551616)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "out of range",
        }
    );
    failures += expect_last_insert_id_value(
        database,
        (struct expected_last_insert_id){
            .value = "33",
            .context = "positive out-of-range error leaves last insert id",
        }
    );

    failures += execute_error(
        database,
        "SELECT LAST_INSERT_ID(-9223372036854775809)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "negative integer literal is out of range",
        }
    );
    failures += expect_last_insert_id_value(
        database,
        (struct expected_last_insert_id){
            .value = "33",
            .context = "negative out-of-range error leaves last insert id",
        }
    );

    failures += execute_error(
        database,
        "SELECT LAST_INSERT_ID(1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += expect_last_insert_id_value(
        database,
        (struct expected_last_insert_id){
            .value = "33",
            .context = "multiple-argument error leaves last insert id",
        }
    );

    failures += execute_error(
        database,
        "DO LAST_INSERT_ID(1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += expect_last_insert_id_value(
        database,
        (struct expected_last_insert_id){
            .value = "33",
            .context = "do arity error leaves last insert id",
        }
    );

    failures += execute_error(
        database,
        "SELECT LAST_INSERT_ID",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor-backed table reads",
        }
    );
    failures += expect_last_insert_id_value(
        database,
        (struct expected_last_insert_id){
            .value = "33",
            .context = "bare name error leaves last insert id",
        }
    );

    failures += execute_ok(database, "SELECT LAST_INSERT_ID() AS id", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = last_insert_id_alias_columns,
            .values = last_insert_id_alias_values,
            .count = 1U,
            .context = "last insert id alias",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "SELECT LAST_INSERT_ID() LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE t (id INT)");
    failures += execute_statement_ok(database, "INSERT INTO t VALUES (1), (2)");
    failures += execute_error(
        database,
        "SELECT LAST_INSERT_ID() FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += expect_last_insert_id_value(
        database,
        (struct expected_last_insert_id){
            .value = "33",
            .context = "zero-arg table-backed error leaves last insert id",
        }
    );
    failures += execute_error(
        database,
        "SELECT LAST_INSERT_ID(id) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += expect_last_insert_id_value(
        database,
        (struct expected_last_insert_id){
            .value = "33",
            .context = "one-arg table-backed error leaves last insert id",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        (void)fprintf(
            stderr,
            "%s: expected success, got rc=%d err=%d state=%s message=%s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }

    *out_result = result;
    return 0;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);

    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        (void)fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, sql);
    failures += expect_true(result == NULL, "error result is null");

    return failures;
}

static int expect_scalar_result(
    const mylite_result *result,
    struct expected_scalar_result expected
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), expected.count, expected.context);
    failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t column = 0U; column < expected.count; ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, column),
            expected.values[column],
            expected.context
        );
    }

    return failures;
}

static int expect_non_query_result(
    const mylite_result *result,
    int64_t affected_rows,
    const char *context
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), 0U, context);
    failures += expect_size(mylite_result_row_count(result), 0U, context);
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    return failures;
}

static int expect_last_insert_id_value(
    mylite_db *database,
    struct expected_last_insert_id expected
) {
    static const char *const columns[] = {"LAST_INSERT_ID()"};
    const char *values[] = {expected.value};
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SELECT LAST_INSERT_ID()", &result);

    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = columns,
            .values = values,
            .count = 1U,
            .context = expected.context,
        }
    );
    mylite_result_free(result);

    return failures;
}

static int expect_last_insert_id(mylite_db *database, const char *context) {
    return expect_last_insert_id_value(
        database,
        (struct expected_last_insert_id){
            .value = "0",
            .context = context,
        }
    );
}

static int expect_single_column_rows(
    const mylite_result *result,
    const char *const *values,
    size_t row_count,
    const char *context
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures += expect_size(mylite_result_row_count(result), row_count, context);
    for (size_t row = 0U; row < row_count; ++row) {
        failures +=
            expect_text_or_null(mylite_result_value_text(result, row, 0U), values[row], context);
    }

    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_last_insert_id_%s_%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        (void)fprintf(stderr, "failed to build test path for %s\n", name);
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
        (void)fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        (void)fprintf(stderr, "%s: failed to seek\n", path);
        (void)fclose(file);
        return 1;
    }
    read_size = fread(buffer, 1U, size, file);
    if (read_size != size) {
        (void)fprintf(stderr, "%s: expected %zu bytes, got %zu\n", path, size, read_size);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        (void)fprintf(stderr, "%s: failed to close file\n", path);
        return 1;
    }

    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        (void)fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        (void)fprintf(
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

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual != expected) {
        (void)fprintf(
            stderr,
            "%s: expected %llu, got %llu\n",
            context,
            (unsigned long long)expected,
            (unsigned long long)actual
        );
        return 1;
    }

    return 0;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        (void)fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        (void)fprintf(
            stderr,
            "%s: expected text [%s], got [%s]\n",
            context,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        (void)fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "NULL" : actual,
            needle == NULL ? "NULL" : needle
        );
        return 1;
    }

    return 0;
}

static int expect_true(int condition, const char *context) {
    if (!condition) {
        (void)fprintf(stderr, "%s: expected true\n", context);
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
    if (actual == NULL || expected == NULL || memcmp(actual, expected, size) != 0) {
        (void)fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }

    return 0;
}
