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
    mysql_error_unknown_column = 1054,
    mysql_error_unknown_table_in_schema = 1109,
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

static int test_information_schema_core_queries(void);
static int test_information_schema_wordpress_bridge_queries(void);
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
    int failures = 0;

    failures += test_information_schema_core_queries();
    failures += test_information_schema_wordpress_bridge_queries();
    return failures == 0 ? 0 : 1;
}

static int test_information_schema_core_queries(void) {
    static const char *const schemata_columns[] = {
        "CATALOG_NAME",
        "SCHEMA_NAME",
        "DEFAULT_CHARACTER_SET_NAME",
        "DEFAULT_COLLATION_NAME",
        "SQL_PATH",
        "DEFAULT_ENCRYPTION",
    };
    static const char *const schemata_values[] = {
        "def",
        "app",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        NULL,
        "NO",
    };
    static const char *const builtin_schemata_values[] = {
        "information_schema",
        "utf8mb3",
        "utf8mb3_general_ci",
        NULL,
        "NO",
        "mysql",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        NULL,
        "NO",
        "performance_schema",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        NULL,
        "NO",
        "sys",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        NULL,
        "NO",
    };
    static const char *const table_columns[] = {
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "VERSION",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "DATA_LENGTH",
        "AUTO_INCREMENT",
    };
    static const char *const table_values[] = {
        "app", "other",    "BASE TABLE", "InnoDB", "10", "Dynamic", "1", "16384", NULL,
        "app", "t",        "BASE TABLE", "InnoDB", "10", "Dynamic", "1", "16384", "2",
        "app", "wp_users", "BASE TABLE", "InnoDB", "10", "Dynamic", "0", "16384", "1",
    };
    static const char *const table_computed_columns[] = {"name", "engine", "data"};
    static const char *const table_computed_values[] = {"t", "InnoDB", "0"};
    static const char *const indexed_column_join_columns[] = {
        "DATA_TYPE",
        "INDEX_NAME",
        "COLUMN_NAME",
    };
    static const char *const indexed_column_join_values[] = {
        "bigint",
        "PRIMARY",
        "ID",
        "varchar",
        "user_email",
        "user_email",
        "varchar",
        "user_login_key",
        "user_login",
        "varchar",
        "user_nicename",
        "user_nicename",
    };
    static const char *const with_union_column[] = {"name"};
    static const char *const with_union_values[] = {
        "display_name (column)",
        "ID (column)",
        "PRIMARY (index)",
        "user_activation_key (column)",
        "user_email (column)",
        "user_email (index)",
        "user_login (column)",
        "user_login_key (index)",
        "user_nicename (column)",
        "user_nicename (index)",
        "user_pass (column)",
        "user_registered (column)",
        "user_status (column)",
        "user_url (column)",
    };
    static const char *const column_columns[] = {
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "COLUMN_DEFAULT",
        "IS_NULLABLE",
        "DATA_TYPE",
        "CHARACTER_MAXIMUM_LENGTH",
        "CHARACTER_OCTET_LENGTH",
        "NUMERIC_PRECISION",
        "NUMERIC_SCALE",
        "COLUMN_TYPE",
        "COLUMN_KEY",
        "EXTRA",
    };
    static const char *const column_values[] = {
        "id",      "1",
        NULL,      "NO",
        "int",     NULL,
        NULL,      "10",
        "0",       "int",
        "PRI",     "auto_increment",
        "v",       "2",
        NULL,      "YES",
        "varchar", "3",
        "12",      NULL,
        NULL,      "varchar(3)",
        "",        "",
        "n",       "3",
        "7",       "NO",
        "int",     NULL,
        NULL,      "10",
        "0",       "int",
        "",        "",
        "u",       "4",
        NULL,      "YES",
        "tinyint", NULL,
        NULL,      "3",
        "0",       "tinyint unsigned",
        "",        "",
        "hidden",  "5",
        NULL,      "YES",
        "int",     NULL,
        NULL,      "10",
        "0",       "int",
        "",        "INVISIBLE",
    };
    static const char *const single_column[] = {"COLUMN_NAME"};
    static const char *const id_value[] = {"id"};
    static const char *const alias_limit_values[] = {"id", "v", "n"};
    static const char *const desc_limit_values[] = {"hidden", "u"};
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_one[] = {"1"};
    static const char *const count_zero[] = {"0"};
    static const char *const builtin_schemata_columns[] = {
        "SCHEMA_NAME",
        "DEFAULT_CHARACTER_SET_NAME",
        "DEFAULT_COLLATION_NAME",
        "SQL_PATH",
        "DEFAULT_ENCRYPTION",
    };
    static const char *const schema_name_column[] = {"SCHEMA_NAME"};
    static const char *const collation_name_column[] = {"COLLATION_NAME"};
    static const char *const builtin_schemata_default_order_values[] = {
        "mysql",
        "information_schema",
        "performance_schema",
        "sys",
    };
    static const char *const mysql_collation_name_order_values[] = {
        "ucs2_romanian_ci",
        "ucs2_roman_ci",
    };
    static const char *const system_table_columns[] = {
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "VERSION",
        "ROW_FORMAT",
        "TABLE_ROWS"
    };
    static const char *const system_table_values[] = {
        "information_schema", "COLUMNS",  "SYSTEM VIEW", NULL, "10", NULL, "0",
        "information_schema", "SCHEMATA", "SYSTEM VIEW", NULL, "10", NULL, "0",
        "information_schema", "TABLES",   "SYSTEM VIEW", NULL, "10", NULL, "0",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "core") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open information schema db");
    failures += seed_database(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME = 'app'",
            .column_names = schemata_columns,
            .column_count = sizeof(schemata_columns) / sizeof(schemata_columns[0]),
            .values = schemata_values,
            .row_count = 1U,
            .context = "schemata wildcard app row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SCHEMA_NAME, DEFAULT_CHARACTER_SET_NAME, DEFAULT_COLLATION_NAME, "
                   "SQL_PATH, DEFAULT_ENCRYPTION FROM INFORMATION_SCHEMA.SCHEMATA "
                   "WHERE SCHEMA_NAME IN ('information_schema', 'mysql', "
                   "'performance_schema', 'sys') ORDER BY SCHEMA_NAME",
            .column_names = builtin_schemata_columns,
            .column_count = sizeof(builtin_schemata_columns) / sizeof(builtin_schemata_columns[0]),
            .values = builtin_schemata_values,
            .row_count = sizeof(builtin_schemata_values) / sizeof(builtin_schemata_values[0]) /
                         (sizeof(builtin_schemata_columns) / sizeof(builtin_schemata_columns[0])),
            .context = "schemata built-in rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA "
                   "WHERE SCHEMA_NAME IN ('information_schema', 'mysql', "
                   "'performance_schema', 'sys')",
            .column_names = schema_name_column,
            .column_count = sizeof(schema_name_column) / sizeof(schema_name_column[0]),
            .values = builtin_schemata_default_order_values,
            .row_count = sizeof(builtin_schemata_default_order_values) /
                         sizeof(builtin_schemata_default_order_values[0]),
            .context = "schemata built-in default order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLLATION_NAME FROM INFORMATION_SCHEMA.COLLATIONS "
                   "WHERE COLLATION_NAME IN ('ucs2_roman_ci', 'ucs2_romanian_ci') "
                   "ORDER BY COLLATION_NAME",
            .column_names = collation_name_column,
            .column_count = sizeof(collation_name_column) / sizeof(collation_name_column[0]),
            .values = mysql_collation_name_order_values,
            .row_count = sizeof(mysql_collation_name_order_values) /
                         sizeof(mysql_collation_name_order_values[0]),
            .context = "collations MySQL catalog name order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' ORDER BY TABLE_NAME",
            .column_names = table_columns,
            .column_count = sizeof(table_columns) / sizeof(table_columns[0]),
            .values = table_values,
            .row_count = 3U,
            .context = "tables base descriptor rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME AS name, ENGINE AS engine, "
                   "CAST(DATA_LENGTH / 1024 / 1024 AS UNSIGNED) AS data "
                   "FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = 't' ORDER BY name ASC",
            .column_names = table_computed_columns,
            .column_count = sizeof(table_computed_columns) / sizeof(table_computed_columns[0]),
            .values = table_computed_values,
            .row_count = 1U,
            .context = "tables computed data length projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT cols.DATA_TYPE, stats.INDEX_NAME, stats.COLUMN_NAME "
                   "FROM INFORMATION_SCHEMA.COLUMNS AS cols "
                   "JOIN INFORMATION_SCHEMA.STATISTICS AS stats "
                   "ON cols.TABLE_SCHEMA = stats.TABLE_SCHEMA "
                   "AND cols.TABLE_NAME = stats.TABLE_NAME "
                   "AND cols.COLUMN_NAME = stats.COLUMN_NAME "
                   "WHERE cols.TABLE_SCHEMA = 'app' AND cols.TABLE_NAME = 'wp_users' "
                   "ORDER BY INDEX_NAME ASC",
            .column_names = indexed_column_join_columns,
            .column_count =
                sizeof(indexed_column_join_columns) / sizeof(indexed_column_join_columns[0]),
            .values = indexed_column_join_values,
            .row_count =
                sizeof(indexed_column_join_values) / sizeof(indexed_column_join_values[0]) /
                (sizeof(indexed_column_join_columns) / sizeof(indexed_column_join_columns[0])),
            .context = "columns statistics join bridge rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "WITH cols AS ("
                   "SELECT COLUMN_NAME AS column_name FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'wp_users'), "
                   "indexes AS ("
                   "SELECT DISTINCT INDEX_NAME AS index_name FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'wp_users') "
                   "SELECT CONCAT(column_name, ' (column)') AS name FROM cols "
                   "UNION ALL "
                   "SELECT CONCAT(index_name, ' (index)') AS name FROM indexes "
                   "ORDER BY name",
            .column_names = with_union_column,
            .column_count = 1U,
            .values = with_union_values,
            .row_count = sizeof(with_union_values) / sizeof(with_union_values[0]),
            .context = "information schema WITH union bridge rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, "
                   "DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "
                   "NUMERIC_PRECISION, NUMERIC_SCALE, COLUMN_TYPE, COLUMN_KEY, EXTRA "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 't' ORDER BY ORDINAL_POSITION",
            .column_names = column_columns,
            .column_count = sizeof(column_columns) / sizeof(column_columns[0]),
            .values = column_values,
            .row_count = sizeof(column_values) / sizeof(column_values[0]) /
                         (sizeof(column_columns) / sizeof(column_columns[0])),
            .context = "columns descriptor rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT c.COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS AS c "
                   "WHERE c.TABLE_SCHEMA = 'app' AND c.TABLE_NAME = 't' "
                   "ORDER BY c.ORDINAL_POSITION LIMIT 3",
            .column_names = single_column,
            .column_count = 1U,
            .values = alias_limit_values,
            .row_count = 3U,
            .context = "alias qualified ordered limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND COLUMN_NAME = 'ID'",
            .column_names = single_column,
            .column_count = 1U,
            .values = id_value,
            .row_count = 1U,
            .context = "metadata string predicate collation",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND ORDINAL_POSITION = '01'",
            .column_names = single_column,
            .column_count = 1U,
            .values = id_value,
            .row_count = 1U,
            .context = "numeric metadata string coercion",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' "
                   "ORDER BY ORDINAL_POSITION DESC LIMIT 2",
            .column_names = single_column,
            .column_count = 1U,
            .values = desc_limit_values,
            .row_count = 2U,
            .context = "descending ordered limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 't'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_one,
            .row_count = 1U,
            .context = "count star database predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND (TABLE_NAME = 'COLUMNS' OR TABLE_NAME = 'SCHEMATA' "
                   "OR TABLE_NAME = 'TABLES') ORDER BY TABLE_NAME",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 3U,
            .context = "system view table rows",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT TABLES.TABLE_NAME FROM INFORMATION_SCHEMA.TABLES AS t",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'TABLES.TABLE_NAME' in 'field list'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.NOPE",
            .code = mysql_error_unknown_table_in_schema,
            .sqlstate = "42S02",
            .message_part = "Unknown table 'NOPE' in information_schema",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT nope FROM INFORMATION_SCHEMA.TABLES",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'field list'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE nope = 'x'",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'where clause'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES ORDER BY nope",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'order clause'",
        }
    );
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen information schema db");
    failures += expect_statement_ok(database, "USE app", -1);
    failures += expect_statement_ok(database, "RENAME TABLE t TO renamed", -1);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'renamed'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_one,
            .row_count = 1U,
            .context = "renamed table metadata row",
        }
    );
    failures += expect_statement_ok(database, "DROP TABLE renamed", -1);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'renamed'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "dropped table metadata rows",
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
        "CREATE TABLE t (id INT AUTO_INCREMENT PRIMARY KEY, v VARCHAR(3), "
        "n INT NOT NULL DEFAULT 7, u TINYINT UNSIGNED, hidden INT)",
        -1
    );
    failures +=
        expect_statement_ok(database, "ALTER TABLE t ALTER COLUMN hidden SET INVISIBLE", -1);
    failures += expect_statement_ok(database, "CREATE TABLE other (x BIGINT UNSIGNED)", -1);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE wp_users ("
        "ID BIGINT UNSIGNED NOT NULL AUTO_INCREMENT, "
        "user_login VARCHAR(60) NOT NULL DEFAULT '', "
        "user_pass VARCHAR(255) NOT NULL DEFAULT '', "
        "user_nicename VARCHAR(50) NOT NULL DEFAULT '', "
        "user_email VARCHAR(100) NOT NULL DEFAULT '', "
        "user_url VARCHAR(100) NOT NULL DEFAULT '', "
        "user_registered DATETIME NOT NULL, "
        "user_activation_key VARCHAR(255) NOT NULL DEFAULT '', "
        "user_status INT NOT NULL DEFAULT '0', "
        "display_name VARCHAR(250) NOT NULL DEFAULT '', "
        "PRIMARY KEY (ID), "
        "KEY user_login_key (user_login), "
        "KEY user_nicename (user_nicename), "
        "KEY user_email (user_email))",
        -1
    );
    failures +=
        expect_statement_ok(database, "INSERT INTO t (v, u, hidden) VALUES ('abc', 2, 9)", 1);
    failures += expect_statement_ok(database, "INSERT INTO other VALUES (42)", 1);

    return failures;
}

