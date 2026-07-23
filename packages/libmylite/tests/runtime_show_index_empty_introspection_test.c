#include "mylite_test_support.h"

#include <mylite/mylite.h>

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
    show_index_column_count = 15,
    indexed_show_index_row_count = 5,
    decimal_base = 10,
    row_count_text_capacity = 32,
    suffix_capacity = 16,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_regexp_range = 3697,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_show_index_empty_result {
    const char *sql;
    const char *context;
};

struct expected_show_index_result {
    const char *sql;
    const char *context;
    const char *const *values;
    size_t row_count;
};

static const char *const show_index_names[show_index_column_count] = {
    "Table",
    "Non_unique",
    "Key_name",
    "Seq_in_index",
    "Column_name",
    "Collation",
    "Cardinality",
    "Sub_part",
    "Packed",
    "Null",
    "Index_type",
    "Comment",
    "Index_comment",
    "Visible",
    "Expression",
};

static int test_show_index_result_shape_persistence_rename_and_drop(void);
static int test_show_index_where_filters(void);
static int test_show_index_diagnostics_and_unsupported_forms(void);
static int test_independent_show_index_handles(void);
static int create_show_index_schema(mylite_db *database);
static int expect_show_index_empty_result(
    mylite_db *database,
    struct expected_show_index_empty_result expectation
);
static int expect_show_index_result(
    mylite_db *database,
    struct expected_show_index_result expectation
);
static int expect_row_count(mylite_db *database, int64_t expected, const char *context);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
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

    failures += test_show_index_result_shape_persistence_rename_and_drop();
    failures += test_show_index_where_filters();
    failures += test_show_index_diagnostics_and_unsupported_forms();
    failures += test_independent_show_index_handles();

    return failures == 0 ? 0 : 1;
}

