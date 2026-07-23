#include "mylite_test_support.h"

#include <mylite/mylite.h>

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
    trigger_row_count = 2,
    information_schema_triggers_column_count = 18,
    show_triggers_column_count = 11,
    datetime2_text_length = 22,
    datetime_year_month_separator = 4,
    datetime_month_day_separator = 7,
    datetime_date_time_separator = 10,
    datetime_hour_minute_separator = 13,
    datetime_minute_second_separator = 16,
    datetime_fraction_separator = 19,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static const char expected_timestamp2_value[] = "<timestamp2>";
static const char action_statement[] =
    "BEGIN\n"
    "    IF @sys.ignore_sys_config_triggers != true AND NEW.set_by IS NULL THEN\n"
    "        SET NEW.set_by = USER();\n"
    "    END IF;\n"
    "END";
static const char trigger_sql_mode[] =
    "ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,"
    "ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION";

static int test_sys_sys_config_triggers(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_timestamp2_text(const char *actual, const char *context);

static const char
    *const information_schema_triggers_columns[information_schema_triggers_column_count] = {
        "TRIGGER_NAME",
        "EVENT_MANIPULATION",
        "EVENT_OBJECT_TABLE",
        "ACTION_ORDER",
        "ACTION_CONDITION",
        "ACTION_STATEMENT",
        "ACTION_ORIENTATION",
        "ACTION_TIMING",
        "ACTION_REFERENCE_OLD_TABLE",
        "ACTION_REFERENCE_NEW_TABLE",
        "ACTION_REFERENCE_OLD_ROW",
        "ACTION_REFERENCE_NEW_ROW",
        "CREATED",
        "SQL_MODE",
        "DEFINER",
        "CHARACTER_SET_CLIENT",
        "COLLATION_CONNECTION",
        "DATABASE_COLLATION",
};

static const char *const show_triggers_columns[show_triggers_column_count] = {
    "Trigger",
    "Event",
    "Table",
    "Statement",
    "Timing",
    "Created",
    "sql_mode",
    "Definer",
    "character_set_client",
    "collation_connection",
    "Database Collation",
};

int main(void) {
    return test_sys_sys_config_triggers() == 0 ? 0 : 1;
}

static int test_sys_sys_config_triggers(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const count_one[] = {"1"};
    static const char *const count_two[] = {"2"};
    static const char *const information_schema_triggers_values[] = {
        "sys_config_insert_set_user",
        "INSERT",
        "sys_config",
        "1",
        NULL,
        action_statement,
        "ROW",
        "BEFORE",
        NULL,
        NULL,
        "OLD",
        "NEW",
        expected_timestamp2_value,
        trigger_sql_mode,
        "mysql.sys@localhost",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb4_0900_ai_ci",
        "sys_config_update_set_user",
        "UPDATE",
        "sys_config",
        "1",
        NULL,
        action_statement,
        "ROW",
        "BEFORE",
        NULL,
        NULL,
        "OLD",
        "NEW",
        expected_timestamp2_value,
        trigger_sql_mode,
        "mysql.sys@localhost",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const show_triggers_values[] = {
        "sys_config_insert_set_user",
        "INSERT",
        "sys_config",
        action_statement,
        "BEFORE",
        expected_timestamp2_value,
        trigger_sql_mode,
        "mysql.sys@localhost",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb4_0900_ai_ci",
        "sys_config_update_set_user",
        "UPDATE",
        "sys_config",
        action_statement,
        "BEFORE",
        expected_timestamp2_value,
        trigger_sql_mode,
        "mysql.sys@localhost",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb4_0900_ai_ci",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "sys-sys-config-triggers") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TRIGGERS "
                   "WHERE TRIGGER_SCHEMA = 'sys'",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_two,
            .row_count = 1U,
            .context = "sys trigger count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TRIGGER_NAME, EVENT_MANIPULATION, EVENT_OBJECT_TABLE, "
                   "ACTION_ORDER, ACTION_CONDITION, ACTION_STATEMENT, "
                   "ACTION_ORIENTATION, ACTION_TIMING, ACTION_REFERENCE_OLD_TABLE, "
                   "ACTION_REFERENCE_NEW_TABLE, ACTION_REFERENCE_OLD_ROW, "
                   "ACTION_REFERENCE_NEW_ROW, CREATED, SQL_MODE, DEFINER, "
                   "CHARACTER_SET_CLIENT, COLLATION_CONNECTION, DATABASE_COLLATION "
                   "FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA = 'sys' "
                   "AND EVENT_OBJECT_TABLE = 'sys_config' ORDER BY TRIGGER_NAME",
            .column_names = information_schema_triggers_columns,
            .column_count = information_schema_triggers_column_count,
            .values = information_schema_triggers_values,
            .row_count = trigger_row_count,
            .context = "sys trigger information_schema rows",
        }
    );
    failures += expect_row_count_status(database, "row count after trigger metadata read");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TRIGGERS "
                   "WHERE TRIGGER_SCHEMA = 'sys' "
                   "AND TRIGGER_NAME LIKE 'sys_config_insert%'",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_one,
            .row_count = 1U,
            .context = "sys trigger name LIKE count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TRIGGERS FROM sys LIKE 'sys_config'",
            .column_names = show_triggers_columns,
            .column_count = show_triggers_column_count,
            .values = show_triggers_values,
            .row_count = trigger_row_count,
            .context = "sys SHOW TRIGGERS rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL TRIGGERS FROM sys LIKE 'sys_config'",
            .column_names = show_triggers_columns,
            .column_count = show_triggers_column_count,
            .values = show_triggers_values,
            .row_count = trigger_row_count,
            .context = "sys SHOW FULL TRIGGERS rows",
        }
    );
    failures += expect_statement_ok(database, "USE sys");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TRIGGERS LIKE 'sys_config'",
            .column_names = show_triggers_columns,
            .column_count = show_triggers_column_count,
            .values = show_triggers_values,
            .row_count = trigger_row_count,
            .context = "sys selected-schema SHOW TRIGGERS rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TRIGGERS LIKE 'sys_config_insert%'",
            .column_names = show_triggers_columns,
            .column_count = show_triggers_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys SHOW TRIGGERS LIKE trigger name is empty",
        }
    );
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TRIGGERS",
            .column_names = show_triggers_columns,
            .column_count = show_triggers_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "app SHOW TRIGGERS remains empty",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TRIGGERS "
                   "WHERE TRIGGER_SCHEMA = 'app'",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "app trigger count remains empty",
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

                if (expected_value == expected_timestamp2_value) {
                    failures += expect_timestamp2_text(actual_value, expected.context);
                } else {
                    failures += mylite_test_expect_text_or_null(
                        actual_value,
                        expected_value,
                        expected.context
                    );
                }
            }
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_row_count_status(mylite_db *database, const char *context) {
    static const char *const column_names[] = {"ROW_COUNT()"};
    static const char *const values[] = {"-1"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .column_names = column_names,
            .column_count = sizeof(column_names) / sizeof(column_names[0]),
            .values = values,
            .row_count = 1U,
            .context = context,
        }
    );
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

