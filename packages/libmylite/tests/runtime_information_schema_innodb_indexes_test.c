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

static int test_information_schema_innodb_index_rows(void);
static int setup_index_schema(mylite_db *database);
static int expect_statement_ok(mylite_db *database, struct expected_statement expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);

int main(void) {
    int failures = 0;

    failures += test_information_schema_innodb_index_rows();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_innodb_index_rows(void) {
    static const char *const index_columns[] = {
        "INDEX_ID",
        "NAME",
        "TABLE_ID",
        "TYPE",
        "N_FIELDS",
        "PAGE_NO",
        "SPACE",
        "MERGE_THRESHOLD",
    };
    static const char *const index_values[] = {
        "4", "ft_body", "1", "32",    "1", "-1",   "0", "50",      "3", "ix_b_desc",
        "1", "0",       "2", "0",     "0", "50",   "1", "PRIMARY", "1", "3",
        "7", "0",       "0", "50",    "5", "sp_p", "1", "64",      "2", "0",
        "0", "50",      "2", "uq_ab", "1", "2",    "3", "0",       "0", "50",
    };
    static const char *const field_columns[] = {
        "INDEX_ID",
        "NAME",
        "POS",
    };
    static const char *const field_values[] = {
        "1",
        "id",
        "0",
        "2",
        "a",
        "0",
        "2",
        "b",
        "1",
        "3",
        "b",
        "0",
        "4",
        "body",
        "0",
        "5",
        "p",
        "0",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_six[] = {"6"};
    static const char *const count_five[] = {"5"};
    static const char *const count_four[] = {"4"};
    static const char *const count_two[] = {"2"};
    static const char *const alias_columns[] = {"NAME", "TYPE"};
    static const char *const alias_values[] = {
        "ft_body",
        "32",
        "ix_b_desc",
        "0",
        "PRIMARY",
        "3",
        "sp_p",
        "64",
        "uq_ab",
        "2",
    };
    static const char *const renamed_index_values[] = {
        "ft_body",
        "32",
        "ix_b2",
        "0",
        "PRIMARY",
        "3",
        "sp_p",
        "64",
    };
    static const char *const clustered_columns[] = {"NAME", "TYPE", "N_FIELDS"};
    static const char *const unique_clustered_values[] = {
        "ix_b",
        "0",
        "2",
        "uq_a",
        "3",
        "4",
    };
    static const char *const generated_clustered_values[] = {
        "GEN_CLUST_INDEX",
        "1",
        "5",
        "ix_b",
        "0",
        "2",
        "uq_a",
        "2",
        "2",
    };
    static const char *const no_index_values[] = {
        "GEN_CLUST_INDEX",
        "1",
        "5",
    };
    static const char *const clustered_field_columns[] = {"INDEX_ID", "NAME", "POS"};
    static const char *const clustered_field_values[] = {
        "6",
        "a",
        "0",
        "7",
        "b",
        "0",
        "8",
        "a",
        "0",
        "9",
        "b",
        "0",
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
        "INNODB_FIELDS",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
        NULL,
        "INNODB_INDEXES",
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
        "INNODB_FIELDS",
        "INDEX_ID",
        "1",
        NULL,
        "YES",
        "varbinary",
        "256",
        "256",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "varbinary(256)",
        "select",
        "INNODB_FIELDS",
        "NAME",
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
        "INNODB_FIELDS",
        "POS",
        "3",
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
        "INNODB_INDEXES",
        "INDEX_ID",
        "1",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_INDEXES",
        "NAME",
        "2",
        "",
        "NO",
        "varchar",
        "64",
        "193",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(193)",
        "select",
        "INNODB_INDEXES",
        "TABLE_ID",
        "3",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_INDEXES",
        "TYPE",
        "4",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int",
        "select",
        "INNODB_INDEXES",
        "N_FIELDS",
        "5",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int",
        "select",
        "INNODB_INDEXES",
        "PAGE_NO",
        "6",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int",
        "select",
        "INNODB_INDEXES",
        "SPACE",
        "7",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int",
        "select",
        "INNODB_INDEXES",
        "MERGE_THRESHOLD",
        "8",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int",
        "select",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "rows") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open innodb index db");
    failures += setup_index_schema(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_ID, NAME, TABLE_ID, TYPE, N_FIELDS, PAGE_NO, SPACE, "
                   "MERGE_THRESHOLD FROM INFORMATION_SCHEMA.INNODB_INDEXES "
                   "WHERE TABLE_ID = 1 ORDER BY NAME",
            .column_names = index_columns,
            .column_count = sizeof(index_columns) / sizeof(index_columns[0]),
            .values = index_values,
            .row_count = sizeof(index_values) / sizeof(index_values[0]) /
                         (sizeof(index_columns) / sizeof(index_columns[0])),
            .context = "innodb index rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_ID, NAME, POS FROM INFORMATION_SCHEMA.INNODB_FIELDS "
                   "WHERE INDEX_ID IN (1, 2, 3, 4, 5) ORDER BY INDEX_ID",
            .column_names = field_columns,
            .column_count = sizeof(field_columns) / sizeof(field_columns[0]),
            .values = field_values,
            .row_count = sizeof(field_values) / sizeof(field_values[0]) /
                         (sizeof(field_columns) / sizeof(field_columns[0])),
            .context = "innodb field rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_INDEXES WHERE TABLE_ID = 1",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_five,
            .row_count = 1U,
            .context = "innodb index count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FIELDS "
                   "WHERE INDEX_ID IN (1, 2, 3, 4, 5)",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_six,
            .row_count = 1U,
            .context = "innodb field count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NAME, TYPE, N_FIELDS "
                   "FROM INFORMATION_SCHEMA.INNODB_INDEXES "
                   "WHERE TABLE_ID = 2 ORDER BY NAME",
            .column_names = clustered_columns,
            .column_count = sizeof(clustered_columns) / sizeof(clustered_columns[0]),
            .values = unique_clustered_values,
            .row_count = sizeof(unique_clustered_values) / sizeof(unique_clustered_values[0]) /
                         (sizeof(clustered_columns) / sizeof(clustered_columns[0])),
            .context = "innodb unique clustered fallback index rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NAME, TYPE, N_FIELDS "
                   "FROM INFORMATION_SCHEMA.INNODB_INDEXES "
                   "WHERE TABLE_ID = 3 ORDER BY NAME",
            .column_names = clustered_columns,
            .column_count = sizeof(clustered_columns) / sizeof(clustered_columns[0]),
            .values = generated_clustered_values,
            .row_count = sizeof(generated_clustered_values) /
                         sizeof(generated_clustered_values[0]) /
                         (sizeof(clustered_columns) / sizeof(clustered_columns[0])),
            .context = "innodb generated clustered fallback index rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NAME, TYPE, N_FIELDS "
                   "FROM INFORMATION_SCHEMA.INNODB_INDEXES "
                   "WHERE TABLE_ID = 4 ORDER BY NAME",
            .column_names = clustered_columns,
            .column_count = sizeof(clustered_columns) / sizeof(clustered_columns[0]),
            .values = no_index_values,
            .row_count = sizeof(no_index_values) / sizeof(no_index_values[0]) /
                         (sizeof(clustered_columns) / sizeof(clustered_columns[0])),
            .context = "innodb generated clustered no-index row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_ID, NAME, POS FROM INFORMATION_SCHEMA.INNODB_FIELDS "
                   "WHERE INDEX_ID IN (6, 7, 8, 9) ORDER BY INDEX_ID",
            .column_names = clustered_field_columns,
            .column_count = sizeof(clustered_field_columns) / sizeof(clustered_field_columns[0]),
            .values = clustered_field_values,
            .row_count = sizeof(clustered_field_values) / sizeof(clustered_field_values[0]) /
                         (sizeof(clustered_field_columns) / sizeof(clustered_field_columns[0])),
            .context = "innodb clustered fallback field rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_INDEXES "
                   "WHERE NAME = 'GEN_CLUST_INDEX'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_two,
            .row_count = 1U,
            .context = "innodb generated clustered index count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_indexes WHERE TABLE_ID = 1",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_five,
            .row_count = 1U,
            .context = "case-insensitive innodb indexes table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT i.NAME, i.TYPE FROM INFORMATION_SCHEMA.INNODB_INDEXES AS i "
                   "WHERE i.TABLE_ID = 1 AND i.TYPE IN (0, 2, 3, 32, 64) ORDER BY i.NAME",
            .column_names = alias_columns,
            .column_count = sizeof(alias_columns) / sizeof(alias_columns[0]),
            .values = alias_values,
            .row_count = sizeof(alias_values) / sizeof(alias_values[0]) /
                         (sizeof(alias_columns) / sizeof(alias_columns[0])),
            .context = "innodb indexes alias predicate",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE information_schema",
            .context = "use information_schema for innodb indexes",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INNODB_INDEXES WHERE TABLE_ID = 1",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_five,
            .row_count = 1U,
            .context = "unqualified innodb indexes count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INNODB_FIELDS WHERE INDEX_ID IN (1, 2, 3, 4, 5)",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_six,
            .row_count = 1U,
            .context = "unqualified innodb fields count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME IN "
                   "('INNODB_INDEXES', 'INNODB_FIELDS') ORDER BY TABLE_NAME",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = sizeof(system_table_values) / sizeof(system_table_values[0]) /
                         (sizeof(system_table_columns) / sizeof(system_table_columns[0])),
            .context = "innodb index system table rows",
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
                   "('INNODB_INDEXES', 'INNODB_FIELDS') ORDER BY TABLE_NAME",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .context = "innodb index columns metadata",
        }
    );
    failures += expect_row_count_status(database, "innodb index row count status");

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen innodb index db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_INDEXES WHERE TABLE_ID = 1",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_five,
            .row_count = 1U,
            .context = "reopened innodb index count",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE app",
            .context = "use app for innodb index rename drop",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "ALTER TABLE idx_sample RENAME INDEX ix_b_desc TO ix_b2",
            .context = "rename innodb index descriptor",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "DROP INDEX uq_ab ON idx_sample",
            .context = "drop innodb index descriptor",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NAME, TYPE FROM INFORMATION_SCHEMA.INNODB_INDEXES "
                   "WHERE TABLE_ID = 1 ORDER BY NAME",
            .column_names = alias_columns,
            .column_count = sizeof(alias_columns) / sizeof(alias_columns[0]),
            .values = renamed_index_values,
            .row_count = sizeof(renamed_index_values) / sizeof(renamed_index_values[0]) /
                         (sizeof(alias_columns) / sizeof(alias_columns[0])),
            .context = "innodb index rows after rename drop",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FIELDS "
                   "WHERE INDEX_ID IN (1, 3, 4, 5)",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_four,
            .row_count = 1U,
            .context = "innodb field count after drop",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int setup_index_schema(mylite_db *database) {
    static const struct expected_statement statements[] = {
        {.sql = "CREATE DATABASE app", .context = "create app schema"},
        {.sql = "USE app", .context = "use app schema"},
        {.sql = "CREATE TABLE idx_sample("
                "id INT NOT NULL, a INT NOT NULL, b INT NOT NULL, body TEXT, p POINT NOT NULL, "
                "PRIMARY KEY(id), UNIQUE KEY uq_ab(a,b), KEY ix_b_desc(b DESC), "
                "FULLTEXT KEY ft_body(body), SPATIAL KEY sp_p(p))",
         .context = "create indexed table"},
        {.sql = "CREATE TABLE unique_clustered("
                "a INT NOT NULL, b INT NOT NULL, UNIQUE KEY uq_a(a), KEY ix_b(b))",
         .context = "create unique clustered fallback table"},
        {.sql = "CREATE TABLE generated_clustered("
                "a INT, b INT, UNIQUE KEY uq_a(a), KEY ix_b(b))",
         .context = "create generated clustered fallback table"},
        {.sql = "CREATE TABLE no_index(a INT, b INT)",
         .context = "create generated clustered no-index table"},
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

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            size_t value_index = (row_index * expected.column_count) + column_index;

            failures += expect_text_or_null(
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

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_information_schema_innodb_indexes_%s_%d.mylite",
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
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected text %s, got %s\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}