static int test_show_index_result_shape_persistence_rename_and_drop(void) {
    static const char *const forms[] = {
        "SHOW INDEX FROM no_keys",
        "SHOW INDEX IN no_keys",
        "SHOW INDEXES FROM no_keys",
        "SHOW INDEXES IN no_keys",
        "SHOW KEYS FROM no_keys",
        "SHOW KEYS IN no_keys",
        "SHOW INDEX FROM app.no_keys",
        "SHOW INDEX FROM no_keys FROM app",
        "SHOW INDEX FROM no_keys IN app",
        "SHOW INDEX IN no_keys FROM app",
        "SHOW INDEX IN no_keys IN app",
        "SHOW INDEXES FROM no_keys FROM app",
        "SHOW KEYS IN no_keys IN app",
    };
    static const char *const trailing_schema_forms[] = {
        "SHOW INDEX FROM app.only_other FROM other",
        "SHOW INDEX FROM app.only_other IN other",
        "SHOW INDEX IN app.only_other FROM other",
        "SHOW INDEX IN app.only_other IN other",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    const struct mylite_session_state *session = NULL;
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    failures += create_show_index_schema(database);
    failures += expect_show_index_empty_result(
        database,
        (struct expected_show_index_empty_result){
            .sql = "SHOW INDEX FROM app.no_keys",
            .context = "qualified show index without default schema",
        }
    );
    failures += execute_statement_ok(database, "USE app");

    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;
    for (size_t form_index = 0U; form_index < sizeof(forms) / sizeof(forms[0]); ++form_index) {
        failures += expect_show_index_empty_result(
            database,
            (struct expected_show_index_empty_result){
                .sql = forms[form_index],
                .context = forms[form_index],
            }
        );
    }
    for (size_t form_index = 0U;
         form_index < sizeof(trailing_schema_forms) / sizeof(trailing_schema_forms[0]);
         ++form_index) {
        failures += expect_show_index_empty_result(
            database,
            (struct expected_show_index_empty_result){
                .sql = trailing_schema_forms[form_index],
                .context = trailing_schema_forms[form_index],
            }
        );
    }
    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation after show index"
    );
    failures += mylite_test_expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation after show index"
    );
    failures += expect_row_count(database, -1, "row count after show index");
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after show index"
    );

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_show_index_empty_result(
        database,
        (struct expected_show_index_empty_result){
            .sql = "SHOW INDEX FROM no_keys",
            .context = "reopened show index",
        }
    );

    failures += execute_statement_ok(database, "RENAME TABLE no_keys TO renamed_keys");
    failures += execute_error(
        database,
        "SHOW INDEX FROM no_keys",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.no_keys' doesn't exist",
        }
    );
    failures += expect_show_index_empty_result(
        database,
        (struct expected_show_index_empty_result){
            .sql = "SHOW KEYS FROM renamed_keys",
            .context = "renamed show keys",
        }
    );

    failures += execute_statement_ok(database, "ALTER TABLE renamed_keys RENAME final_keys");
    failures += expect_show_index_empty_result(
        database,
        (struct expected_show_index_empty_result){
            .sql = "SHOW INDEXES FROM final_keys",
            .context = "alter renamed show indexes",
        }
    );
    failures += execute_statement_ok(database, "DROP TABLE final_keys");
    failures += execute_error(
        database,
        "SHOW INDEX FROM final_keys",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.final_keys' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_show_index_where_filters(void) {
    static const char *const key_v_row[show_index_column_count] = {
        "indexed",
        "1",
        "k_v",
        "1",
        "v",
        "A",
        "0",
        NULL,
        NULL,
        "YES",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const key_prefix_row[show_index_column_count] = {
        "indexed",
        "1",
        "k_prefix",
        "1",
        "txt",
        "A",
        "0",
        "3",
        NULL,
        "YES",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const fulltext_row[show_index_column_count] = {
        "indexed",
        "1",
        "ft_txt",
        "1",
        "txt",
        NULL,
        "0",
        NULL,
        NULL,
        "YES",
        "FULLTEXT",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const primary_unique_rows[] = {
        "indexed", "0", "PRIMARY", "1",   "id",  "A",       "0", NULL,  NULL,  "",
        "BTREE",   "",  "",        "YES", NULL,  "indexed", "0", "u_n", "1",   "n",
        "A",       "0", NULL,      NULL,  "YES", "BTREE",   "",  "",    "YES", NULL,
    };
    static const char *const id_and_k_rows[] = {
        "indexed", "0",     "PRIMARY", "1",       "id",    "A",        "0",       NULL,    NULL,
        "",        "BTREE", "",        "",        "YES",   NULL,       "indexed", "1",     "k_v",
        "1",       "v",     "A",       "0",       NULL,    NULL,       "YES",     "BTREE", "",
        "",        "YES",   NULL,      "indexed", "1",     "k_prefix", "1",       "txt",   "A",
        "0",       "3",     NULL,      "YES",     "BTREE", "",         "",        "YES",   NULL,
    };
    static const char *const k_rows[] = {
        "indexed", "1", "k_v", "1",   "v",   "A",       "0", NULL,       NULL,  "YES",
        "BTREE",   "",  "",    "YES", NULL,  "indexed", "1", "k_prefix", "1",   "txt",
        "A",       "0", "3",   NULL,  "YES", "BTREE",   "",  "",         "YES", NULL,
    };
    static const char *const all_rows[] = {
        "indexed",  "0", "PRIMARY", "1",   "id",  "A",       "0", NULL,       NULL,  "",
        "BTREE",    "",  "",        "YES", NULL,  "indexed", "0", "u_n",      "1",   "n",
        "A",        "0", NULL,      NULL,  "YES", "BTREE",   "",  "",         "YES", NULL,
        "indexed",  "1", "k_v",     "1",   "v",   "A",       "0", NULL,       NULL,  "YES",
        "BTREE",    "",  "",        "YES", NULL,  "indexed", "1", "k_prefix", "1",   "txt",
        "A",        "0", "3",       NULL,  "YES", "BTREE",   "",  "",         "YES", NULL,
        "indexed",  "1", "ft_txt",  "1",   "txt", NULL,      "0", NULL,       NULL,  "YES",
        "FULLTEXT", "",  "",        "YES", NULL,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "where") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open where file");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE indexed ("
        "id INT NOT NULL,"
        "v VARCHAR(20),"
        "txt TEXT,"
        "n INT,"
        "PRIMARY KEY (id),"
        "KEY k_v (v),"
        "UNIQUE KEY u_n (n),"
        "KEY k_prefix (txt(3)),"
        "FULLTEXT KEY ft_txt (txt)"
        ")"
    );

    failures += expect_show_index_result(
        database,
        (struct expected_show_index_result){
            .sql = "SHOW INDEX FROM indexed WHERE Key_name = 'k_v'",
            .context = "key name filter",
            .values = key_v_row,
            .row_count = 1U,
        }
    );
    failures += expect_show_index_result(
        database,
        (struct expected_show_index_result){
            .sql = "SHOW KEYS FROM indexed WHERE Expression <=> NULL "
                   "AND Key_name IN ('PRIMARY','u_n')",
            .context = "null-safe and in filter",
            .values = primary_unique_rows,
            .row_count = 2U,
        }
    );
    failures += expect_show_index_result(
        database,
        (struct expected_show_index_result){
            .sql = "SHOW INDEX FROM indexed WHERE Key_name LIKE 'k\\_%' "
                   "OR Column_name = 'ID'",
            .context = "like or case-insensitive column filter",
            .values = id_and_k_rows,
            .row_count = 3U,
        }
    );
    failures += expect_show_index_result(
        database,
        (struct expected_show_index_result){
            .sql = "SHOW INDEX FROM indexed WHERE Key_name REGEXP '^k_'",
            .context = "where key name regexp",
            .values = k_rows,
            .row_count = 2U,
        }
    );
    failures += expect_show_index_result(
        database,
        (struct expected_show_index_result){
            .sql = "SHOW INDEX FROM indexed WHERE Key_name RLIKE '^K_'",
            .context = "where key name rlike",
            .values = k_rows,
            .row_count = 2U,
        }
    );
    failures += expect_show_index_result(
        database,
        (struct expected_show_index_result){
            .sql = "SHOW INDEX FROM indexed WHERE Sub_part <=> '3'",
            .context = "prefix sub part filter",
            .values = key_prefix_row,
            .row_count = 1U,
        }
    );
    failures += expect_show_index_result(
        database,
        (struct expected_show_index_result){
            .sql = "SHOW INDEX FROM indexed WHERE `Column_name` IN ('id','v') "
                   "AND Non_unique = '1'",
            .context = "backticked output column and numeric string filter",
            .values = key_v_row,
            .row_count = 1U,
        }
    );
    failures += expect_show_index_result(
        database,
        (struct expected_show_index_result){
            .sql = "SHOW INDEX FROM indexed WHERE NOT (Visible = 'NO') "
                   "AND Index_type = 'FULLTEXT'",
            .context = "not and fulltext filter",
            .values = fulltext_row,
            .row_count = 1U,
        }
    );
    failures += expect_show_index_result(
        database,
        (struct expected_show_index_result){
            .sql = "SHOW INDEX FROM indexed WHERE Cardinality >= '0' AND Seq_in_index = '1'",
            .context = "numeric metadata filter",
            .values = all_rows,
            .row_count = indexed_show_index_row_count,
        }
    );
    failures += expect_show_index_result(
        database,
        (struct expected_show_index_result){
            .sql = "SHOW INDEX FROM indexed WHERE Non_unique = 0",
            .context = "numeric metadata integer literal filter",
            .values = primary_unique_rows,
            .row_count = 2U,
        }
    );
    failures += expect_show_index_result(
        database,
        (struct expected_show_index_result){
            .sql = "SHOW INDEX FROM indexed WHERE Sub_part REGEXP '^3$'",
            .context = "where numeric metadata regexp",
            .values = key_prefix_row,
            .row_count = 1U,
        }
    );
    failures += expect_show_index_empty_result(
        database,
        (struct expected_show_index_empty_result){
            .sql = "SHOW INDEX FROM indexed WHERE Packed REGEXP '.*'",
            .context = "where regexp skips null packed cells",
        }
    );
    failures += expect_show_index_empty_result(
        database,
        (struct expected_show_index_empty_result){
            .sql = "SHOW INDEX FROM indexed WHERE Key_name = 'missing'",
            .context = "empty filtered index result",
        }
    );
    failures += expect_row_count(database, -1, "row count after show index where");

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen where file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_show_index_result(
        database,
        (struct expected_show_index_result){
            .sql = "SHOW INDEX FROM indexed WHERE Sub_part IN (NULL, 3)",
            .context = "reopened prefix filter",
            .values = key_prefix_row,
            .row_count = 1U,
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_show_index_diagnostics_and_unsupported_forms(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += create_show_index_schema(database);

    failures += execute_error(
        database,
        "SHOW INDEX FROM no_keys",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "SHOW INDEX FROM missing_schema.no_keys",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "SHOW INDEX FROM no_keys FROM missing_schema",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "SHOW INDEX FROM _mylite_catalog.no_keys",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_catalog'",
        }
    );

    failures += execute_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "SHOW INDEX FROM missing_table",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SHOW INDEX FROM no_keys FROM other",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'other.no_keys' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SHOW INDEX FROM _mylite_numbers",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_numbers'",
        }
    );

    failures += expect_show_index_empty_result(
        database,
        (struct expected_show_index_empty_result){
            .sql = "SHOW EXTENDED INDEX FROM no_keys",
            .context = "show extended index no-index table",
        }
    );
    failures += expect_show_index_empty_result(
        database,
        (struct expected_show_index_empty_result){
            .sql = "SHOW INDEX FROM no_keys WHERE Key_name = 'idx'",
            .context = "filtered no-index table",
        }
    );
    failures += execute_error(
        database,
        "SHOW INDEX FROM no_keys WHERE missing = 'x'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SHOW INDEX FROM no_keys WHERE indexes.Key_name = 'idx'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'indexes.Key_name' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SHOW INDEX FROM no_keys WHERE Key_name = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW INDEX WHERE supports only string literal predicates",
        }
    );
    failures += execute_error(
        database,
        "SHOW INDEX FROM no_keys WHERE Expression = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW INDEX WHERE supports only string literal predicates",
        }
    );
    failures += execute_error(
        database,
        "SHOW INDEX FROM no_keys WHERE Expression IN (1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW INDEX WHERE supports only string literal predicates",
        }
    );
    failures += execute_error(
        database,
        "SHOW INDEX FROM no_keys WHERE Key_name REGEXP '[z-a]'",
        (struct expected_sql_error){
            .code = mysql_error_regexp_range,
            .sqlstate = "HY000",
            .message_part = "invalid character range",
        }
    );
    failures += execute_error(
        database,
        "SHOW INDEX FROM no_keys LIKE 'idx'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW INDEX FROM no_keys WHERE Key_name = 'idx' ORDER BY Key_name",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_show_index_handles(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (mylite_test_make_path(first_path, sizeof(first_path), "first") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures +=
        mylite_test_expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first database");
    failures += mylite_test_expect_int(
        mylite_open(second_path, &second),
        MYLITE_OK,
        "open second database"
    );
    failures += execute_statement_ok(first, "CREATE DATABASE app");
    failures += execute_statement_ok(first, "CREATE TABLE app.left_table (id INT NULL)");
    failures += execute_statement_ok(first, "USE app");
    failures += execute_statement_ok(second, "CREATE DATABASE other");
    failures += execute_statement_ok(second, "CREATE TABLE other.right_table (id BIGINT NULL)");
    failures += execute_statement_ok(second, "USE other");

    failures += expect_show_index_empty_result(
        first,
        (struct expected_show_index_empty_result){
            .sql = "SHOW INDEX FROM left_table",
            .context = "first table",
        }
    );
    failures += expect_show_index_empty_result(
        second,
        (struct expected_show_index_empty_result){
            .sql = "SHOW INDEX FROM right_table",
            .context = "second table",
        }
    );
    failures += execute_error(
        first,
        "SHOW INDEX FROM right_table",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.right_table' doesn't exist",
        }
    );
    failures += execute_error(
        second,
        "SHOW INDEX FROM left_table",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'other.left_table' doesn't exist",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int create_show_index_schema(mylite_db *database) {
    int failures = 0;

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "CREATE DATABASE other");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE app.no_keys (id INT NOT NULL, value BIGINT NULL)"
    );
    failures +=
        execute_statement_ok(database, "CREATE TABLE other.only_other (other_id BIGINT NULL)");
    return failures;
}

