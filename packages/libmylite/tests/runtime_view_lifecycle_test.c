#include "mylite_test_support.h"

#include <mylite/mylite.h>

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
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_table_exists = 1050,
    mysql_error_unknown_table = 1051,
    mysql_error_unknown_column = 1054,
    mysql_error_duplicate_column = 1060,
    mysql_error_check_option_on_non_updatable_view = 1368,
    mysql_error_not_view = 1347,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_unknown_database = 1049,
    show_columns_result_column_count = 6,
    information_schema_views_result_column_count = 9,
    information_schema_tables_result_column_count = 10,
    information_schema_columns_result_column_count = 9,
    show_table_status_create_time_column = 11,
    show_table_status_comment_column = 17,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_result {
    const char *sql;
    const char *const *columns;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_view_metadata_surfaces(void);
static int test_view_option_metadata(void);
static int test_view_diagnostics_and_drop_semantics(void);
static int test_view_persistence_and_preamble(void);
static int test_view_transaction_boundaries(void);
static int test_view_drop_database_cleanup(void);
static int create_seed_schema(mylite_db *database);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_ok_affected_rows(
    mylite_db *database,
    const char *sql,
    int64_t expected_affected_rows,
    const char *context
);
static int expect_statement_ok_warnings(
    mylite_db *database,
    const char *sql,
    size_t expected_warnings,
    const char *context
);
static int expect_query(mylite_db *database, struct expected_result expected);
static int expect_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_view_metadata_surfaces();
    failures += test_view_option_metadata();
    failures += test_view_diagnostics_and_drop_semantics();
    failures += test_view_persistence_and_preamble();
    failures += test_view_transaction_boundaries();
    failures += test_view_drop_database_cleanup();

    return failures == 0 ? 0 : 1;
}

