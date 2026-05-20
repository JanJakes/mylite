#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"

#include <stdbool.h>
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
    test_path_capacity = 1024,
    show_columns_field_count = 6,
    show_index_field_count = 15,
    numbers_column_count = 6,
    row_count_text_capacity = 32,
    physical_drop_sql_capacity = 256,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_unknown = 1105,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_incorrect_column_name = 1166,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_invalid_default = 1067,
    mysql_error_invalid_use_of_null = 1138,
    mysql_error_duplicate_key = 1062,
    mysql_error_incorrect_prefix_key = 1089,
    mysql_error_primary_key_part_null = 1171,
    mysql_error_data_out_of_range = 1264,
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

static int test_modify_column_success_persistence_and_dml(void);
static int test_modify_column_keyed_tables(void);
static int test_modify_column_diagnostics_and_rollback(void);
static int test_modify_column_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_modify_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_column_descriptor(
    mylite_db *database,
    int64_t table_id,
    const char *name,
    int64_t ordinal_position,
    const char *logical_type,
    bool is_nullable,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int execute_sql(sqlite3 *connection, const char *sql);
static int drop_physical_table(sqlite3 *connection, const char *physical_name);
static int expect_physical_index_count(
    mylite_db *database,
    int expected_count,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
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

    failures += test_modify_column_success_persistence_and_dml();
    failures += test_modify_column_keyed_tables();
    failures += test_modify_column_diagnostics_and_rollback();
    failures += test_modify_column_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_modify_column_success_persistence_and_dml(void) {
    static const char *const rows_after_type_change[] = {"1", "2", "3", "2", "6", "7"};
    static const char *const nullable_insert_row[] = {"3", "4", "11", "12"};
    static const char *const show_columns_after_modify[] = {
        "id",  "int",
        "NO",  "",
        NULL,  "",
        "i",   "bigint",
        "YES", "",
        NULL,  "",
        "iu",  "bigint unsigned",
        "NO",  "",
        NULL,  "",
        "b",   "int",
        "YES", "",
        NULL,  "",
        "bu",  "bigint unsigned",
        "YES", "",
        NULL,  "",
        "n",   "int",
        "YES", "",
        NULL,  "",
    };
    static const char *const show_create_after_modify[] = {
        "numbers",
        "CREATE TABLE `numbers` (\n"
        "  `id` int NOT NULL,\n"
        "  `i` bigint DEFAULT NULL,\n"
        "  `iu` bigint unsigned NOT NULL,\n"
        "  `b` int DEFAULT NULL,\n"
        "  `bu` bigint unsigned DEFAULT NULL,\n"
        "  `n` int DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const positioned_columns[] = {
        "c", "int", "YES", "", NULL, "", "id", "int",    "NO",  "", NULL, "",
        "b", "int", "YES", "", NULL, "", "a",  "bigint", "YES", "", NULL, "",
    };
    static const char *const positioned_ordinals[] = {
        "c",
        "1",
        "id",
        "2",
        "b",
        "3",
        "a",
        "4",
    };
    static const char *const positioned_rows[] = {"4", "1", "3", "2", "8", "5", "7", "6"};
    static const char *const modify_temporal_columns[] = {
        "dt",
        "datetime",
        "YES",
        "",
        NULL,
        "on update CURRENT_TIMESTAMP",
        "id",
        "int",
        "YES",
        "",
        NULL,
        "",
        "ts",
        "datetime",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const modify_temporal_rows[] = {
        "2020-01-01 01:02:03",
        "1",
        "2020-01-01 01:02:03",
        "2021-02-03 04:05:06",
        "2",
        NULL,
    };
    static const char *const case_column_rows[] = {"mixed", "int", "YES", "", NULL, ""};
    static const char *const persisted_rows[] = {"1", "2", "2", "6", "3", "4"};
    static const char *const renamed_rows[] = {"9"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t noop_catalog_generation = 0U;
    uint64_t noop_sqlite_generation = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE numbers ("
        "id INT NOT NULL, i INT NOT NULL, iu INT UNSIGNED, b BIGINT, "
        "bu BIGINT UNSIGNED, n INT NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO numbers VALUES (1, 2, 3, 4, 5, NULL), (2, 6, 7, 8, 9, 10)",
        &result
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_modify_ok(database, "ALTER TABLE numbers MODIFY i BIGINT", 2);
    failures +=
        expect_modify_ok(database, "ALTER TABLE numbers MODIFY iu BIGINT UNSIGNED NOT NULL", 2);
    failures += expect_modify_ok(database, "ALTER TABLE numbers MODIFY COLUMN b INT", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, iu FROM numbers ORDER BY id",
            .values = rows_after_type_change,
            .column_count = 3U,
            .row_count = 2U,
            .context = "rows preserved after modify",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM numbers",
            .values = show_columns_after_modify,
            .column_count = show_columns_field_count,
            .row_count = numbers_column_count,
            .context = "show columns after modify",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE numbers",
            .values = show_create_after_modify,
            .column_count = 2U,
            .row_count = 1U,
            .context = "show create after modify",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE positioned (id INT NOT NULL, a INT, b INT, c INT)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "INSERT INTO positioned VALUES (1, 2, 3, 4), (5, 6, 7, 8)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_modify_ok(database, "ALTER TABLE positioned MODIFY c INT FIRST", 0);
    failures += expect_modify_ok(database, "ALTER TABLE positioned MODIFY a BIGINT AFTER b", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM positioned",
            .values = positioned_columns,
            .column_count = show_columns_field_count,
            .row_count = 4U,
            .context = "show columns after modify positioning",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'positioned' "
                   "ORDER BY ORDINAL_POSITION",
            .values = positioned_ordinals,
            .column_count = 2U,
            .row_count = 4U,
            .context = "information schema ordinals after modify positioning",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM positioned ORDER BY id",
            .values = positioned_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "select star after modify positioning",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE modify_temporal (id INT, dt DATETIME NULL, ts TIMESTAMP NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO modify_temporal VALUES "
        "(1, '2020-01-01 01:02:03', '2020-01-01 01:02:03'), "
        "(2, '2021-02-03 04:05:06', NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_modify_ok(
        database,
        "ALTER TABLE modify_temporal MODIFY dt DATETIME NULL ON UPDATE CURRENT_TIMESTAMP FIRST",
        0
    );
    failures += expect_modify_ok(
        database,
        "ALTER TABLE modify_temporal MODIFY ts DATETIME NULL AFTER id",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM modify_temporal",
            .values = modify_temporal_columns,
            .column_count = show_columns_field_count,
            .row_count = 3U,
            .context = "show columns after modify temporal positioning",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM modify_temporal ORDER BY id",
            .values = modify_temporal_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "rows after modify temporal positioning",
        }
    );

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "numbers", &table),
        MYLITE_OK,
        "read numbers descriptor"
    );
    failures +=
        expect_column_descriptor(database, table.table_id, "i", 2, "BIGINT", true, "i descriptor");
    failures += expect_column_descriptor(
        database,
        table.table_id,
        "iu",
        3,
        "BIGINT UNSIGNED",
        false,
        "iu descriptor"
    );
    failures +=
        expect_column_descriptor(database, table.table_id, "b", 4, "INT", true, "b descriptor");

    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    noop_catalog_generation = catalog == NULL ? 0U : catalog->generation;
    noop_sqlite_generation = session == NULL ? 0U : session->sqlite_schema_generation;
    failures += expect_modify_ok(database, "ALTER TABLE numbers MODIFY i BIGINT NULL", 0);
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    failures += expect_size(
        catalog == NULL ? 0U : (size_t)catalog->generation,
        (size_t)noop_catalog_generation,
        "same definition leaves catalog generation"
    );
    failures += expect_size(
        session == NULL ? 0U : (size_t)session->sqlite_schema_generation,
        (size_t)noop_sqlite_generation,
        "same definition leaves SQLite schema generation"
    );

    failures += execute_ok(database, "CREATE TABLE case_columns (MiXeD INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO case_columns VALUES (1)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_modify_ok(database, "ALTER TABLE case_columns MODIFY mixed INT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM case_columns",
            .values = case_column_rows,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "case-only spelling update",
        }
    );

    failures += expect_modify_ok(database, "ALTER TABLE numbers MODIFY b INT NOT NULL", 0);
    failures += expect_modify_ok(database, "ALTER TABLE numbers MODIFY b INT", 0);

    failures +=
        execute_ok(database, "INSERT INTO numbers VALUES (3, NULL, 11, 12, 13, NULL)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "UPDATE numbers SET i = 4 WHERE id = 3", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 1, "update after modify");
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, iu, b FROM numbers WHERE id = 3",
            .values = nullable_insert_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "insert and update after modify",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE app.qualified (id INT NOT NULL, n INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO app.qualified VALUES (1, 2), (2, 3)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_modify_ok(database, "ALTER TABLE app.qualified MODIFY n BIGINT NOT NULL", 2);

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "modify preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i FROM numbers ORDER BY id",
            .values = persisted_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "modified table persists after reopen",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE rename_source (id INT NOT NULL, n INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO rename_source VALUES (1, 9)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "RENAME TABLE rename_source TO renamed_target", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_modify_ok(database, "ALTER TABLE renamed_target MODIFY n BIGINT", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT n FROM renamed_target",
            .values = renamed_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "modify after table rename",
        }
    );
    failures += execute_ok(database, "DROP TABLE renamed_target", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "ALTER TABLE renamed_target MODIFY n INT",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_target' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_modify_column_keyed_tables(void) {
    static const char *const rows_after_non_key_modify[] = {
        "1",
        "aa",
        "10",
        "100",
        "2",
        "bb",
        "20",
        "200",
    };
    static const char *const statistic_count_rows[] = {"4"};
    static const char *const prefix_index_rows[] = {"k", "10"};
    static const char *const primary_column_rows[] = {"bigint", "NO", "PRI"};
    static const char *const fulltext_show_create_rows[] = {
        "fulltext_modify",
        "CREATE TABLE `fulltext_modify` (\n"
        "  `body` varchar(120) DEFAULT NULL,\n"
        "  `title` varchar(40) DEFAULT NULL,\n"
        "  FULLTEXT KEY `ft_body` (`body`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const fulltext_index_rows[] = {"FULLTEXT", "body"};
    static const char *const fulltext_rows[] = {
        "alpha body",
        "first",
        "beta body",
        "second",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "keyed") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open keyed file");
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE keyed_modify ("
        "id INT NOT NULL PRIMARY KEY, "
        "k VARCHAR(20) NOT NULL, "
        "u INT, "
        "v INT, "
        "UNIQUE KEY u_idx (u), "
        "KEY k_idx (k(10)), "
        "KEY v_idx (v))",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO keyed_modify VALUES (1, 'aa', 10, 100), (2, 'bb', 20, 200)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_physical_index_count(database, 4, "physical indexes before keyed modify");

    failures += expect_modify_ok(database, "ALTER TABLE keyed_modify MODIFY v BIGINT", 2);
    failures += expect_physical_index_count(database, 4, "physical indexes after keyed modify");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, k, u, v FROM keyed_modify ORDER BY id",
            .values = rows_after_non_key_modify,
            .column_count = 4U,
            .row_count = 2U,
            .context = "rows preserved after keyed non-key modify",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'keyed_modify'",
            .values = statistic_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "key descriptors preserved after keyed modify",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, SUB_PART FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'keyed_modify' "
                   "AND INDEX_NAME = 'k_idx'",
            .values = prefix_index_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "prefix index descriptor after keyed modify",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO keyed_modify VALUES (3, 'cc', 20, 300)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '20' for key 'keyed_modify.u_idx'",
        }
    );

    failures += expect_modify_ok(database, "ALTER TABLE keyed_modify MODIFY id BIGINT", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_TYPE, IS_NULLABLE, COLUMN_KEY "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'keyed_modify' "
                   "AND COLUMN_NAME = 'id'",
            .values = primary_column_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "primary key nullability preserved after omitted nullability",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE keyed_modify MODIFY id BIGINT NULL",
        (struct expected_sql_error){
            .code = mysql_error_primary_key_part_null,
            .sqlstate = "42000",
            .message_part = "All parts of a PRIMARY KEY must be NOT NULL",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE fulltext_modify ("
        "body VARCHAR(100), "
        "title VARCHAR(40), "
        "FULLTEXT KEY ft_body (body))",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO fulltext_modify VALUES ('alpha body', 'first'), ('beta body', 'second')",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_physical_index_count(
        database,
        4,
        "metadata fulltext creates no physical index before modify"
    );
    failures +=
        expect_modify_ok(database, "ALTER TABLE fulltext_modify MODIFY body VARCHAR(120)", 2);
    failures += expect_physical_index_count(
        database,
        4,
        "metadata fulltext creates no physical index after modify"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE fulltext_modify",
            .values = fulltext_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "fulltext preserved after modify in SHOW CREATE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_TYPE, COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'fulltext_modify' "
                   "AND INDEX_NAME = 'ft_body'",
            .values = fulltext_index_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "fulltext preserved after modify in statistics",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT body, title FROM fulltext_modify ORDER BY title",
            .values = fulltext_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "fulltext rows preserved after modify",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE prefix_shrink (k VARCHAR(20), KEY k_idx (k(10)))",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "ALTER TABLE prefix_shrink MODIFY k VARCHAR(8)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_prefix_key,
            .sqlstate = "HY000",
            .message_part = "Incorrect prefix key",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_modify_column_diagnostics_and_rollback(void) {
    static const char *const unchanged_nullable[] = {NULL, "1"};
    static const char *const unchanged_range[] = {"2147483648"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "ALTER TABLE numbers MODIFY n BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.numbers MODIFY n BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_reserved.numbers MODIFY n BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name",
        }
    );

    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE numbers (id INT NOT NULL, n INT, nn INT NOT NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO numbers VALUES (1, NULL, 5), (2, 1, 6)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "ALTER TABLE missing_table MODIFY n BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_reserved MODIFY n BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers MODIFY _mylite_hidden BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_column_name,
            .sqlstate = "42000",
            .message_part = "Incorrect column name",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers MODIFY missing BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'numbers'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers MODIFY n BIGINT NOT NULL",
        (struct expected_sql_error){
            .code = mysql_error_invalid_use_of_null,
            .sqlstate = "22004",
            .message_part = "Invalid use of NULL value",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT n FROM numbers ORDER BY id",
            .values = unchanged_nullable,
            .column_count = 1U,
            .row_count = 2U,
            .context = "NULL validation failure preserves rows",
        }
    );

    failures += execute_ok(database, "CREATE TABLE range_bad (c BIGINT NOT NULL)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO range_bad VALUES (2147483648)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "ALTER TABLE range_bad MODIFY c INT NOT NULL",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'c' at row 1",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT c FROM range_bad",
            .values = unchanged_range,
            .column_count = 1U,
            .row_count = 1U,
            .context = "range validation failure preserves rows",
        }
    );

    failures += execute_ok(database, "CREATE TABLE unsigned_bad (c INT NOT NULL)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO unsigned_bad VALUES (-1), (0)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "ALTER TABLE unsigned_bad MODIFY c INT UNSIGNED NOT NULL",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'c' at row 1",
        }
    );

    failures += execute_error(
        database,
        "ALTER TABLE numbers MODIFY numbers.n BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers MODIFY n BIGINT DEFAULT 'abc'",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'n'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers MODIFY n BIGINT AFTER n",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'n' in 'numbers'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers MODIFY n BIGINT, MODIFY nn BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers MODIFY n VARCHAR(10)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ALTER TABLE MODIFY COLUMN supports only baseline integer, character, "
                            "and temporal columns",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers MODIFY n BIGINT, ALGORITHM=INSTANT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE rowid_shadow (rowid INT, _rowid_ INT, oid INT)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO rowid_shadow VALUES (1, 2, 3)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "ALTER TABLE rowid_shadow MODIFY rowid BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ALTER TABLE MODIFY COLUMN requires an unshadowed SQLite rowid alias",
        }
    );

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read physical failure schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "numbers", &table),
        MYLITE_OK,
        "read physical failure table"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += drop_physical_table(sqlite, table.physical_name);
    }
    failures += execute_error(
        database,
        "ALTER TABLE numbers MODIFY nn BIGINT NOT NULL",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite row operation failed",
        }
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        "nn",
        3,
        "INT",
        false,
        "physical failure preserves descriptor"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_modify_column_independent_handles(void) {
    static const char *const first_columns[] = {"n", "bigint", "YES", "", NULL, ""};
    static const char *const second_columns[] = {"n", "int", "YES", "", NULL, ""};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += execute_ok(first, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "CREATE TABLE app.t (n INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "CREATE TABLE app.t (n INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "INSERT INTO app.t VALUES (1)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "INSERT INTO app.t VALUES (2)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += expect_modify_ok(first, "ALTER TABLE app.t MODIFY n BIGINT", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM app.t",
            .values = first_columns,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "first independent descriptor",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM app.t",
            .values = second_columns,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "second independent descriptor",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected success for %s: rc=%d err=%d state=%s msg=%s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (out_result == NULL) {
        mylite_result_free(result);
    } else {
        *out_result = result;
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    const char *message = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "expected error for %s, got rc=%d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    message = mylite_errmsg(database);
    if (expected.message_part != NULL) {
        failures += expect_contains(message, expected.message_part, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_modify_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    char row_count_text[row_count_text_capacity];
    const char *row_count_values[] = {row_count_text};
    mylite_result *result = NULL;
    int written =
        snprintf(row_count_text, sizeof(row_count_text), "%lld", (long long)affected_rows);
    int failures = execute_ok(database, sql, &result);

    if (written < 0 || (size_t)written >= sizeof(row_count_text)) {
        fprintf(stderr, "failed to format expected affected rows\n");
        failures += 1;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, "modify column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "modify row count");
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, "modify affected");
    failures += expect_size(mylite_result_warning_count(result), 0U, "modify warning count");
    mylite_result_free(result);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .values = row_count_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "ROW_COUNT after modify",
        }
    );

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    size_t value_count = query.row_count * query.column_count;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t index = 0U; index < value_count; ++index) {
        failures += expect_result_value(
            result,
            index / query.column_count,
            index % query.column_count,
            query.values[index],
            query.context
        );
    }
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

static int expect_column_descriptor(
    mylite_db *database,
    int64_t table_id,
    const char *name,
    int64_t ordinal_position,
    const char *logical_type,
    bool is_nullable,
    const char *context
) {
    struct mylite_catalog_column_descriptor column = {0};
    int failures = expect_int(
        mylite_catalog_read_column_by_name(database, table_id, name, &column),
        MYLITE_OK,
        context
    );

    failures += expect_int64(column.ordinal_position, ordinal_position, context);
    failures += expect_text(column.name, name, context);
    failures += expect_text(column.logical_type, logical_type, context);
    failures += expect_text(column.physical_type, "INTEGER", context);
    failures += expect_true(column.is_nullable == is_nullable, context);
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
        "%s/mylite_alter_table_modify_column_%d_%s.mylite",
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

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        (void)fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
        return 1;
    }

    return 0;
}

static int execute_sql(sqlite3 *connection, const char *sql) {
    char *message = NULL;
    int rc = sqlite3_exec(connection, sql, NULL, NULL, &message);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "sqlite exec failed for %s: %s\n", sql, message == NULL ? "" : message);
        sqlite3_free(message);
        return 1;
    }

    return 0;
}

static int drop_physical_table(sqlite3 *connection, const char *physical_name) {
    char sql[physical_drop_sql_capacity];
    int written = snprintf(sql, sizeof(sql), "DROP TABLE \"%s\"", physical_name);

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "physical drop SQL is too long\n");
        return 1;
    }

    return execute_sql(connection, sql);
}

static int expect_physical_index_count(
    mylite_db *database,
    int expected_count,
    const char *context
) {
    enum { sqlite_use_nul_terminated_string = -1 };

    sqlite3 *connection = mylite_connection_sqlite_for_test(database);
    sqlite3_stmt *statement = NULL;
    int actual_count = 0;
    int rc = SQLITE_OK;

    if (connection == NULL) {
        fprintf(stderr, "%s: missing SQLite test connection\n", context);
        return 1;
    }

    rc = sqlite3_prepare_v2(
        connection,
        "SELECT count(*) FROM sqlite_schema "
        "WHERE type = 'index' AND name GLOB '_mylite_user_index_*'",
        sqlite_use_nul_terminated_string,
        &statement,
        NULL
    );
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s: prepare physical index query failed: %d\n", context, rc);
        return 1;
    }

    rc = sqlite3_step(statement);
    if (rc == SQLITE_ROW) {
        actual_count = sqlite3_column_int(statement, 0);
        rc = SQLITE_OK;
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && rc == SQLITE_OK) {
        rc = SQLITE_ERROR;
    }
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s: physical index query failed: %d\n", context, rc);
        return 1;
    }

    return expect_int(actual_count, expected_count, context);
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

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected '%s', got '%s'\n",
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
            "%s: expected '%s' to contain '%s'\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
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
