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
    test_path_suffix_capacity = 16,
    mysql_error_no_referenced_row = 1452,
    mysql_error_row_is_referenced = 1451,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_replace_key_values_and_set(void);
static int test_replace_key_auto_increment_and_foreign_keys(void);
static int test_replace_key_persistence_rename_and_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_replace_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
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
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
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

    failures += test_replace_key_values_and_set();
    failures += test_replace_key_auto_increment_and_foreign_keys();
    failures += test_replace_key_persistence_rename_and_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_replace_key_values_and_set(void) {
    static const char *const pk_diff_rows[] = {"1", "20"};
    static const char *const pk_same_rows[] = {"1", "10"};
    static const char *const set_pk_rows[] = {"1", "20"};
    static const char *const uniq_rows[] = {"2", "10", "200"};
    static const char *const multi_unique_rows[] = {"1", "4", "99"};
    static const char *const composite_rows[] = {"1", "2", "20"};
    static const char *const string_key_rows[] = {"abc", "20"};
    static const char *const prefix_key_rows[] = {"abczzz", "20"};
    static const char *const nullable_unique_rows[] = {
        "1",
        NULL,
        "10",
        "2",
        NULL,
        "20",
    };
    static const char *const multi_replace_rows[] = {
        "1",
        "20",
        "2",
        "30",
    };
    char path[test_path_capacity];
    unsigned char preamble_before[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char preamble_after[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    uint64_t catalog_generation_before = 0U;
    uint64_t sqlite_generation_before = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values_set") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open replace key database");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE pk_diff(id INT PRIMARY KEY, v INT)");
    failures += expect_statement_ok(database, "CREATE TABLE pk_same(id INT PRIMARY KEY, v INT)");
    failures += expect_statement_ok(database, "CREATE TABLE set_pk(id INT PRIMARY KEY, v INT)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE uniq_one(id INT PRIMARY KEY, u INT UNIQUE, v INT)"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE multi_unique(a INT UNIQUE, b INT UNIQUE, v INT)"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE comp_pk(a INT, b INT, v INT, PRIMARY KEY(a,b))"
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE str_pk(id VARCHAR(10) PRIMARY KEY, v INT)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE prefix_u(name VARCHAR(10), v INT, UNIQUE KEY name_u(name(3)))"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE nullable_unique(id INT PRIMARY KEY, u INT UNIQUE, v INT)"
    );
    failures += expect_statement_ok(database, "CREATE TABLE multi_rows(id INT PRIMARY KEY, v INT)");

    if (failures == 0) {
        failures += expect_int(
            read_file_at(path, 0L, preamble_before, sizeof(preamble_before)),
            0,
            "read replace key preamble before writes"
        );
        catalog_generation_before = database->catalog.generation;
        sqlite_generation_before = database->session.sqlite_schema_generation;
    }

    failures += expect_statement_ok(database, "INSERT INTO pk_diff VALUES(1,10)");
    failures += expect_replace_ok(database, "REPLACE INTO pk_diff VALUES(1,20)", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM pk_diff",
            .values = pk_diff_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "primary key changed replacement",
        }
    );

    failures += expect_statement_ok(database, "INSERT INTO pk_same VALUES(1,10)");
    failures += expect_replace_ok(database, "REPLACE INTO pk_same VALUES(1,10)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM pk_same",
            .values = pk_same_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "primary key exact replacement",
        }
    );

    failures += expect_statement_ok(database, "INSERT INTO set_pk VALUES(1,10)");
    failures += expect_replace_ok(database, "REPLACE INTO set_pk SET id=1, v=10", 1);
    failures += expect_replace_ok(database, "REPLACE INTO set_pk SET id=1, v=20", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM set_pk",
            .values = set_pk_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "replace set key replacement",
        }
    );

    failures += expect_statement_ok(database, "INSERT INTO uniq_one VALUES(1,10,100)");
    failures += expect_replace_ok(database, "REPLACE INTO uniq_one VALUES(2,10,200)", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, u, v FROM uniq_one",
            .values = uniq_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "unique secondary replacement",
        }
    );

    failures += expect_statement_ok(database, "INSERT INTO multi_unique VALUES(1,2,12),(3,4,34)");
    failures += expect_replace_ok(database, "REPLACE INTO multi_unique VALUES(1,4,99)", 3);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM multi_unique",
            .values = multi_unique_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "multiple unique conflict replacement",
        }
    );

    failures += expect_statement_ok(database, "INSERT INTO comp_pk VALUES(1,2,10)");
    failures += expect_replace_ok(database, "REPLACE INTO comp_pk VALUES(1,2,20)", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM comp_pk",
            .values = composite_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "composite primary replacement",
        }
    );

    failures += expect_statement_ok(database, "INSERT INTO str_pk VALUES('abc',10)");
    failures += expect_replace_ok(database, "REPLACE INTO str_pk VALUES('abc',20)", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM str_pk",
            .values = string_key_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "varchar primary replacement",
        }
    );

    failures += expect_statement_ok(database, "INSERT INTO prefix_u VALUES('abcdef',10)");
    failures += expect_replace_ok(database, "REPLACE INTO prefix_u VALUES('abczzz',20)", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT name, v FROM prefix_u",
            .values = prefix_key_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "prefix unique replacement",
        }
    );

    failures += expect_statement_ok(database, "INSERT INTO nullable_unique VALUES(1,NULL,10)");
    failures += expect_replace_ok(database, "REPLACE INTO nullable_unique VALUES(2,NULL,20)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, u, v FROM nullable_unique ORDER BY id",
            .values = nullable_unique_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "nullable unique null replacement",
        }
    );

    failures +=
        expect_replace_ok(database, "REPLACE INTO multi_rows VALUES(1,10),(1,20),(2,30)", 4);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM multi_rows ORDER BY id",
            .values = multi_replace_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "multi-row replace within statement",
        }
    );

    failures += expect_uint64(
        database->catalog.generation,
        catalog_generation_before,
        "replace key leaves catalog generation unchanged"
    );
    failures += expect_uint64(
        database->session.sqlite_schema_generation,
        sqlite_generation_before,
        "replace key leaves SQLite schema generation unchanged"
    );
    failures += expect_int(
        read_file_at(path, 0L, preamble_after, sizeof(preamble_after)),
        0,
        "read replace key preamble after writes"
    );
    failures += expect_bytes(
        preamble_after,
        preamble_before,
        sizeof(preamble_before),
        "replace key preserves preamble"
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_replace_key_auto_increment_and_foreign_keys(void) {
    static const char *const ai_first_id[] = {"1"};
    static const char *const ai_second_id[] = {"2"};
    static const char *const ai_explicit_last_id[] = {"2"};
    static const char *const ai_next_generated_last_id[] = {"8"};
    static const char *const ai_rows[] = {
        "7",
        "10",
        "700",
        "8",
        "20",
        "300",
    };
    static const char *const parent_rows[] = {"1", "10"};
    static const char *const child_rows[] = {"1", "1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "auto_fk") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open replace key auto fk");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ai_sec(id INT AUTO_INCREMENT PRIMARY KEY, u INT UNIQUE, v INT)"
    );
    failures += expect_replace_ok(database, "REPLACE INTO ai_sec(u,v) VALUES(10,100)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = ai_first_id,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first generated replace id",
        }
    );
    failures += expect_replace_ok(database, "REPLACE INTO ai_sec(u,v) VALUES(10,200)", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = ai_second_id,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second generated replace id",
        }
    );
    failures += expect_replace_ok(database, "REPLACE INTO ai_sec(id,u,v) VALUES(7,10,700)", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = ai_explicit_last_id,
            .column_count = 1U,
            .row_count = 1U,
            .context = "explicit replace leaves last insert id",
        }
    );
    failures += expect_replace_ok(database, "REPLACE INTO ai_sec(u,v) VALUES(20,300)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = ai_next_generated_last_id,
            .column_count = 1U,
            .row_count = 1U,
            .context = "generated replace after explicit value",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, u, v FROM ai_sec ORDER BY id",
            .values = ai_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "auto increment replace rows",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE parent(id INT PRIMARY KEY, v INT)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE child(id INT PRIMARY KEY, parent_id INT, "
        "FOREIGN KEY(parent_id) REFERENCES parent(id))"
    );
    failures += expect_statement_ok(database, "INSERT INTO parent VALUES(1,10)");
    failures += expect_statement_ok(database, "INSERT INTO child VALUES(1,1)");
    failures += execute_error(
        database,
        "REPLACE INTO parent VALUES(1,10)",
        (struct expected_sql_error){
            .code = mysql_error_row_is_referenced,
            .sqlstate = "23000",
            .message_part = "Cannot delete or update a parent row",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM parent",
            .values = parent_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "parent replace rollback",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO child VALUES(1,2)",
        (struct expected_sql_error){
            .code = mysql_error_no_referenced_row,
            .sqlstate = "23000",
            .message_part = "Cannot add or update a child row",
        }
    );
    failures += expect_replace_ok(database, "REPLACE INTO child VALUES(1,1)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, parent_id FROM child",
            .values = child_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "child exact replace",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_replace_key_persistence_rename_and_independent_handles(void) {
    static const char *const reopened_rows[] = {"abc", "30"};
    static const char *const first_rows[] = {"1", "11"};
    static const char *const second_rows[] = {"1", "22"};
    char path[test_path_capacity];
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "persistence") != 0 ||
        make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(path);
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open replace key persist");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures +=
        expect_statement_ok(database, "CREATE TABLE keyed(id VARCHAR(10) PRIMARY KEY, v INT)");
    failures += expect_replace_ok(database, "REPLACE INTO keyed VALUES('abc',10)", 1);
    failures += expect_statement_ok(database, "RENAME TABLE keyed TO renamed_keyed");
    failures += expect_replace_ok(database, "REPLACE INTO renamed_keyed VALUES('abc',30)", 2);
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen replace key persist");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM renamed_keyed",
            .values = reopened_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "reopened replace key row",
        }
    );
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first replace key");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second replace key");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t(id INT PRIMARY KEY, v INT)");
    failures += expect_statement_ok(second, "CREATE TABLE t(id INT PRIMARY KEY, v INT)");
    failures += expect_replace_ok(first, "REPLACE INTO t VALUES(1,10)", 1);
    failures += expect_replace_ok(second, "REPLACE INTO t VALUES(1,20)", 1);
    failures += expect_replace_ok(first, "REPLACE INTO t VALUES(1,11)", 2);
    failures += expect_replace_ok(second, "REPLACE INTO t VALUES(1,22)", 2);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, v FROM t",
            .values = first_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "first independent replace key rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, v FROM t",
            .values = second_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second independent replace key rows",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(path);
    remove_related_files(first_path);
    remove_related_files(second_path);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    mylite_result_free(result);

    return failures;
}

static int expect_replace_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "replace key column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "replace key row count");
    failures += expect_int64(
        mylite_result_affected_rows(result),
        affected_rows,
        "replace key affected rows"
    );
    failures += expect_size(mylite_result_warning_count(result), 0U, "replace key warning count");
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
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

    if (expected == NULL) {
        return expect_true(actual == NULL, context);
    }
    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "%s/mylite_replace_key_lifecycle_%d_%s.mylite",
        getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path too long\n");
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
    remove(path);
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-journal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        perror("fopen");
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        perror("fseek");
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        perror("fread");
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

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
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
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: condition failed\n", context);
        return 1;
    }
    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(stderr, "%s: expected '%s' to contain '%s'\n", context, actual, needle);
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
