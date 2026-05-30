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
    sql_buffer_capacity = 160,
    message_buffer_capacity = 160,
    mysql_error_data_dictionary_access = 3554,
    show_full_tables_column_count = 2,
    show_table_status_column_count = 18,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

struct hidden_table_expectation {
    const char *table_name;
    const char *table_kind;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_mysql_data_dictionary_table_diagnostics(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *text, const char *needle, const char *context);

int main(void) {
    return test_mysql_data_dictionary_table_diagnostics() == 0 ? 0 : 1;
}

static int test_mysql_data_dictionary_table_diagnostics(void) {
    static const struct hidden_table_expectation hidden_tables[] = {
        {"catalogs", "data dictionary"},
        {"character_sets", "data dictionary"},
        {"check_constraints", "data dictionary"},
        {"collations", "data dictionary"},
        {"column_statistics", "data dictionary"},
        {"column_type_elements", "data dictionary"},
        {"columns", "data dictionary"},
        {"dd_properties", "data dictionary"},
        {"events", "data dictionary"},
        {"foreign_keys", "data dictionary"},
        {"foreign_key_column_usage", "data dictionary"},
        {"index_column_usage", "data dictionary"},
        {"index_partitions", "data dictionary"},
        {"index_stats", "data dictionary"},
        {"indexes", "data dictionary"},
        {"innodb_ddl_log", "system"},
        {"innodb_dynamic_metadata", "system"},
        {"parameter_type_elements", "data dictionary"},
        {"parameters", "data dictionary"},
        {"resource_groups", "data dictionary"},
        {"routines", "data dictionary"},
        {"schemata", "data dictionary"},
        {"st_spatial_reference_systems", "data dictionary"},
        {"table_partition_values", "data dictionary"},
        {"table_partitions", "data dictionary"},
        {"table_stats", "data dictionary"},
        {"tables", "data dictionary"},
        {"tablespace_files", "data dictionary"},
        {"tablespaces", "data dictionary"},
        {"triggers", "data dictionary"},
        {"view_routine_usage", "data dictionary"},
        {"view_table_usage", "data dictionary"},
    };
    static const char *const count_columns[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const show_full_tables_columns[show_full_tables_column_count] = {
        "Tables_in_mysql (schemata)",
        "Table_type",
    };
    static const char *const show_table_status_columns[show_table_status_column_count] = {
        "Name",
        "Engine",
        "Version",
        "Row_format",
        "Rows",
        "Avg_row_length",
        "Data_length",
        "Max_data_length",
        "Index_length",
        "Data_free",
        "Auto_increment",
        "Create_time",
        "Update_time",
        "Check_time",
        "Collation",
        "Checksum",
        "Create_options",
        "Comment",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "mysql-data-dictionary-diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");

    for (size_t table_index = 0U; table_index < sizeof(hidden_tables) / sizeof(hidden_tables[0]);
         ++table_index) {
        char sql[sql_buffer_capacity];
        char message[message_buffer_capacity];
        const struct hidden_table_expectation *table = &hidden_tables[table_index];

        (void)snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM mysql.%s", table->table_name);
        (void)snprintf(
            message,
            sizeof(message),
            "Access to %s table 'mysql.%s' is rejected.",
            table->table_kind,
            table->table_name
        );
        failures += expect_error(
            database,
            sql,
            (struct expected_sql_error){
                .code = mysql_error_data_dictionary_access,
                .sqlstate = "HY000",
                .message_part = message,
            }
        );
    }

    failures += expect_statement_ok(database, "USE mysql");
    failures += expect_error(
        database,
        "SELECT COUNT(*) FROM innodb_dynamic_metadata",
        (struct expected_sql_error){
            .code = mysql_error_data_dictionary_access,
            .sqlstate = "HY000",
            .message_part = "Access to system table 'mysql.innodb_dynamic_metadata' is rejected.",
        }
    );
    failures += expect_error(
        database,
        "SELECT COUNT(*) FROM schemata",
        (struct expected_sql_error){
            .code = mysql_error_data_dictionary_access,
            .sqlstate = "HY000",
            .message_part = "Access to data dictionary table 'mysql.schemata' is rejected.",
        }
    );
    failures += expect_statement_ok(database, "USE app");
    failures += expect_error(
        database,
        "SHOW COLUMNS FROM mysql.catalogs",
        (struct expected_sql_error){
            .code = mysql_error_data_dictionary_access,
            .sqlstate = "HY000",
            .message_part = "Access to data dictionary table 'mysql.catalogs' is rejected.",
        }
    );
    failures += expect_error(
        database,
        "DESC mysql.schemata",
        (struct expected_sql_error){
            .code = mysql_error_data_dictionary_access,
            .sqlstate = "HY000",
            .message_part = "Access to data dictionary table 'mysql.schemata' is rejected.",
        }
    );
    failures += expect_error(
        database,
        "SHOW INDEX FROM mysql.tables",
        (struct expected_sql_error){
            .code = mysql_error_data_dictionary_access,
            .sqlstate = "HY000",
            .message_part = "Access to data dictionary table 'mysql.tables' is rejected.",
        }
    );
    failures += expect_error(
        database,
        "SHOW COLUMNS FROM mysql.innodb_ddl_log",
        (struct expected_sql_error){
            .code = mysql_error_data_dictionary_access,
            .sqlstate = "HY000",
            .message_part = "Access to system table 'mysql.innodb_ddl_log' is rejected.",
        }
    );
    failures += expect_error(
        database,
        "SHOW INDEX FROM mysql.innodb_dynamic_metadata",
        (struct expected_sql_error){
            .code = mysql_error_data_dictionary_access,
            .sqlstate = "HY000",
            .message_part = "Access to system table 'mysql.innodb_dynamic_metadata' is rejected.",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'mysql' "
                   "AND TABLE_NAME IN ('catalogs','schemata','tables','innodb_ddl_log',"
                   "'innodb_dynamic_metadata')",
            .column_names = count_columns,
            .column_count = sizeof(count_columns) / sizeof(count_columns[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "hidden dictionary tables absent from information_schema.tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL TABLES FROM mysql LIKE 'schemata'",
            .column_names = show_full_tables_columns,
            .column_count = show_full_tables_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "hidden dictionary tables absent from show full tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM mysql LIKE 'innodb_dynamic_metadata'",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "mysql.innodb_dynamic_metadata absent from show table status",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
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

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
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

static int expect_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);

    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(path, path_size, "/tmp/mylite-%s-%d.mylite", name, current_process_id());

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path buffer too small\n");
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
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written > 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
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
            "%s: expected %s, got %s\n",
            context,
            expected == NULL ? "<null>" : expected,
            actual == NULL ? "<null>" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *text, const char *needle, const char *context) {
    if (text == NULL || needle == NULL || strstr(text, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            text == NULL ? "<null>" : text,
            needle == NULL ? "<null>" : needle
        );
        return 1;
    }
    return 0;
}
