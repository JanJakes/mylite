#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"

#include <stdbool.h>
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
    sql_capacity = 512,
    show_columns_column_count = 6,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_unknown = 1105,
    mysql_error_duplicate_column = 1060,
    mysql_error_duplicate_key = 1062,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_incorrect_column_name = 1166,
    mysql_error_no_referenced_row = 1452,
    mysql_error_table_does_not_exist = 1146,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t row_count;
    size_t column_count;
    const char *context;
};

struct physical_index_name_glob {
    const char *value;
};

struct physical_index_sql_needle {
    const char *value;
};

static int test_rename_column_success_descriptor_persistence_and_dml(void);
static int test_rename_column_keyed_dependencies(void);
static int test_rename_column_diagnostics(void);
static int test_rename_column_physical_failure_preserves_catalog(void);
static int test_rename_column_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_ddl_result(const mylite_result *result, const char *context);
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
static int expect_missing_column_descriptor(
    mylite_db *database,
    int64_t table_id,
    const char *name,
    const char *context
);
static int expect_physical_index_count(
    mylite_db *database,
    int expected_count,
    const char *context
);
static int expect_physical_index_sql_contains(
    mylite_db *database,
    struct physical_index_name_glob name_glob,
    struct physical_index_sql_needle needle,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int execute_sql(sqlite3 *connection, const char *sql);
static int rename_physical_column(
    sqlite3 *connection,
    const char *physical_name,
    const char *old_name,
    const char *new_name
);
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

    failures += test_rename_column_success_descriptor_persistence_and_dml();
    failures += test_rename_column_keyed_dependencies();
    failures += test_rename_column_diagnostics();
    failures += test_rename_column_physical_failure_preserves_catalog();
    failures += test_rename_column_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_rename_column_success_descriptor_persistence_and_dml(void) {
    static const char *const row_count_zero[] = {"0"};
    static const char *const warning_count_zero[] = {"0"};
    static const char *const rows_after_rename[] = {
        "1",
        NULL,
        "10",
        "2",
        "20",
        "30",
    };
    static const char *const rows_after_dml[] = {
        "1",
        NULL,
        "10",
        "2",
        "20",
        "30",
        "3",
        "-5",
        "40",
    };
    static const char *const edge_rows_after_first_rename[] = {
        "1",
        "2",
        "3",
        "4",
        "5",
        "6",
    };
    static const char *const edge_rows_after_last_rename[] = {
        "1",
        "2",
        "3",
        "4",
        "5",
        "6",
    };
    static const char *const cleanup_after_delete[] = {"2", "20"};
    static const char *const show_columns_rows[] = {
        "id",
        "int",
        "NO",
        "",
        NULL,
        "",
        "n",
        "int",
        "YES",
        "",
        NULL,
        "",
        "nn",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
    };
    static const char *const show_case_columns_rows[] = {
        "id",
        "int",
        "NO",
        "",
        NULL,
        "",
        "N",
        "int",
        "YES",
        "",
        NULL,
        "",
        "nn",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
    };
    static const char *const show_create_rows[] = {
        "numbers",
        "CREATE TABLE `numbers` (\n"
        "  `id` int NOT NULL,\n"
        "  `n` int DEFAULT NULL,\n"
        "  `nn` bigint unsigned NOT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const show_case_create_rows[] = {
        "numbers",
        "CREATE TABLE `numbers` (\n"
        "  `id` int NOT NULL,\n"
        "  `N` int DEFAULT NULL,\n"
        "  `nn` bigint unsigned NOT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor before_table = {0};
    struct mylite_catalog_table_descriptor after_table = {0};
    struct mylite_catalog_table_descriptor noop_before_table = {0};
    struct mylite_catalog_table_descriptor noop_after_table = {0};
    struct mylite_catalog_table_descriptor edge_table = {0};
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation_before = 0U;
    uint64_t sqlite_generation_before = 0U;
    uint64_t noop_catalog_generation = 0U;
    uint64_t noop_sqlite_generation = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open rename-column file");
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    failures += expect_ddl_result(result, "use schema result");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE numbers (id INT NOT NULL, i INT, nn BIGINT UNSIGNED NOT NULL)",
        &result
    );
    failures += expect_ddl_result(result, "create numbers result");
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "INSERT INTO numbers VALUES (1, NULL, 10), (2, 20, 30)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read app schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "numbers", &before_table),
        MYLITE_OK,
        "read numbers table before rename"
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        catalog_generation_before = catalog->generation;
    }
    if (session != NULL) {
        sqlite_generation_before = session->sqlite_schema_generation;
    }

    failures += execute_ok(database, "ALTER TABLE numbers RENAME COLUMN i TO n", &result);
    failures += expect_ddl_result(result, "rename column result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "numbers", &after_table),
        MYLITE_OK,
        "read numbers table after rename"
    );
    failures += expect_int64(after_table.table_id, before_table.table_id, "table id unchanged");
    failures += expect_text(
        after_table.physical_name,
        before_table.physical_name,
        "physical name unchanged"
    );
    failures += expect_uint64(
        after_table.descriptor_version,
        before_table.descriptor_version + 1U,
        "rename bumps table descriptor version"
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            catalog_generation_before + 1U,
            "rename bumps catalog generation"
        );
    }
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before + 1U,
            "rename bumps SQLite schema generation"
        );
    }
    failures += expect_missing_column_descriptor(
        database,
        before_table.table_id,
        "i",
        "old column descriptor removed"
    );
    failures += expect_column_descriptor(
        database,
        before_table.table_id,
        "n",
        2,
        "INT",
        true,
        "renamed column descriptor"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .values = row_count_zero,
            .row_count = 1U,
            .column_count = 1U,
            .context = "row count after rename column",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count",
            .values = warning_count_zero,
            .row_count = 1U,
            .column_count = 1U,
            .context = "warning count after rename column",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, nn FROM numbers ORDER BY id",
            .values = rows_after_rename,
            .row_count = 2U,
            .column_count = 3U,
            .context = "rows after rename",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM numbers",
            .values = show_columns_rows,
            .row_count = 3U,
            .column_count = show_columns_column_count,
            .context = "show columns after rename",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE numbers",
            .values = show_create_rows,
            .row_count = 1U,
            .column_count = 2U,
            .context = "show create table after rename",
        }
    );

    failures += expect_int(
        mylite_catalog_read_table_by_name(
            database,
            schema.schema_id,
            "numbers",
            &noop_before_table
        ),
        MYLITE_OK,
        "read numbers table before no-op rename"
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        noop_catalog_generation = catalog->generation;
    }
    if (session != NULL) {
        noop_sqlite_generation = session->sqlite_schema_generation;
    }
    failures += execute_ok(database, "ALTER TABLE numbers RENAME COLUMN n TO n", &result);
    failures += expect_ddl_result(result, "same-name rename result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "numbers", &noop_after_table),
        MYLITE_OK,
        "read numbers table after no-op rename"
    );
    failures += expect_uint64(
        noop_after_table.descriptor_version,
        noop_before_table.descriptor_version,
        "same-name rename leaves table descriptor version"
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            noop_catalog_generation,
            "same-name rename leaves catalog generation"
        );
    }
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            noop_sqlite_generation,
            "same-name rename leaves SQLite schema generation"
        );
    }

    failures += execute_ok(database, "ALTER TABLE numbers RENAME COLUMN n TO N", &result);
    failures += expect_ddl_result(result, "case-only rename result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_column_descriptor(
        database,
        before_table.table_id,
        "N",
        2,
        "INT",
        true,
        "case-only renamed column descriptor"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM numbers",
            .values = show_case_columns_rows,
            .row_count = 3U,
            .column_count = show_columns_column_count,
            .context = "show columns after case-only rename",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE numbers",
            .values = show_case_create_rows,
            .row_count = 1U,
            .column_count = 2U,
            .context = "show create table after case-only rename",
        }
    );
    failures += execute_ok(database, "INSERT INTO numbers VALUES (3, 31, 40)", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 1, "insert after rename rows");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "UPDATE numbers SET N = -5 WHERE id = 3", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 1, "update after rename rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, N, nn FROM numbers ORDER BY id",
            .values = rows_after_dml,
            .row_count = 3U,
            .column_count = 3U,
            .context = "rows after rename dml",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE edge_rename (first_col INT, middle_col BIGINT, last_col INT UNSIGNED)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "INSERT INTO edge_rename VALUES (1, 2, 3), (4, 5, 6)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "edge_rename", &edge_table),
        MYLITE_OK,
        "read edge_rename table"
    );
    failures += execute_ok(
        database,
        "ALTER TABLE edge_rename RENAME COLUMN first_col TO renamed_first",
        &result
    );
    failures += expect_ddl_result(result, "rename first column result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_column_descriptor(
        database,
        edge_table.table_id,
        "renamed_first",
        1,
        "INT",
        true,
        "renamed first column descriptor"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM edge_rename ORDER BY renamed_first",
            .values = edge_rows_after_first_rename,
            .row_count = 2U,
            .column_count = 3U,
            .context = "rows after first column rename",
        }
    );
    failures += execute_ok(
        database,
        "ALTER TABLE edge_rename RENAME COLUMN last_col TO renamed_last",
        &result
    );
    failures += expect_ddl_result(result, "rename last column result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_column_descriptor(
        database,
        edge_table.table_id,
        "renamed_last",
        3,
        "INT UNSIGNED",
        true,
        "renamed last column descriptor"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM edge_rename ORDER BY renamed_first",
            .values = edge_rows_after_last_rename,
            .row_count = 2U,
            .column_count = 3U,
            .context = "rows after last column rename",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE cleanup (id INT NOT NULL, old_col INT, kept INT)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "INSERT INTO cleanup VALUES (1, 10, 10), (2, 20, 20)", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "ALTER TABLE cleanup RENAME COLUMN old_col TO renamed", &result);
    failures += expect_ddl_result(result, "rename cleanup column result");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "DELETE FROM cleanup WHERE renamed = 10", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 1, "delete after rename rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, kept FROM cleanup ORDER BY id",
            .values = cleanup_after_delete,
            .row_count = 1U,
            .column_count = 2U,
            .context = "delete observes rename",
        }
    );
    failures += execute_ok(database, "TRUNCATE TABLE cleanup", &result);
    failures += expect_ddl_result(result, "truncate after rename");
    mylite_result_free(result);
    result = NULL;
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures +=
        expect_bytes(actual_preamble, expected_preamble, sizeof(expected_preamble), "preamble");

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen rename-column file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, N, nn FROM numbers ORDER BY id",
            .values = rows_after_dml,
            .row_count = 3U,
            .column_count = 3U,
            .context = "reopen sees renamed column",
        }
    );
    failures += execute_ok(database, "RENAME TABLE numbers TO renamed_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "ALTER TABLE renamed_numbers RENAME COLUMN N TO final_n", &result);
    failures += expect_ddl_result(result, "rename column after table rename result");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "DROP TABLE renamed_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "ALTER TABLE renamed_numbers RENAME COLUMN final_n TO n",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_numbers' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_rename_column_keyed_dependencies(void) {
    static const char *const keyed_rows_after_rename[] = {
        "1",
        "alpha",
        "10",
        "hello world",
        "2",
        "beta",
        "20",
        "beta words",
    };
    static const char *const primary_statistics[] = {"pk_id", NULL, "BTREE"};
    static const char *const unique_statistics[] = {"key_name", NULL, "BTREE"};
    static const char *const index_statistics[] = {"v", NULL, "BTREE"};
    static const char *const prefix_statistics[] = {"key_name", "10", "BTREE"};
    static const char *const fulltext_statistics[] = {"content", NULL, "FULLTEXT"};
    static const char *const key_column_usage_values[] = {"pid", "parent_pk"};
    static const char *const child_after_delete[] = {"11", "2"};
    static const char *const child_after_update[] = {"11", "20"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor keyed_table = {0};
    int failures = 0;

    if (make_test_path(path, sizeof(path), "keyed") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open keyed rename file");
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE keyed_rename ("
        "id INT NOT NULL, "
        "k VARCHAR(20) NOT NULL, "
        "v INT, "
        "body VARCHAR(100), "
        "PRIMARY KEY (id), "
        "UNIQUE KEY uk_k (k), "
        "KEY idx_v (v), "
        "KEY pref_k (k(10)), "
        "FULLTEXT KEY ft_body (body))",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO keyed_rename VALUES "
        "(1, 'alpha', 10, 'hello world'), (2, 'beta', 20, 'beta words')",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read keyed rename schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "keyed_rename", &keyed_table),
        MYLITE_OK,
        "read keyed rename table"
    );
    failures += expect_physical_index_count(database, 4, "physical indexes before keyed rename");

    failures += execute_ok(database, "ALTER TABLE keyed_rename RENAME COLUMN id TO pk_id", &result);
    failures += expect_ddl_result(result, "rename primary-key column result");
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "ALTER TABLE keyed_rename RENAME COLUMN k TO key_name", &result);
    failures += expect_ddl_result(result, "rename indexed string column result");
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "ALTER TABLE keyed_rename RENAME COLUMN body TO content", &result);
    failures += expect_ddl_result(result, "rename fulltext column result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_physical_index_count(database, 4, "physical indexes after keyed rename");
    failures += expect_physical_index_sql_contains(
        database,
        (struct physical_index_name_glob){"_mylite_user_index_*"},
        (struct physical_index_sql_needle){"\"pk_id\""},
        "physical primary index follows rename"
    );
    failures += expect_physical_index_sql_contains(
        database,
        (struct physical_index_name_glob){"_mylite_user_index_*"},
        (struct physical_index_sql_needle){"\"key_name\""},
        "physical secondary index follows rename"
    );
    failures += expect_physical_index_sql_contains(
        database,
        (struct physical_index_name_glob){"_mylite_user_index_*"},
        (struct physical_index_sql_needle){"substr(\"key_name\", 1, 10)"},
        "physical prefix index follows rename"
    );
    failures += expect_physical_index_sql_contains(
        database,
        (struct physical_index_name_glob){"_mylite_user_index_*"},
        (struct physical_index_sql_needle){"\"v\""},
        "physical nonunique index remains valid"
    );
    failures += expect_column_descriptor(
        database,
        keyed_table.table_id,
        "pk_id",
        1,
        "INT",
        false,
        "renamed primary-key column descriptor"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT pk_id, key_name, v, content FROM keyed_rename ORDER BY pk_id",
            .values = keyed_rows_after_rename,
            .row_count = 2U,
            .column_count = 4U,
            .context = "rows after keyed renames",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, SUB_PART, INDEX_TYPE "
                   "FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'keyed_rename' "
                   "AND INDEX_NAME = 'PRIMARY'",
            .values = primary_statistics,
            .row_count = 1U,
            .column_count = 3U,
            .context = "primary statistics after keyed renames",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, SUB_PART, INDEX_TYPE "
                   "FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'keyed_rename' "
                   "AND INDEX_NAME = 'uk_k'",
            .values = unique_statistics,
            .row_count = 1U,
            .column_count = 3U,
            .context = "unique statistics after keyed renames",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, SUB_PART, INDEX_TYPE "
                   "FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'keyed_rename' "
                   "AND INDEX_NAME = 'idx_v'",
            .values = index_statistics,
            .row_count = 1U,
            .column_count = 3U,
            .context = "secondary statistics after keyed renames",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, SUB_PART, INDEX_TYPE "
                   "FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'keyed_rename' "
                   "AND INDEX_NAME = 'pref_k'",
            .values = prefix_statistics,
            .row_count = 1U,
            .column_count = 3U,
            .context = "prefix statistics after keyed renames",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, SUB_PART, INDEX_TYPE "
                   "FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'keyed_rename' "
                   "AND INDEX_NAME = 'ft_body'",
            .values = fulltext_statistics,
            .row_count = 1U,
            .column_count = 3U,
            .context = "fulltext statistics after keyed renames",
        }
    );
    failures += execute_ok(database, "SHOW CREATE TABLE keyed_rename", &result);
    if (failures == 0) {
        const char *show_create = mylite_result_value_text(result, 0U, 1U);

        failures +=
            expect_contains(show_create, "PRIMARY KEY (`pk_id`)", "show create primary rename");
        failures += expect_contains(
            show_create,
            "UNIQUE KEY `uk_k` (`key_name`)",
            "show create unique rename"
        );
        failures += expect_contains(
            show_create,
            "KEY `pref_k` (`key_name`(10))",
            "show create prefix rename"
        );
        failures += expect_contains(
            show_create,
            "FULLTEXT KEY `ft_body` (`content`)",
            "show create fulltext rename"
        );
    }
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "INSERT INTO keyed_rename VALUES (1, 'gamma', 30, 'duplicate primary')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO keyed_rename VALUES (3, 'alpha', 30, 'duplicate unique')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE parent_rename (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE child_rename ("
        "id INT PRIMARY KEY, "
        "parent_id INT, "
        "KEY p_idx(parent_id), "
        "CONSTRAINT fk_parent FOREIGN KEY (parent_id) REFERENCES parent_rename(id) "
        "ON DELETE CASCADE ON UPDATE CASCADE)"
    );
    failures += expect_dml_ok(database, "INSERT INTO parent_rename VALUES (1), (2)", 2);
    failures += expect_dml_ok(database, "INSERT INTO child_rename VALUES (10, 1), (11, 2)", 2);
    failures +=
        execute_ok(database, "ALTER TABLE parent_rename RENAME COLUMN id TO parent_pk", &result);
    failures += expect_ddl_result(result, "rename parent foreign-key column result");
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "ALTER TABLE child_rename RENAME COLUMN parent_id TO pid", &result);
    failures += expect_ddl_result(result, "rename child foreign-key column result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, REFERENCED_COLUMN_NAME "
                   "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'child_rename' "
                   "AND CONSTRAINT_NAME = 'fk_parent'",
            .values = key_column_usage_values,
            .row_count = 1U,
            .column_count = 2U,
            .context = "foreign-key key-column usage after rename",
        }
    );
    failures += execute_ok(database, "SHOW CREATE TABLE child_rename", &result);
    if (failures == 0) {
        const char *show_create = mylite_result_value_text(result, 0U, 1U);

        failures += expect_contains(
            show_create,
            "KEY `p_idx` (`pid`)",
            "show create foreign-key child index rename"
        );
        failures += expect_contains(
            show_create,
            "CONSTRAINT `fk_parent` FOREIGN KEY (`pid`) REFERENCES `parent_rename` (`parent_pk`) "
            "ON DELETE CASCADE ON UPDATE CASCADE",
            "show create foreign-key rename"
        );
    }
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "INSERT INTO child_rename VALUES (12, 99)",
        (struct expected_sql_error){
            .code = mysql_error_no_referenced_row,
            .sqlstate = "23000",
            .message_part = "child row",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM parent_rename WHERE parent_pk = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, pid FROM child_rename ORDER BY id",
            .values = child_after_delete,
            .row_count = 1U,
            .column_count = 2U,
            .context = "foreign-key cascade delete after rename",
        }
    );
    failures +=
        expect_dml_ok(database, "UPDATE parent_rename SET parent_pk = 20 WHERE parent_pk = 2", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, pid FROM child_rename ORDER BY id",
            .values = child_after_update,
            .row_count = 1U,
            .column_count = 2U,
            .context = "foreign-key cascade update after rename",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen keyed rename file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT pk_id, key_name, v, content FROM keyed_rename ORDER BY pk_id",
            .values = keyed_rows_after_rename,
            .row_count = 2U,
            .column_count = 4U,
            .context = "reopen rows after keyed renames",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, pid FROM child_rename ORDER BY id",
            .values = child_after_update,
            .row_count = 1U,
            .column_count = 2U,
            .context = "reopen foreign-key rows after rename",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_rename_column_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics database");
    failures += execute_error(
        database,
        "ALTER TABLE missing RENAME COLUMN old_col TO new_col",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE numbers (id INT NOT NULL, n INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE case_collision (a INT, B INT)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "ALTER TABLE unknown RENAME COLUMN n TO renamed",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.unknown' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.numbers RENAME COLUMN n TO renamed",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers RENAME COLUMN missing TO renamed",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'numbers'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers RENAME COLUMN n TO id",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_column,
            .sqlstate = "42S21",
            .message_part = "Duplicate column name 'id'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE case_collision RENAME COLUMN a TO b",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_column,
            .sqlstate = "42S21",
            .message_part = "Duplicate column name 'B'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private RENAME COLUMN n TO renamed",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers RENAME COLUMN _mylite_private TO renamed",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_column_name,
            .sqlstate = "42000",
            .message_part = "Incorrect column name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers RENAME COLUMN n TO _mylite_private",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_column_name,
            .sqlstate = "42000",
            .message_part = "Incorrect column name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers RENAME n TO renamed",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers RENAME COLUMN numbers.n TO renamed",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers RENAME COLUMN n TO numbers.renamed",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers RENAME COLUMN n renamed",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers RENAME COLUMN n TO renamed FIRST",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers RENAME COLUMN n TO renamed, RENAME COLUMN id TO old_id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers RENAME COLUMN n TO renamed, ALGORITHM=INSTANT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers RENAME COLUMN n TO renamed, LOCK=DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_rename_column_physical_failure_preserves_catalog(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor before_table = {0};
    struct mylite_catalog_table_descriptor after_table = {0};
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t generation_before_failure = 0U;
    uint64_t sqlite_generation_before_failure = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "physical_failure") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open physical failure file");
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE broken (id INT NOT NULL, old_col INT, kept INT)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO broken VALUES (1, 10, 20)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read failure schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "broken", &before_table),
        MYLITE_OK,
        "read failure table before"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += rename_physical_column(
            sqlite,
            before_table.physical_name,
            "old_col",
            "already_renamed"
        );
    }
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        generation_before_failure = catalog->generation;
    }
    if (session != NULL) {
        sqlite_generation_before_failure = session->sqlite_schema_generation;
    }

    failures += execute_error(
        database,
        "ALTER TABLE broken RENAME COLUMN old_col TO renamed",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite schema operation failed",
        }
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            generation_before_failure,
            "physical failure leaves catalog generation unchanged"
        );
    }
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before_failure,
            "physical failure leaves SQLite schema generation unchanged"
        );
    }
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "broken", &after_table),
        MYLITE_OK,
        "read failure table after"
    );
    failures += expect_uint64(
        after_table.descriptor_version,
        before_table.descriptor_version,
        "physical failure leaves table descriptor version unchanged"
    );
    failures += expect_column_descriptor(
        database,
        before_table.table_id,
        "old_col",
        2,
        "INT",
        true,
        "physical failure keeps old descriptor"
    );
    failures += expect_missing_column_descriptor(
        database,
        before_table.table_id,
        "renamed",
        "physical failure does not expose new descriptor"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_rename_column_independent_handles(void) {
    static const char *const first_rows[] = {"1", "10"};
    static const char *const second_rows[] = {"2", "200", "20"};
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += execute_ok(first, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(first, "CREATE TABLE app.t (id INT NOT NULL, old_col INT, kept INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(second, "CREATE TABLE app.t (id INT NOT NULL, old_col INT, kept INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "INSERT INTO app.t VALUES (1, 100, 10)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "INSERT INTO app.t VALUES (2, 200, 20)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "ALTER TABLE app.t RENAME COLUMN old_col TO renamed", &result);
    failures += expect_ddl_result(result, "first independent rename");
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, kept FROM app.t WHERE renamed = 100",
            .values = first_rows,
            .row_count = 1U,
            .column_count = 2U,
            .context = "first handle renamed column",
        }
    );
    failures += execute_error(
        first,
        "SELECT id, old_col FROM app.t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'old_col'",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, old_col, kept FROM app.t ORDER BY id",
            .values = second_rows,
            .row_count = 1U,
            .column_count = 3U,
            .context = "second handle keeps old column",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
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

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result != NULL) {
        failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
    }
    mylite_result_free(result);
    return failures;
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
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    message = mylite_errmsg(database);
    if (expected.message_part != NULL) {
        failures += expect_contains(message, expected.message_part, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_ddl_result(const mylite_result *result, const char *context) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), 0U, context);
    failures += expect_size(mylite_result_row_count(result), 0U, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
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
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s: expected NULL at %zu,%zu, got '%s'\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
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

static int expect_missing_column_descriptor(
    mylite_db *database,
    int64_t table_id,
    const char *name,
    const char *context
) {
    struct mylite_catalog_column_descriptor column = {0};

    return expect_int(
        mylite_catalog_read_column_by_name(database, table_id, name, &column),
        MYLITE_ERROR,
        context
    );
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

static int expect_physical_index_sql_contains(
    mylite_db *database,
    struct physical_index_name_glob name_glob,
    struct physical_index_sql_needle needle,
    const char *context
) {
    enum { sqlite_use_nul_terminated_string = -1 };

    sqlite3 *connection = mylite_connection_sqlite_for_test(database);
    sqlite3_stmt *statement = NULL;
    bool found = false;
    int rc = SQLITE_OK;

    if (connection == NULL) {
        fprintf(stderr, "%s: missing SQLite test connection\n", context);
        return 1;
    }

    rc = sqlite3_prepare_v2(
        connection,
        "SELECT sql FROM sqlite_schema WHERE type = 'index' AND name GLOB ?1",
        sqlite_use_nul_terminated_string,
        &statement,
        NULL
    );
    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_text(
            statement,
            1,
            name_glob.value,
            sqlite_use_nul_terminated_string,
            NULL
        );
    }
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s: prepare physical index SQL query failed: %d\n", context, rc);
        sqlite3_finalize(statement);
        return 1;
    }

    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        const unsigned char *sql = sqlite3_column_text(statement, 0);

        if (sql != NULL && strstr((const char *)sql, needle.value) != NULL) {
            found = true;
            rc = SQLITE_OK;
            break;
        }
    }
    if (rc == SQLITE_DONE) {
        rc = SQLITE_OK;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK && rc == SQLITE_OK) {
        rc = SQLITE_ERROR;
    }
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s: physical index SQL query failed: %d\n", context, rc);
        return 1;
    }
    if (!found) {
        fprintf(
            stderr,
            "%s: expected physical index SQL matching '%s' to contain '%s'\n",
            context,
            name_glob.value,
            needle.value
        );
        return 1;
    }
    return 0;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "test-runtime-alter-table-rename-column-%s-%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path for %s\n", name);
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
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(file);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    if (read_count != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
        return 1;
    }
    return 0;
}

static int execute_sql(sqlite3 *connection, const char *sql) {
    int rc = sqlite3_exec(connection, sql, NULL, NULL, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQLite exec failed for %s: %d\n", sql, rc);
        return 1;
    }
    return 0;
}

static int rename_physical_column(
    sqlite3 *connection,
    const char *physical_name,
    const char *old_name,
    const char *new_name
) {
    char sql[sql_capacity];
    int written = snprintf(
        sql,
        sizeof(sql),
        "ALTER TABLE \"%s\" RENAME COLUMN \"%s\" TO \"%s\"",
        physical_name,
        old_name,
        new_name
    );

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "failed to build physical rename column SQL\n");
        return 1;
    }
    return execute_sql(connection, sql);
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