static int test_view_metadata_surfaces(void) {
    static const char *const show_create_columns[] = {
        "View",
        "Create View",
        "character_set_client",
        "collation_connection",
    };
    static const char show_create_sql[] =
        "CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`%` SQL SECURITY DEFINER VIEW `v` AS "
        "select `t`.`id` AS `id`,`t`.`name` AS `label` from `t`";
    static const char *const show_create_values[] = {
        "v",
        show_create_sql,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const show_full_tables_columns[] = {"Tables_in_app", "Table_type"};
    static const char *const show_full_tables_values[] = {
        "t",
        "BASE TABLE",
        "v",
        "VIEW",
    };
    static const char *const show_columns_columns[] = {
        "Field",
        "Type",
        "Null",
        "Key",
        "Default",
        "Extra",
    };
    static const char *const show_columns_values[] = {
        "id",
        "int",
        "NO",
        "",
        NULL,
        "",
        "label",
        "varchar(20)",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const views_columns[] = {
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "VIEW_DEFINITION",
        "CHECK_OPTION",
        "IS_UPDATABLE",
        "DEFINER",
        "SECURITY_TYPE",
        "CHARACTER_SET_CLIENT",
        "COLLATION_CONNECTION",
    };
    static const char *const views_values[] = {
        "app",
        "v",
        "select `app`.`t`.`id` AS `id`,`app`.`t`.`name` AS `label` from `app`.`t`",
        "NONE",
        "YES",
        "root@%",
        "DEFINER",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const usage_columns[] = {
        "VIEW_SCHEMA",
        "VIEW_NAME",
        "TABLE_SCHEMA",
        "TABLE_NAME",
    };
    static const char *const usage_values[] = {"app", "v", "app", "t"};
    static const char *const tables_columns[] = {
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "VERSION",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "DATA_LENGTH",
        "AUTO_INCREMENT",
        "TABLE_COLLATION",
        "TABLE_COMMENT",
    };
    static const char *const tables_values[] = {
        "t",  "BASE TABLE", "InnoDB", "10", "Dynamic", "0",  "16384", NULL, "utf8mb4_0900_ai_ci",
        "",   "v",          "VIEW",   NULL, NULL,      NULL, NULL,    NULL, NULL,
        NULL, "VIEW",
    };
    static const char *const columns_columns[] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "DATA_TYPE",
        "COLUMN_TYPE",
        "IS_NULLABLE",
        "COLUMN_DEFAULT",
        "COLUMN_KEY",
        "EXTRA",
    };
    static const char *const columns_values[] = {
        "v",
        "id",
        "1",
        "int",
        "int",
        "NO",
        NULL,
        "",
        "",
        "v",
        "label",
        "2",
        "varchar",
        "varchar(20)",
        "YES",
        NULL,
        "",
        "",
    };
    static const char *const alias_columns[] = {"VIEW_DEFINITION"};
    static const char *const alias_values[] = {
        "select `a`.`id` AS `x` from `app`.`t` `a`",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *status = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "metadata") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open metadata database");
    failures += create_seed_schema(database);
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci");
    failures += expect_statement_ok(database, "CREATE VIEW v AS SELECT id, name AS label FROM t");

    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SHOW CREATE VIEW v",
            .columns = show_create_columns,
            .values = show_create_values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "show create view",
        }
    );
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SHOW CREATE TABLE v",
            .columns = show_create_columns,
            .values = show_create_values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "show create table view",
        }
    );
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SHOW FULL TABLES",
            .columns = show_full_tables_columns,
            .values = show_full_tables_values,
            .column_count = 2U,
            .row_count = 2U,
            .context = "show full tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SHOW COLUMNS FROM v",
            .columns = show_columns_columns,
            .values = show_columns_values,
            .column_count = show_columns_result_column_count,
            .row_count = 2U,
            .context = "show columns from view",
        }
    );
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SELECT TABLE_SCHEMA,TABLE_NAME,VIEW_DEFINITION,CHECK_OPTION,IS_UPDATABLE,"
                   "DEFINER,SECURITY_TYPE,CHARACTER_SET_CLIENT,COLLATION_CONNECTION "
                   "FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'v'",
            .columns = views_columns,
            .values = views_values,
            .column_count = information_schema_views_result_column_count,
            .row_count = 1U,
            .context = "information schema views",
        }
    );
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SELECT VIEW_SCHEMA,VIEW_NAME,TABLE_SCHEMA,TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE "
                   "WHERE VIEW_SCHEMA = 'app' AND VIEW_NAME = 'v'",
            .columns = usage_columns,
            .values = usage_values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "information schema view table usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,"
                   "DATA_LENGTH,AUTO_INCREMENT,TABLE_COLLATION,TABLE_COMMENT "
                   "FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'app' ORDER BY TABLE_NAME",
            .columns = tables_columns,
            .values = tables_values,
            .column_count = information_schema_tables_result_column_count,
            .row_count = 2U,
            .context = "information schema tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,DATA_TYPE,COLUMN_TYPE,"
                   "IS_NULLABLE,COLUMN_DEFAULT,COLUMN_KEY,EXTRA "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'v' ORDER BY ORDINAL_POSITION",
            .columns = columns_columns,
            .values = columns_values,
            .column_count = information_schema_columns_result_column_count,
            .row_count = 2U,
            .context = "information schema columns",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE VIEW alias_v AS SELECT a.id AS x FROM t AS a");
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SELECT VIEW_DEFINITION FROM INFORMATION_SCHEMA.VIEWS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'alias_v'",
            .columns = alias_columns,
            .values = alias_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "aliased view definition",
        }
    );

    failures += expect_statement_ok(database, "CREATE TEMPORARY TABLE v (id INT)");
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SHOW CREATE VIEW v",
            .columns = show_create_columns,
            .values = show_create_values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "show create view ignores temporary shadow",
        }
    );
    failures += expect_statement_ok(database, "DROP TEMPORARY TABLE v");

    failures += mylite_test_expect_int(
        mylite_execute(
            database,
            "SHOW TABLE STATUS LIKE 'v'",
            strlen("SHOW TABLE STATUS LIKE 'v'"),
            &status
        ),
        MYLITE_OK,
        "show table status view"
    );
    if (status == NULL) {
        failures += 1;
    } else {
        failures +=
            mylite_test_expect_size(mylite_result_row_count(status), 1U, "show table status rows");
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(status, 0U, 0U),
            "v",
            "status name"
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(status, 0U, 1U),
            NULL,
            "status engine"
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(status, 0U, 2U),
            NULL,
            "status version"
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(status, 0U, 3U),
            NULL,
            "status row format"
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(status, 0U, 4U),
            NULL,
            "status rows"
        );
        if (mylite_result_value_text(status, 0U, show_table_status_create_time_column) == NULL) {
            fprintf(stderr, "status create time: expected non-NULL value\n");
            failures += 1;
        }
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(status, 0U, show_table_status_comment_column),
            "VIEW",
            "status comment"
        );
    }
    mylite_result_free(status);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_view_option_metadata(void) {
    static const char *const show_create_columns[] = {
        "View",
        "Create View",
        "character_set_client",
        "collation_connection",
    };
    static const char option_show_create_sql[] =
        "CREATE ALGORITHM=MERGE DEFINER=`app`@`example.com` SQL SECURITY INVOKER VIEW `option_v` "
        "(`view_id`,`label`) AS select `t`.`id` AS `id`,`t`.`name` AS `name` from `t` "
        "WITH LOCAL CHECK OPTION";
    static const char *const option_show_create_values[] = {
        "option_v",
        option_show_create_sql,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const views_columns[] = {
        "TABLE_NAME",
        "VIEW_DEFINITION",
        "CHECK_OPTION",
        "IS_UPDATABLE",
        "DEFINER",
        "SECURITY_TYPE",
    };
    static const char *const option_views_values[] = {
        "option_v",
        "select `app`.`t`.`id` AS `id`,`app`.`t`.`name` AS `name` from `app`.`t`",
        "LOCAL",
        "YES",
        "app@example.com",
        "INVOKER",
    };
    static const char *const replace_columns[] = {
        "TABLE_NAME",
        "CHECK_OPTION",
        "IS_UPDATABLE",
        "SECURITY_TYPE",
    };
    static const char *const replace_values[] = {"option_v", "NONE", "NO", "DEFINER"};
    static const char alter_show_create_sql[] =
        "CREATE ALGORITHM=MERGE DEFINER=`root`@`%` SQL SECURITY INVOKER VIEW `alter_v` "
        "(`x`) AS select `t`.`id` AS `id` from `t` WITH CASCADED CHECK OPTION";
    static const char *const alter_show_create_values[] = {
        "alter_v",
        alter_show_create_sql,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const alter_values[] = {"alter_v", "CASCADED", "YES", "INVOKER"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "view_options") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open view options database"
    );
    failures += create_seed_schema(database);
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci");
    failures += expect_statement_ok(
        database,
        "CREATE ALGORITHM=MERGE DEFINER='app'@'example.com' SQL SECURITY INVOKER "
        "VIEW option_v (view_id, label) "
        "AS SELECT id, name FROM t WITH LOCAL CHECK OPTION"
    );
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SHOW CREATE VIEW option_v",
            .columns = show_create_columns,
            .values = option_show_create_values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "view options show create",
        }
    );
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SELECT TABLE_NAME,VIEW_DEFINITION,CHECK_OPTION,IS_UPDATABLE,DEFINER,"
                   "SECURITY_TYPE FROM INFORMATION_SCHEMA.VIEWS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'option_v'",
            .columns = views_columns,
            .values = option_views_values,
            .column_count = sizeof(views_columns) / sizeof(views_columns[0]),
            .row_count = 1U,
            .context = "view options information schema",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE OR REPLACE ALGORITHM=TEMPTABLE VIEW option_v AS SELECT id FROM t"
    );
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SELECT TABLE_NAME,CHECK_OPTION,IS_UPDATABLE,SECURITY_TYPE "
                   "FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'option_v'",
            .columns = replace_columns,
            .values = replace_values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "create or replace view metadata",
        }
    );

    failures += expect_statement_ok(database, "CREATE VIEW alter_v AS SELECT id FROM t");
    failures += expect_statement_ok(
        database,
        "ALTER ALGORITHM=MERGE SQL SECURITY INVOKER VIEW alter_v (x) AS SELECT id FROM t "
        "WITH CHECK OPTION"
    );
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SHOW CREATE VIEW alter_v",
            .columns = show_create_columns,
            .values = alter_show_create_values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "alter view show create",
        }
    );
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SELECT TABLE_NAME,CHECK_OPTION,IS_UPDATABLE,SECURITY_TYPE "
                   "FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'alter_v'",
            .columns = replace_columns,
            .values = alter_values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "alter view metadata",
        }
    );
    failures += expect_statement_ok_warnings(
        database,
        "DROP VIEW IF EXISTS missing_v RESTRICT",
        1U,
        "drop missing view restrict"
    );
    failures += expect_statement_ok(database, "DROP VIEW IF EXISTS option_v CASCADE");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_view_diagnostics_and_drop_semantics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open diagnostics database"
    );
    failures += create_seed_schema(database);

    failures += expect_error(
        database,
        "CREATE VIEW no_default AS SELECT id FROM app.t",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += expect_error(
        database,
        "CREATE VIEW missing_schema.v AS SELECT id FROM app.t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );

    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE VIEW v AS SELECT id FROM t");
    failures += expect_error(
        database,
        "SHOW CREATE VIEW t",
        (struct expected_sql_error){
            .code = mysql_error_not_view,
            .sqlstate = "HY000",
            .message_part = "'app.t' is not VIEW",
        }
    );
    failures += expect_error(
        database,
        "DROP VIEW t",
        (struct expected_sql_error){
            .code = mysql_error_not_view,
            .sqlstate = "HY000",
            .message_part = "'app.t' is not VIEW",
        }
    );
    failures += expect_error(
        database,
        "DROP TABLE v",
        (struct expected_sql_error){
            .code = mysql_error_unknown_table,
            .sqlstate = "42S02",
            .message_part = "Unknown table 'app.v'",
        }
    );
    failures += expect_error(
        database,
        "CREATE VIEW missing_source AS SELECT id FROM missing_t",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_t' doesn't exist",
        }
    );
    failures += expect_error(
        database,
        "CREATE VIEW missing_column AS SELECT missing_col FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing_col' in 'field list'",
        }
    );
    failures += expect_error(
        database,
        "CREATE VIEW duplicate_column AS SELECT id, id FROM t",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_column,
            .sqlstate = "42S21",
            .message_part = "Duplicate column name 'id'",
        }
    );
    failures += expect_error(
        database,
        "CREATE VIEW v AS SELECT id FROM t",
        (struct expected_sql_error){
            .code = mysql_error_table_exists,
            .sqlstate = "42S01",
            .message_part = "Table 'v' already exists",
        }
    );
    failures += expect_error(
        database,
        "CREATE VIEW _mylite_v AS SELECT id FROM t",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_v'",
        }
    );
    failures += expect_error(
        database,
        "CREATE VIEW expression_v AS SELECT id + 1 AS x FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CREATE VIEW supports only direct column projection",
        }
    );
    failures += expect_error(
        database,
        "CREATE ALGORITHM=TEMPTABLE VIEW check_v AS SELECT id FROM t WITH CHECK OPTION",
        (struct expected_sql_error){
            .code = mysql_error_check_option_on_non_updatable_view,
            .sqlstate = "HY000",
            .message_part = "CHECK OPTION on non-updatable view 'app.check_v'",
        }
    );
    failures +=
        expect_statement_ok(database, "CREATE OR REPLACE VIEW replace_v AS SELECT id FROM t");
    failures += expect_error(
        database,
        "CREATE OR REPLACE VIEW t AS SELECT id FROM t",
        (struct expected_sql_error){
            .code = mysql_error_table_exists,
            .sqlstate = "42S01",
            .message_part = "Table 't' already exists",
        }
    );
    failures += expect_error(
        database,
        "ALTER VIEW missing_view AS SELECT id FROM t",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_view' doesn't exist",
        }
    );
    failures += expect_error(
        database,
        "ALTER VIEW t AS SELECT id FROM t",
        (struct expected_sql_error){
            .code = mysql_error_not_view,
            .sqlstate = "HY000",
            .message_part = "'app.t' is not VIEW",
        }
    );
    failures += expect_error(
        database,
        "DROP VIEW missing_v",
        (struct expected_sql_error){
            .code = mysql_error_unknown_table,
            .sqlstate = "42S02",
            .message_part = "Unknown table 'app.missing_v'",
        }
    );
    failures += expect_statement_ok_warnings(
        database,
        "DROP VIEW IF EXISTS missing_v",
        1U,
        "drop missing view if exists"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_view_persistence_and_preamble(void) {
    static const char *const count_columns[] = {"COUNT(*)"};
    static const char *const count_one[] = {"1"};
    static const char *const usage_columns[] = {
        "VIEW_SCHEMA",
        "VIEW_NAME",
        "TABLE_SCHEMA",
        "TABLE_NAME",
    };
    static const char *const usage_values[] = {"app", "keep_v", "app", "t"};
    static const char *const show_create_columns[] = {
        "View",
        "Create View",
        "character_set_client",
        "collation_connection",
    };
    static const char show_create_sql[] =
        "CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`%` SQL SECURITY DEFINER VIEW `keep_v` AS "
        "select `t`.`id` AS `id` from `t`";
    static const char *const show_create_values[] = {
        "keep_v",
        show_create_sql,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "persistence") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open persistence database"
    );
    failures += create_seed_schema(database);
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE VIEW keep_v AS SELECT id FROM t");
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after create view"
    );
    mylite_close(database);
    database = NULL;

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "reopen persistence database"
    );
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SHOW CREATE VIEW keep_v",
            .columns = show_create_columns,
            .values = show_create_values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "reopened show create view",
        }
    );
    failures += expect_statement_ok(database, "RENAME TABLE t TO renamed_t");
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SELECT VIEW_SCHEMA,VIEW_NAME,TABLE_SCHEMA,TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE "
                   "WHERE VIEW_SCHEMA = 'app' AND VIEW_NAME = 'keep_v'",
            .columns = usage_columns,
            .values = usage_values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "view table usage after source rename",
        }
    );
    failures += expect_statement_ok(database, "DROP TABLE renamed_t");
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SELECT VIEW_SCHEMA,VIEW_NAME,TABLE_SCHEMA,TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE "
                   "WHERE VIEW_SCHEMA = 'app' AND VIEW_NAME = 'keep_v'",
            .columns = usage_columns,
            .values = usage_values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "view table usage after source drop",
        }
    );
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SHOW CREATE VIEW keep_v",
            .columns = show_create_columns,
            .values = show_create_values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "show create view after source drop",
        }
    );
    failures += expect_error(
        database,
        "DROP VIEW keep_v, missing_atomic",
        (struct expected_sql_error){
            .code = mysql_error_unknown_table,
            .sqlstate = "42S02",
            .message_part = "Unknown table 'app.missing_atomic'",
        }
    );
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEWS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'keep_v'",
            .columns = count_columns,
            .values = count_one,
            .column_count = 1U,
            .row_count = 1U,
            .context = "atomic drop keeps view",
        }
    );
    failures += expect_statement_ok_warnings(
        database,
        "DROP VIEW IF EXISTS keep_v, missing_v",
        1U,
        "drop view if exists warning"
    );
    failures += expect_error(
        database,
        "SHOW CREATE VIEW keep_v",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.keep_v' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_view_transaction_boundaries(void) {
    static const char *const count_columns[] = {"COUNT(*)"};
    static const char *const count_one[] = {"1"};
    static const char *const count_two[] = {"2"};
    static const char *const show_create_columns[] = {
        "View",
        "Create View",
        "character_set_client",
        "collation_connection",
    };
    static const char show_create_sql[] =
        "CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`%` SQL SECURITY DEFINER VIEW `tx_v` AS "
        "select `t`.`id` AS `id` from `t`";
    static const char *const show_create_values[] = {
        "tx_v",
        show_create_sql,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "transactions") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open transaction database"
    );
    failures += create_seed_schema(database);
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "START TRANSACTION");
    failures += expect_statement_ok(database, "INSERT INTO t VALUES (1, 'a')");
    failures += expect_statement_ok(database, "CREATE VIEW tx_v AS SELECT id FROM t");
    failures += expect_statement_ok(database, "ROLLBACK");
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM t",
            .columns = count_columns,
            .values = count_one,
            .column_count = 1U,
            .row_count = 1U,
            .context = "create view commits active transaction",
        }
    );
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SHOW CREATE VIEW tx_v",
            .columns = show_create_columns,
            .values = show_create_values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "create view descriptor survives rollback",
        }
    );
    failures += expect_statement_ok(database, "START TRANSACTION");
    failures += expect_statement_ok(database, "INSERT INTO t VALUES (2, 'b')");
    failures += expect_statement_ok(database, "DROP VIEW tx_v");
    failures += expect_statement_ok(database, "ROLLBACK");
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM t",
            .columns = count_columns,
            .values = count_two,
            .column_count = 1U,
            .row_count = 1U,
            .context = "drop view commits active transaction",
        }
    );
    failures += expect_error(
        database,
        "SHOW CREATE VIEW tx_v",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.tx_v' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_view_drop_database_cleanup(void) {
    static const char *const count_columns[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "drop_database") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open drop database file");
    failures += create_seed_schema(database);
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE VIEW cleanup_v AS SELECT id FROM t");
    failures += expect_statement_ok_affected_rows(
        database,
        "DROP DATABASE app",
        2,
        "drop database removes table and view descriptors"
    );
    failures += expect_query(
        database,
        (struct expected_result){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.SCHEMATA "
                   "WHERE SCHEMA_NAME = 'app'",
            .columns = count_columns,
            .values = count_zero,
            .column_count = 1U,
            .row_count = 1U,
            .context = "drop database removes schema",
        }
    );
    failures += expect_error(
        database,
        "SHOW CREATE VIEW app.cleanup_v",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'app'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int create_seed_schema(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "CREATE DATABASE other");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE app.t (id INT NOT NULL, name VARCHAR(20) NULL)"
    );
    failures += expect_statement_ok(database, "CREATE TABLE other.t (id INT NOT NULL)");

    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    return expect_statement_ok_warnings(database, sql, 0U, sql);
}

