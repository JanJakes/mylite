#include "mylite_test_support.h"

#include <mylite/mylite.h>

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
    redundant_indexes_column_count = 10,
    flattened_keys_column_count = 6,
    show_columns_column_count = 6,
    information_schema_columns_sample_column_count = 7,
    information_schema_tables_sample_column_count = 7,
    information_schema_views_column_count = 9,
    information_schema_view_table_usage_column_count = 4,
    show_create_view_column_count = 4,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

struct expected_query_contains {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    size_t row_count;
    size_t row_index;
    size_t column_index;
    const char *needle;
    const char *context;
};

static int test_sys_schema_redundant_indexes_views(void);
static int seed_schema_objects(mylite_db *database);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_query_contains(mylite_db *database, struct expected_query_contains expected);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static const char *const redundant_indexes_columns[redundant_indexes_column_count] = {
    "table_schema",
    "table_name",
    "redundant_index_name",
    "redundant_index_columns",
    "redundant_index_non_unique",
    "dominant_index_name",
    "dominant_index_columns",
    "dominant_index_non_unique",
    "subpart_exists",
    "sql_drop_index",
};

static const char *const flattened_keys_columns[flattened_keys_column_count] = {
    "table_schema",
    "table_name",
    "index_name",
    "non_unique",
    "subpart_exists",
    "index_columns",
};

static const char *const selected_flattened_keys_columns[] = {
    "table_schema",
    "table_name",
    "index_name",
    "index_columns",
};

static const char *const selected_redundant_indexes_columns[] = {
    "table_schema",
    "table_name",
    "redundant_index_name",
    "dominant_index_name",
};

static const char *const show_columns_columns[show_columns_column_count] = {
    "Field",
    "Type",
    "Null",
    "Key",
    "Default",
    "Extra",
};

static const char *const
    information_schema_columns_sample_columns[information_schema_columns_sample_column_count] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "COLUMN_DEFAULT",
        "IS_NULLABLE",
        "COLUMN_TYPE",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
};

static const char *const
    information_schema_tables_sample_columns[information_schema_tables_sample_column_count] = {
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "TABLE_ROWS",
        "DATA_LENGTH",
        "TABLE_COMMENT",
};

static const char *const information_schema_views_columns[information_schema_views_column_count] = {
    "TABLE_CATALOG",
    "TABLE_SCHEMA",
    "TABLE_NAME",
    "CHECK_OPTION",
    "IS_UPDATABLE",
    "DEFINER",
    "SECURITY_TYPE",
    "CHARACTER_SET_CLIENT",
    "COLLATION_CONNECTION",
};

static const char *const
    information_schema_view_table_usage_columns[information_schema_view_table_usage_column_count] =
        {
            "VIEW_SCHEMA",
            "VIEW_NAME",
            "TABLE_SCHEMA",
            "TABLE_NAME",
};

static const char *const show_create_view_columns[show_create_view_column_count] = {
    "View",
    "Create View",
    "character_set_client",
    "collation_connection",
};

int main(void) {
    return test_sys_schema_redundant_indexes_views() == 0 ? 0 : 1;
}