static int expect_show_index_empty_result(
    mylite_db *database,
    struct expected_show_index_empty_result expectation
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, expectation.sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        show_index_column_count,
        "show index column count"
    );
    for (size_t column_index = 0U; column_index < show_index_column_count; ++column_index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column_index),
            show_index_names[column_index],
            expectation.context
        );
    }
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, expectation.context);
    failures +=
        mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expectation.context);
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), 0U, expectation.context);

    mylite_result_free(result);
    return failures;
}

static int expect_show_index_result(
    mylite_db *database,
    struct expected_show_index_result expectation
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, expectation.sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        show_index_column_count,
        "show index column count"
    );
    for (size_t column_index = 0U; column_index < show_index_column_count; ++column_index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column_index),
            show_index_names[column_index],
            expectation.context
        );
    }
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expectation.row_count,
        expectation.context
    );
    failures +=
        mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expectation.context);
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), 0U, expectation.context);

    for (size_t row_index = 0U; row_index < expectation.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < show_index_column_count; ++column_index) {
            size_t value_index = (row_index * show_index_column_count) + column_index;

            failures += mylite_test_expect_text_or_null(
                mylite_result_value_text(result, row_index, column_index),
                expectation.values[value_index],
                expectation.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_row_count(mylite_db *database, int64_t expected, const char *context) {
    char expected_text[row_count_text_capacity];
    mylite_result *result = NULL;
    int written = snprintf(expected_text, sizeof(expected_text), "%" PRId64, expected);
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(expected_text)) {
        fprintf(stderr, "failed to format row count expectation for %s\n", context);
        return 1;
    }

    failures += execute_ok(database, "SELECT ROW_COUNT()", &result);
    failures += mylite_test_expect_size(mylite_result_column_count(result), 1U, context);
    failures += mylite_test_expect_text_or_null(
        mylite_result_column_name(result, 0U),
        "ROW_COUNT()",
        context
    );
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, 0U, 0U),
        expected_text,
        context
    );
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, context);
    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result != NULL) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected success for %s, got rc=%d code=%d state=%s message=%s\n",
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
        fprintf(stderr, "expected error for %s, got rc=%d\n", sql, rc);
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
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return;
    }
    remove(buffer);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(file);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    fclose(file);
    if (read_count != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
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
    if (memcmp(actual, expected, size) == 0) {
        return 0;
    }
    fprintf(stderr, "%s: bytes differ\n", context);
    return 1;
}
