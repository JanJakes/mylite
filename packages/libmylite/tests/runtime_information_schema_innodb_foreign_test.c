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
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

struct expected_statement {
    const char *sql;
    const char *context;
};

static int test_information_schema_innodb_foreign_rows(void);
static int setup_foreign_key_schema(mylite_db *database);
static int expect_statement_ok(mylite_db *database, struct expected_statement expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_information_schema_innodb_foreign_rows();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_innodb_foreign_rows(void) {
    static const char *const foreign_columns[] = {
        "ID",
        "FOR_NAME",
        "REF_NAME",
        "N_COLS",
        "TYPE",
    };
    static const char *const foreign_values[] = {
        "app/child_default_ibfk_1",
        "app/child_default",
        "app/parent",
        "1",
        "48",
        "app/fk_cascade",
        "app/child_cascade",
        "app/parent",
        "1",
        "5",
        "app/fk_delete_cascade_update_restrict",
        "app/child_delete_cascade_update_restrict",
        "app/parent",
        "1",
        "1",
        "app/fk_delete_noaction_update_setnull",
        "app/child_delete_noaction_update_setnull",
        "app/parent",
        "1",
        "24",
        "app/fk_delete_restrict_update_cascade",
        "app/child_delete_restrict_update_cascade",
        "app/parent",
        "1",
        "4",
        "app/fk_delete_setnull_update_noaction",
        "app/child_delete_setnull_update_noaction",
        "app/parent",
        "1",
        "34",
        "app/fk_noaction",
        "app/child_noaction",
        "app/parent",
        "1",
        "48",
        "app/fk_restrict",
        "app/child_restrict",
        "app/parent",
        "1",
        "0",
        "app/fk_setnull",
        "app/child_setnull",
        "app/parent",
        "2",
        "10",
    };
    static const char *const foreign_cols_columns[] = {
        "ID",
        "FOR_COL_NAME",
        "REF_COL_NAME",
        "POS",
    };
    static const char *const foreign_cols_values[] = {
        "app/child_default_ibfk_1",
        "pid",
        "id",
        "1",
        "app/fk_cascade",
        "pid",
        "id",
        "1",
        "app/fk_delete_cascade_update_restrict",
        "pid",
        "id",
        "1",
        "app/fk_delete_noaction_update_setnull",
        "pid",
        "id",
        "1",
        "app/fk_delete_restrict_update_cascade",
        "pid",
        "id",
        "1",
        "app/fk_delete_setnull_update_noaction",
        "pid",
        "id",
        "1",
        "app/fk_noaction",
        "pid",
        "id",
        "1",
        "app/fk_restrict",
        "pid",
        "id",
        "1",
        "app/fk_setnull",
        "pid",
        "id",
        "1",
        "app/fk_setnull",
        "pb",
        "b",
        "2",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_ten[] = {"10"};
    static const char *const count_nine[] = {"9"};
    static const char *const count_eight[] = {"8"};
    static const char *const alias_columns[] = {"ID", "TYPE"};
    static const char *const alias_values[] = {
        "app/child_default_ibfk_1",
        "48",
        "app/fk_noaction",
        "48",
        "app/fk_restrict",
        "0",
        "app/fk_setnull",
        "10",
    };
    static const char *const system_table_columns[] = {
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "VERSION",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "DATA_LENGTH",
        "AUTO_INCREMENT",
    };
    static const char *const system_table_values[] = {
        "INNODB_FOREIGN",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
        NULL,
        "INNODB_FOREIGN_COLS",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
        NULL,
    };
    static const char *const columns_metadata_columns[] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "COLUMN_DEFAULT",
        "IS_NULLABLE",
        "DATA_TYPE",
        "CHARACTER_MAXIMUM_LENGTH",
        "CHARACTER_OCTET_LENGTH",
        "NUMERIC_PRECISION",
        "NUMERIC_SCALE",
        "DATETIME_PRECISION",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
        "COLUMN_TYPE",
        "PRIVILEGES",
    };
    static const char *const columns_metadata_values[] = {
        "INNODB_FOREIGN",
        "ID",
        "1",
        NULL,
        "YES",
        "varchar",
        "129",
        "387",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(129)",
        "select",
        "INNODB_FOREIGN",
        "FOR_NAME",
        "2",
        NULL,
        "YES",
        "varchar",
        "129",
        "387",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(129)",
        "select",
        "INNODB_FOREIGN",
        "REF_NAME",
        "3",
        NULL,
        "YES",
        "varchar",
        "129",
        "387",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(129)",
        "select",
        "INNODB_FOREIGN",
        "N_COLS",
        "4",
        "0",
        "NO",
        "bigint",
        NULL,
        NULL,
        "19",
        "0",
        NULL,
        NULL,
        NULL,
        "bigint",
        "select",
        "INNODB_FOREIGN",
        "TYPE",
        "5",
        "0",
        "NO",
        "bigint",
        NULL,
        NULL,
        "20",
        "0",
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_FOREIGN_COLS",
        "ID",
        "1",
        NULL,
        "YES",
        "varchar",
        "129",
        "387",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(129)",
        "select",
        "INNODB_FOREIGN_COLS",
        "FOR_COL_NAME",
        "2",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_tolower_ci",
        "varchar(64)",
        "select",
        "INNODB_FOREIGN_COLS",
        "REF_COL_NAME",
        "3",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_tolower_ci",
        "varchar(64)",
        "select",
        "INNODB_FOREIGN_COLS",
        "POS",
        "4",
        NULL,
        "NO",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        NULL,
        "int unsigned",
        "select",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "rows") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open innodb foreign db");
    failures += setup_foreign_key_schema(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ID, FOR_NAME, REF_NAME, N_COLS, TYPE "
                   "FROM INFORMATION_SCHEMA.INNODB_FOREIGN "
                   "WHERE ID LIKE 'app/%' ORDER BY ID",
            .column_names = foreign_columns,
            .column_count = sizeof(foreign_columns) / sizeof(foreign_columns[0]),
            .values = foreign_values,
            .row_count = sizeof(foreign_values) / sizeof(foreign_values[0]) /
                         (sizeof(foreign_columns) / sizeof(foreign_columns[0])),
            .context = "innodb foreign rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ID, FOR_COL_NAME, REF_COL_NAME, POS "
                   "FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS "
                   "WHERE ID LIKE 'app/%' ORDER BY ID",
            .column_names = foreign_cols_columns,
            .column_count = sizeof(foreign_cols_columns) / sizeof(foreign_cols_columns[0]),
            .values = foreign_cols_values,
            .row_count = sizeof(foreign_cols_values) / sizeof(foreign_cols_values[0]) /
                         (sizeof(foreign_cols_columns) / sizeof(foreign_cols_columns[0])),
            .context = "innodb foreign cols rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FOREIGN "
                   "WHERE ID LIKE 'app/%'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_nine,
            .row_count = 1U,
            .context = "innodb foreign count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS "
                   "WHERE ID LIKE 'app/%'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_ten,
            .row_count = 1U,
            .context = "innodb foreign cols count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_foreign "
                   "WHERE ID LIKE 'app/%'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_nine,
            .row_count = 1U,
            .context = "case-insensitive innodb foreign table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT f.ID, f.TYPE FROM INFORMATION_SCHEMA.INNODB_FOREIGN AS f "
                   "WHERE f.ID LIKE 'app/%' AND f.TYPE IN (0, 10, 48) ORDER BY f.ID",
            .column_names = alias_columns,
            .column_count = sizeof(alias_columns) / sizeof(alias_columns[0]),
            .values = alias_values,
            .row_count = sizeof(alias_values) / sizeof(alias_values[0]) /
                         (sizeof(alias_columns) / sizeof(alias_columns[0])),
            .context = "innodb foreign alias predicate",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE information_schema",
            .context = "use information_schema for innodb foreign",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INNODB_FOREIGN WHERE ID LIKE 'app/%'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_nine,
            .row_count = 1U,
            .context = "unqualified innodb foreign count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INNODB_FOREIGN_COLS WHERE ID LIKE 'app/%'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_ten,
            .row_count = 1U,
            .context = "unqualified innodb foreign cols count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME IN "
                   "('INNODB_FOREIGN', 'INNODB_FOREIGN_COLS') ORDER BY TABLE_NAME",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = sizeof(system_table_values) / sizeof(system_table_values[0]) /
                         (sizeof(system_table_columns) / sizeof(system_table_columns[0])),
            .context = "innodb foreign system table rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, "
                   "DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, "
                   "PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME IN "
                   "('INNODB_FOREIGN', 'INNODB_FOREIGN_COLS')",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .context = "innodb foreign columns metadata",
        }
    );
    failures += expect_row_count_status(database, "innodb foreign row count status");

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen innodb foreign db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FOREIGN WHERE ID LIKE 'app/%'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_nine,
            .row_count = 1U,
            .context = "reopened innodb foreign count",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE app",
            .context = "use app for innodb foreign drop",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "ALTER TABLE child_restrict DROP FOREIGN KEY fk_restrict",
            .context = "drop innodb foreign descriptor",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FOREIGN WHERE ID LIKE 'app/%'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_eight,
            .row_count = 1U,
            .context = "innodb foreign count after drop",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS "
                   "WHERE ID LIKE 'app/%'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_nine,
            .row_count = 1U,
            .context = "innodb foreign cols count after drop",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int setup_foreign_key_schema(mylite_db *database) {
    static const struct expected_statement statements[] = {
        {.sql = "CREATE DATABASE app", .context = "create app schema"},
        {.sql = "USE app", .context = "use app schema"},
        {.sql = "CREATE TABLE parent(id INT NOT NULL PRIMARY KEY, b INT NOT NULL, "
                "UNIQUE KEY ub(id,b))",
         .context = "create parent table"},
        {.sql = "CREATE TABLE child_default(id INT NOT NULL PRIMARY KEY, pid INT, "
                "FOREIGN KEY(pid) REFERENCES parent(id))",
         .context = "create generated-name foreign key"},
        {.sql = "CREATE TABLE child_cascade(id INT NOT NULL PRIMARY KEY, pid INT, "
                "CONSTRAINT fk_cascade FOREIGN KEY(pid) REFERENCES parent(id) "
                "ON DELETE CASCADE ON UPDATE CASCADE)",
         .context = "create cascade foreign key"},
        {.sql = "CREATE TABLE child_delete_cascade_update_restrict("
                "id INT NOT NULL PRIMARY KEY, pid INT, "
                "CONSTRAINT fk_delete_cascade_update_restrict "
                "FOREIGN KEY(pid) REFERENCES parent(id) "
                "ON DELETE CASCADE ON UPDATE RESTRICT)",
         .context = "create delete-cascade update-restrict foreign key"},
        {.sql = "CREATE TABLE child_delete_restrict_update_cascade("
                "id INT NOT NULL PRIMARY KEY, pid INT, "
                "CONSTRAINT fk_delete_restrict_update_cascade "
                "FOREIGN KEY(pid) REFERENCES parent(id) "
                "ON DELETE RESTRICT ON UPDATE CASCADE)",
         .context = "create delete-restrict update-cascade foreign key"},
        {.sql = "CREATE TABLE child_delete_setnull_update_noaction("
                "id INT NOT NULL PRIMARY KEY, pid INT, "
                "CONSTRAINT fk_delete_setnull_update_noaction "
                "FOREIGN KEY(pid) REFERENCES parent(id) "
                "ON DELETE SET NULL ON UPDATE NO ACTION)",
         .context = "create delete-set-null update-no-action foreign key"},
        {.sql = "CREATE TABLE child_delete_noaction_update_setnull("
                "id INT NOT NULL PRIMARY KEY, pid INT, "
                "CONSTRAINT fk_delete_noaction_update_setnull "
                "FOREIGN KEY(pid) REFERENCES parent(id) "
                "ON DELETE NO ACTION ON UPDATE SET NULL)",
         .context = "create delete-no-action update-set-null foreign key"},
        {.sql = "CREATE TABLE child_setnull(id INT NOT NULL PRIMARY KEY, pid INT, pb INT, "
                "CONSTRAINT fk_setnull FOREIGN KEY(pid,pb) REFERENCES parent(id,b) "
                "ON DELETE SET NULL ON UPDATE SET NULL)",
         .context = "create set-null foreign key"},
        {.sql = "CREATE TABLE child_noaction(id INT NOT NULL PRIMARY KEY, pid INT, "
                "CONSTRAINT fk_noaction FOREIGN KEY(pid) REFERENCES parent(id) "
                "ON DELETE NO ACTION ON UPDATE NO ACTION)",
         .context = "create no-action foreign key"},
        {.sql = "CREATE TABLE child_restrict(id INT NOT NULL PRIMARY KEY, pid INT, "
                "CONSTRAINT fk_restrict FOREIGN KEY(pid) REFERENCES parent(id) "
                "ON DELETE RESTRICT ON UPDATE RESTRICT)",
         .context = "create restrict foreign key"},
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_ok(database, statements[index]);
    }
    return failures;
}

static int expect_statement_ok(mylite_db *database, struct expected_statement expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

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

    mylite_result_free(result);
    return 0;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
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
            expected.column_names[column_index],
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

static int expect_row_count_status(mylite_db *database, const char *context) {
    static const char *const status_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const status_values[] = {"0", "-1"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .column_names = status_columns,
            .column_count = 2U,
            .values = status_values,
            .row_count = 1U,
            .context = context,
        }
    );
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written > 0 && (size_t)written < sizeof(related)) {
        remove(related);
    }
}