static int test_sys_schema_redundant_indexes_views(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const count_three[] = {"3"};
    static const char *const count_seven[] = {"7"};
    static const char *const flattened_prefix_values[] = {
        "app",
        "base_one",
        "idx_c_prefix",
        "1",
        "1",
        "c",
    };
    static const char *const redundant_idx_b_values[] = {
        "app",
        "base_one",
        "idx_b",
        "b",
        "1",
        "uq_b",
        "b",
        "0",
        "0",
        "ALTER TABLE `app`.`base_one` DROP INDEX `idx_b`",
    };
    static const char *const selected_flattened_values[] = {
        "app",
        "base_one",
        "idx_ab",
        "a,b",
    };
    static const char *const selected_redundant_values[] = {
        "app",
        "base_one",
        "idx_b",
        "uq_b",
    };
    static const char *const redundant_show_columns_values[] = {
        "table_schema",
        "varchar(64)",
        "NO",
        "",
        NULL,
        "",
        "table_name",
        "varchar(64)",
        "NO",
        "",
        NULL,
        "",
        "redundant_index_name",
        "varchar(64)",
        "YES",
        "",
        NULL,
        "",
        "redundant_index_columns",
        "text",
        "YES",
        "",
        NULL,
        "",
        "redundant_index_non_unique",
        "int",
        "YES",
        "",
        NULL,
        "",
        "dominant_index_name",
        "varchar(64)",
        "YES",
        "",
        NULL,
        "",
        "dominant_index_columns",
        "text",
        "YES",
        "",
        NULL,
        "",
        "dominant_index_non_unique",
        "int",
        "YES",
        "",
        NULL,
        "",
        "subpart_exists",
        "int",
        "NO",
        "",
        "0",
        "",
        "sql_drop_index",
        "varchar(223)",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const flattened_show_columns_values[] = {
        "table_schema",   "varchar(64)", "NO",  "", NULL, "",
        "table_name",     "varchar(64)", "NO",  "", NULL, "",
        "index_name",     "varchar(64)", "YES", "", NULL, "",
        "non_unique",     "int",         "YES", "", NULL, "",
        "subpart_exists", "bigint",      "YES", "", NULL, "",
        "index_columns",  "text",        "YES", "", NULL, "",
    };
    static const char *const redundant_information_schema_columns_sample_values[] = {
        "schema_redundant_indexes",
        "table_schema",
        NULL,
        "NO",
        "varchar(64)",
        "utf8mb3",
        "utf8mb3_bin",
        "schema_redundant_indexes",
        "subpart_exists",
        "0",
        "NO",
        "int",
        NULL,
        NULL,
        "schema_redundant_indexes",
        "sql_drop_index",
        NULL,
        "YES",
        "varchar(223)",
        "utf8mb3",
        "utf8mb3_tolower_ci",
    };
    static const char *const flattened_information_schema_columns_sample_values[] = {
        "x$schema_flattened_keys",
        "table_schema",
        NULL,
        "NO",
        "varchar(64)",
        "utf8mb3",
        "utf8mb3_bin",
        "x$schema_flattened_keys",
        "subpart_exists",
        NULL,
        "YES",
        "bigint",
        NULL,
        NULL,
        "x$schema_flattened_keys",
        "index_columns",
        NULL,
        "YES",
        "text",
        "utf8mb3",
        "utf8mb3_tolower_ci",
    };
    static const char *const information_schema_tables_sample_values[] = {
        "sys",
        "schema_redundant_indexes",
        "VIEW",
        NULL,
        NULL,
        NULL,
        "VIEW",
        "sys",
        "x$schema_flattened_keys",
        "VIEW",
        NULL,
        NULL,
        NULL,
        "VIEW",
    };
    static const char *const redundant_views_values[] = {
        "def",
        "sys",
        "schema_redundant_indexes",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const flattened_views_values[] = {
        "def",
        "sys",
        "x$schema_flattened_keys",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const redundant_usage_values[] = {
        "sys",
        "schema_redundant_indexes",
        "sys",
        "x$schema_flattened_keys",
    };
    static const char *const flattened_usage_values[] = {
        "sys",
        "x$schema_flattened_keys",
        "information_schema",
        "STATISTICS",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "sys-schema-redundant-indexes-views") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += seed_schema_objects(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.`x$schema_flattened_keys` "
                   "WHERE table_schema = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_seven,
            .row_count = 1U,
            .context = "sys.x$schema_flattened_keys app row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.schema_redundant_indexes "
                   "WHERE table_schema = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_three,
            .row_count = 1U,
            .context = "sys.schema_redundant_indexes app row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT table_schema, table_name, index_name, non_unique, subpart_exists, "
                   "index_columns FROM sys.`x$schema_flattened_keys` "
                   "WHERE table_schema = 'app' AND index_name = 'idx_c_prefix'",
            .column_names = flattened_keys_columns,
            .column_count = flattened_keys_column_count,
            .values = flattened_prefix_values,
            .row_count = 1U,
            .context = "sys.x$schema_flattened_keys prefix row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT table_schema, table_name, redundant_index_name, "
                   "redundant_index_columns, redundant_index_non_unique, "
                   "dominant_index_name, dominant_index_columns, "
                   "dominant_index_non_unique, subpart_exists, sql_drop_index "
                   "FROM sys.schema_redundant_indexes "
                   "WHERE table_schema = 'app' AND redundant_index_name = 'idx_b'",
            .column_names = redundant_indexes_columns,
            .column_count = redundant_indexes_column_count,
            .values = redundant_idx_b_values,
            .row_count = 1U,
            .context = "sys.schema_redundant_indexes unique-dominant row",
        }
    );
    failures += expect_statement_ok(database, "USE sys");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT table_schema, table_name, index_name, index_columns "
                   "FROM `x$schema_flattened_keys` "
                   "WHERE table_schema = 'app' AND index_name = 'idx_ab'",
            .column_names = selected_flattened_keys_columns,
            .column_count = 4U,
            .values = selected_flattened_values,
            .row_count = 1U,
            .context = "sys.x$schema_flattened_keys selected schema read",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT table_schema, table_name, redundant_index_name, dominant_index_name "
                   "FROM schema_redundant_indexes "
                   "WHERE table_schema = 'app' AND redundant_index_name = 'idx_b'",
            .column_names = selected_redundant_indexes_columns,
            .column_count = 4U,
            .values = selected_redundant_values,
            .row_count = 1U,
            .context = "sys.schema_redundant_indexes selected schema read",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM schema_redundant_indexes",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = redundant_show_columns_values,
            .row_count = sizeof(redundant_show_columns_values) /
                         sizeof(redundant_show_columns_values[0]) / show_columns_column_count,
            .context = "sys.schema_redundant_indexes show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM `x$schema_flattened_keys`",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = flattened_show_columns_values,
            .row_count = sizeof(flattened_show_columns_values) /
                         sizeof(flattened_show_columns_values[0]) / show_columns_column_count,
            .context = "sys.x$schema_flattened_keys show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, COLUMN_DEFAULT, IS_NULLABLE, COLUMN_TYPE, "
                   "CHARACTER_SET_NAME, COLLATION_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_redundant_indexes' "
                   "AND COLUMN_NAME IN ('table_schema', 'subpart_exists', 'sql_drop_index') "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_sample_columns,
            .column_count = information_schema_columns_sample_column_count,
            .values = redundant_information_schema_columns_sample_values,
            .row_count = sizeof(redundant_information_schema_columns_sample_values) /
                         sizeof(redundant_information_schema_columns_sample_values[0]) /
                         information_schema_columns_sample_column_count,
            .context = "sys.schema_redundant_indexes information_schema.columns sample",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, COLUMN_DEFAULT, IS_NULLABLE, COLUMN_TYPE, "
                   "CHARACTER_SET_NAME, COLLATION_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x$schema_flattened_keys' "
                   "AND COLUMN_NAME IN ('table_schema', 'subpart_exists', 'index_columns') "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_sample_columns,
            .column_count = information_schema_columns_sample_column_count,
            .values = flattened_information_schema_columns_sample_values,
            .row_count = sizeof(flattened_information_schema_columns_sample_values) /
                         sizeof(flattened_information_schema_columns_sample_values[0]) /
                         information_schema_columns_sample_column_count,
            .context = "sys.x$schema_flattened_keys information_schema.columns sample",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, "
                   "IS_UPDATABLE, DEFINER, SECURITY_TYPE, CHARACTER_SET_CLIENT, "
                   "COLLATION_CONNECTION FROM INFORMATION_SCHEMA.VIEWS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_redundant_indexes'",
            .column_names = information_schema_views_columns,
            .column_count = information_schema_views_column_count,
            .values = redundant_views_values,
            .row_count = 1U,
            .context = "sys.schema_redundant_indexes information_schema.views",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, "
                   "IS_UPDATABLE, DEFINER, SECURITY_TYPE, CHARACTER_SET_CLIENT, "
                   "COLLATION_CONNECTION FROM INFORMATION_SCHEMA.VIEWS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x$schema_flattened_keys'",
            .column_names = information_schema_views_columns,
            .column_count = information_schema_views_column_count,
            .values = flattened_views_values,
            .row_count = 1U,
            .context = "sys.x$schema_flattened_keys information_schema.views",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE "
                   "WHERE VIEW_SCHEMA = 'sys' AND VIEW_NAME = 'schema_redundant_indexes'",
            .column_names = information_schema_view_table_usage_columns,
            .column_count = information_schema_view_table_usage_column_count,
            .values = redundant_usage_values,
            .row_count = 1U,
            .context = "sys.schema_redundant_indexes information_schema.view_table_usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE "
                   "WHERE VIEW_SCHEMA = 'sys' AND VIEW_NAME = 'x$schema_flattened_keys'",
            .column_names = information_schema_view_table_usage_columns,
            .column_count = information_schema_view_table_usage_column_count,
            .values = flattened_usage_values,
            .row_count = 1U,
            .context = "sys.x$schema_flattened_keys information_schema.view_table_usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('schema_redundant_indexes', 'x$schema_flattened_keys')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys redundant indexes statistics count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('schema_redundant_indexes', 'x$schema_flattened_keys')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys redundant indexes table_constraints count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('schema_redundant_indexes', 'x$schema_flattened_keys')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys redundant indexes key_column_usage count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "
                   "WHERE CONSTRAINT_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('schema_redundant_indexes', 'x$schema_flattened_keys')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys redundant indexes table_constraints_extensions count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE "
                   "WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('schema_redundant_indexes', 'x$schema_flattened_keys')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "sys redundant indexes view_routine_usage count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, "
                   "DATA_LENGTH, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('schema_redundant_indexes', 'x$schema_flattened_keys') "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_sample_columns,
            .column_count = information_schema_tables_sample_column_count,
            .values = information_schema_tables_sample_values,
            .row_count = sizeof(information_schema_tables_sample_values) /
                         sizeof(information_schema_tables_sample_values[0]) /
                         information_schema_tables_sample_column_count,
            .context = "sys redundant indexes information_schema.tables sample",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW schema_redundant_indexes",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY "
                      "INVOKER VIEW `schema_redundant_indexes`",
            .context = "sys.schema_redundant_indexes show create view",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE TABLE `x$schema_flattened_keys`",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "VIEW `x$schema_flattened_keys`",
            .context = "sys.x$schema_flattened_keys show create table",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int seed_schema_objects(mylite_db *database) {
    return expect_statement_ok(
        database,
        "CREATE TABLE base_one (id INT NOT NULL, a INT, b INT, c VARCHAR(20), body TEXT, "
        "PRIMARY KEY (id), KEY idx_a (a), KEY idx_ab (a,b), UNIQUE KEY uq_b (b), "
        "KEY idx_b (b), KEY idx_c_prefix (c(5)), KEY idx_c_full (c), "
        "FULLTEXT KEY ft_body (body))"
    );
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    mylite_result_free(result);
    return 0;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK, got %d / %d %s %s\n",
            expected.context,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (result == NULL) {
        fprintf(stderr, "%s: expected result object\n", expected.context);
        return 1;
    }

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    for (size_t column_index = 0U;
         expected.column_names != NULL && column_index < expected.column_count;
         ++column_index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);

    if (expected.values != NULL) {
        for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
            for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
                const char *expected_value =
                    expected.values[(row_index * expected.column_count) + column_index];
                const char *actual_value =
                    mylite_result_value_text(result, row_index, column_index);

                failures +=
                    mylite_test_expect_text_or_null(actual_value, expected_value, expected.context);
            }
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_query_contains(mylite_db *database, struct expected_query_contains expected) {
    mylite_result *result = NULL;
    const char *actual = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK, got %d / %d %s %s\n",
            expected.context,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (result == NULL) {
        fprintf(stderr, "%s: expected result object\n", expected.context);
        return 1;
    }

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    for (size_t column_index = 0U;
         expected.column_names != NULL && column_index < expected.column_count;
         ++column_index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);

    if (failures == 0) {
        actual = mylite_result_value_text(result, expected.row_index, expected.column_index);
        if (actual == NULL || strstr(actual, expected.needle) == NULL) {
            fprintf(
                stderr,
                "%s: expected value to contain [%s], got [%s]\n",
                expected.context,
                expected.needle,
                actual == NULL ? "<NULL>" : actual
            );
            ++failures;
        }
    }

    mylite_result_free(result);
    return failures;
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