static int expect_statement_ok_affected_rows(
    mylite_db *database,
    const char *sql,
    int64_t expected_affected_rows,
    const char *context
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            context,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (result == NULL) {
        fprintf(stderr, "%s: expected result object\n", context);
        return 1;
    }
    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, context);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, context);
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        expected_affected_rows,
        context
    );
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, context);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok_warnings(
    mylite_db *database,
    const char *sql,
    size_t expected_warnings,
    const char *context
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            context,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (result == NULL) {
        fprintf(stderr, "%s: expected result object\n", context);
        return 1;
    }
    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, context);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, context);
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), expected_warnings, context);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_result expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            expected.context,
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
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.columns[column_index],
            expected.context
        );
    }
    for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            size_t value_index = (row_index * expected.column_count) + column_index;

            failures += mylite_test_expect_text_or_null(
                mylite_result_value_text(result, row_index, column_index),
                expected.values[value_index],
                expected.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected error %d/%s, got success\n",
            sql,
            expected.code,
            expected.sqlstate
        );
        mylite_result_free(result);
        return 1;
    }

    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
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
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read file\n", path);
        fclose(file);
        return 1;
    }

    fclose(file);
    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) == 0) {
        return 0;
    }
    fprintf(stderr, "%s: byte comparison failed\n", context);
    return 1;
}