static int test_information_schema_wordpress_bridge_queries(void) {
    static const char *const dynamic_columns[] = {
        "id",
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "COLUMN_NAME",
    };
    static const char *const dynamic_values[] = {
        "2",
        "app",
        "t",
        "id",
        "2",
        "app",
        "t",
        "db_name",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "wordpress_bridges") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open wordpress bridge db");
    failures += expect_statement_ok(database, "CREATE DATABASE app", -1);
    failures += expect_statement_ok(database, "USE app", -1);
    failures += expect_statement_ok(database, "CREATE TABLE t (id INT, db_name TEXT)", -1);
    failures += expect_statement_ok(
        database,
        "INSERT INTO t (id, db_name) VALUES (1, 'other'), (2, 'app')",
        2
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT sub.id, sub.table_schema, sub.table_name, sub.column_name "
                   "FROM ("
                   "SELECT * FROM information_schema.columns c "
                   "JOIN t ON t.db_name = CONCAT(COALESCE(c.table_schema, 'default'), '') "
                   "JOIN information_schema.schemata s ON s.schema_name = c.table_schema "
                   "WHERE c.table_name = 't'"
                   ") sub "
                   "ORDER BY ordinal_position",
            .column_names = dynamic_columns,
            .column_count = sizeof(dynamic_columns) / sizeof(dynamic_columns[0]),
            .values = dynamic_values,
            .row_count = sizeof(dynamic_values) / sizeof(dynamic_values[0]) /
                         (sizeof(dynamic_columns) / sizeof(dynamic_columns[0])),
            .context = "wordpress dynamic information schema columns bridge",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = 0;
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
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
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
            "%s: expected query OK, got %d / %d %s %s\n",
            expected.context,
            rc,
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
        "%s/mylite_information_schema_core_%d_%s.mylite",
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
        if (actual != expected) {
            fprintf(stderr, "%s: expected %s, got %s\n", context, expected, actual);
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(stderr, "%s: expected '%s' to contain '%s'\n", context, actual, needle);
        return 1;
    }
    return 0;
}
