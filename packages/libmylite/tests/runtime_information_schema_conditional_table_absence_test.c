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
    sql_buffer_capacity = 512,
    message_buffer_capacity = 128,
    mysql_error_unknown_table_in_schema = 1109,
    show_full_tables_column_count = 2,
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
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_information_schema_conditional_table_absence(void);
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
    return test_information_schema_conditional_table_absence() == 0 ? 0 : 1;
}

static int test_information_schema_conditional_table_absence(void) {
    static const char *const conditional_tables[] = {
        "MYSQL_FIREWALL_USERS",
        "MYSQL_FIREWALL_WHITELIST",
        "ndb_transid_mysql_connection_map",
        "TP_THREAD_GROUP_STATE",
        "TP_THREAD_GROUP_STATS",
        "TP_THREAD_STATE",
    };
    static const char *const count_columns[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const show_full_tables_columns[show_full_tables_column_count] = {
        "Tables_in_information_schema",
        "Table_type",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "information-schema-conditional-absence") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");

    for (size_t table_index = 0U;
         table_index < sizeof(conditional_tables) / sizeof(conditional_tables[0]);
         ++table_index) {
        char sql[sql_buffer_capacity];
        char message[message_buffer_capacity];
        const char *table_name = conditional_tables[table_index];

        (void)snprintf(
            message,
            sizeof(message),
            "Unknown table '%s' in information_schema",
            table_name
        );
        (void)snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM INFORMATION_SCHEMA.%s", table_name);
        failures += expect_error(
            database,
            sql,
            (struct expected_sql_error){
                .code = mysql_error_unknown_table_in_schema,
                .sqlstate = "42S02",
                .message_part = message,
            }
        );
        (void)snprintf(sql, sizeof(sql), "SHOW COLUMNS FROM INFORMATION_SCHEMA.%s", table_name);
        failures += expect_error(
            database,
            sql,
            (struct expected_sql_error){
                .code = mysql_error_unknown_table_in_schema,
                .sqlstate = "42S02",
                .message_part = message,
            }
        );
    }

    failures += expect_statement_ok(database, "USE information_schema");
    failures += expect_error(
        database,
        "SELECT COUNT(*) FROM MYSQL_FIREWALL_USERS",
        (struct expected_sql_error){
            .code = mysql_error_unknown_table_in_schema,
            .sqlstate = "42S02",
            .message_part = "Unknown table 'MYSQL_FIREWALL_USERS' in information_schema",
        }
    );
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME IN "
                   "('MYSQL_FIREWALL_USERS','MYSQL_FIREWALL_WHITELIST',"
                   "'ndb_transid_mysql_connection_map','TP_THREAD_GROUP_STATE',"
                   "'TP_THREAD_GROUP_STATS','TP_THREAD_STATE')",
            .column_names = count_columns,
            .column_count = sizeof(count_columns) / sizeof(count_columns[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "conditional tables absent from information_schema.tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME IN "
                   "('MYSQL_FIREWALL_USERS','MYSQL_FIREWALL_WHITELIST',"
                   "'ndb_transid_mysql_connection_map','TP_THREAD_GROUP_STATE',"
                   "'TP_THREAD_GROUP_STATS','TP_THREAD_STATE')",
            .column_names = count_columns,
            .column_count = sizeof(count_columns) / sizeof(count_columns[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "conditional tables absent from information_schema.columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL TABLES FROM information_schema "
                   "WHERE Tables_in_information_schema IN "
                   "('MYSQL_FIREWALL_USERS','MYSQL_FIREWALL_WHITELIST',"
                   "'ndb_transid_mysql_connection_map','TP_THREAD_GROUP_STATE',"
                   "'TP_THREAD_GROUP_STATS','TP_THREAD_STATE')",
            .column_names = show_full_tables_columns,
            .column_count = show_full_tables_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "conditional tables absent from show full tables",
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