static int expect_timestamp2_text(const char *actual, const char *context) {
    if (actual == NULL) {
        fprintf(stderr, "%s: expected timestamp(2) text, got NULL\n", context);
        return 1;
    }
    if (strlen(actual) != datetime2_text_length) {
        fprintf(
            stderr,
            "%s: expected timestamp(2) length %d, got [%s]\n",
            context,
            datetime2_text_length,
            actual
        );
        return 1;
    }
    for (size_t index = 0U; index < datetime2_text_length; ++index) {
        bool is_separator =
            index == datetime_year_month_separator || index == datetime_month_day_separator ||
            index == datetime_date_time_separator || index == datetime_hour_minute_separator ||
            index == datetime_minute_second_separator || index == datetime_fraction_separator;
        char expected_separator = '\0';

        if (!is_separator) {
            if (actual[index] < '0' || actual[index] > '9') {
                fprintf(stderr, "%s: expected timestamp(2) digit, got [%s]\n", context, actual);
                return 1;
            }
            continue;
        }

        if (index == datetime_date_time_separator) {
            expected_separator = ' ';
        } else if (index == datetime_fraction_separator) {
            expected_separator = '.';
        } else if (index < datetime_date_time_separator) {
            expected_separator = '-';
        } else {
            expected_separator = ':';
        }
        if (actual[index] != expected_separator) {
            fprintf(stderr, "%s: expected timestamp(2) separator, got [%s]\n", context, actual);
            return 1;
        }
    }
    return 0;
}
