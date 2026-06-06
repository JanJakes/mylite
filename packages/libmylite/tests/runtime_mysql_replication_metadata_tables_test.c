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
    slave_worker_info_column_count = 13,
    show_columns_column_count = 6,
    show_full_columns_column_count = 9,
    show_index_column_count = 15,
    information_schema_column_probe_count = 11,
    information_schema_key_column_usage_column_count = 7,
    information_schema_statistics_column_count = 13,
    information_schema_tables_column_count = 19,
    show_table_status_column_count = 18,
    datetime_text_length = 19,
    datetime_year_month_separator = 4,
    datetime_month_day_separator = 7,
    datetime_date_time_separator = 10,
    datetime_hour_minute_separator = 13,
    datetime_minute_second_separator = 16,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static const char expected_datetime_value[] = "<datetime>";

static int test_mysql_replication_metadata_tables(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
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
static int expect_datetime_text(const char *actual, const char *context);

static const char *const count_column[] = {"COUNT(*)"};
static const char *const count_zero[] = {"0"};

static const char *const slave_worker_info_columns[slave_worker_info_column_count] = {
    "Id",
    "Relay_log_name",
    "Relay_log_pos",
    "Master_log_name",
    "Master_log_pos",
    "Checkpoint_relay_log_name",
    "Checkpoint_relay_log_pos",
    "Checkpoint_master_log_name",
    "Checkpoint_master_log_pos",
    "Checkpoint_seqno",
    "Checkpoint_group_size",
    "Checkpoint_group_bitmap",
    "Channel_name",
};

static const char *const show_columns_columns[show_columns_column_count] = {
    "Field",
    "Type",
    "Null",
    "Key",
    "Default",
    "Extra",
};

static const char *const show_full_columns_columns[show_full_columns_column_count] = {
    "Field",
    "Type",
    "Collation",
    "Null",
    "Key",
    "Default",
    "Extra",
    "Privileges",
    "Comment",
};

static const char *const show_index_columns[show_index_column_count] = {
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

static const char
    *const information_schema_column_probe_columns[information_schema_column_probe_count] = {
        "COLUMN_NAME",
        "COLUMN_DEFAULT",
        "IS_NULLABLE",
        "DATA_TYPE",
        "CHARACTER_MAXIMUM_LENGTH",
        "CHARACTER_OCTET_LENGTH",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
        "COLUMN_TYPE",
        "COLUMN_KEY",
        "COLUMN_COMMENT",
};

static const char *const
    information_schema_key_column_usage_columns[information_schema_key_column_usage_column_count] =
        {
            "CONSTRAINT_NAME",
            "COLUMN_NAME",
            "ORDINAL_POSITION",
            "POSITION_IN_UNIQUE_CONSTRAINT",
            "REFERENCED_TABLE_SCHEMA",
            "REFERENCED_TABLE_NAME",
            "REFERENCED_COLUMN_NAME",
};

static const char
    *const information_schema_statistics_columns[information_schema_statistics_column_count] = {
        "INDEX_NAME",
        "SEQ_IN_INDEX",
        "COLUMN_NAME",
        "COLLATION",
        "CARDINALITY",
        "SUB_PART",
        "PACKED",
        "NULLABLE",
        "INDEX_TYPE",
        "COMMENT",
        "INDEX_COMMENT",
        "IS_VISIBLE",
        "EXPRESSION",
};

static const char *const information_schema_tables_columns[information_schema_tables_column_count] =
    {
        "TABLE_NAME",      "TABLE_TYPE",     "ENGINE",         "VERSION",         "ROW_FORMAT",
        "TABLE_ROWS",      "AVG_ROW_LENGTH", "DATA_LENGTH",    "MAX_DATA_LENGTH", "INDEX_LENGTH",
        "DATA_FREE",       "AUTO_INCREMENT", "CREATE_TIME",    "UPDATE_TIME",     "CHECK_TIME",
        "TABLE_COLLATION", "CHECKSUM",       "CREATE_OPTIONS", "TABLE_COMMENT",
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

int main(void) {
    return test_mysql_replication_metadata_tables() == 0 ? 0 : 1;
}

static int test_mysql_replication_metadata_tables(void) {
    static const char *const show_full_master_channel_values[] = {
        "Channel_name",
        "varchar(64)",
        "utf8mb3_general_ci",
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        ("The channel on which the replica is connected to a source. Used in Multisource "
         "Replication"),
    };
    static const char *const show_full_master_failover_values[] = {
        "Source_connection_auto_failover",
        "tinyint(1)",
        NULL,
        "NO",
        "",
        "0",
        "",
        "select,insert,update,references",
        "Indicates whether the channel connection failover is enabled.",
    };
    static const char *const show_full_relay_policy_values[] = {
        "Require_table_primary_key_check",
        "enum('STREAM','ON','OFF','GENERATE')",
        "utf8mb3_general_ci",
        "NO",
        "",
        "STREAM",
        "",
        "select,insert,update,references",
        ("Indicates what is the channel policy regarding tables without primary keys on create and "
         "alter table queries"),
    };
    static const char *const describe_worker_values[] = {
        "Id",
        "int unsigned",
        "NO",
        "PRI",
        NULL,
        "",
        "Relay_log_name",
        "text",
        "NO",
        "",
        NULL,
        "",
        "Relay_log_pos",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "Master_log_name",
        "text",
        "NO",
        "",
        NULL,
        "",
        "Master_log_pos",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "Checkpoint_relay_log_name",
        "text",
        "NO",
        "",
        NULL,
        "",
        "Checkpoint_relay_log_pos",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "Checkpoint_master_log_name",
        "text",
        "NO",
        "",
        NULL,
        "",
        "Checkpoint_master_log_pos",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "Checkpoint_seqno",
        "int unsigned",
        "NO",
        "",
        NULL,
        "",
        "Checkpoint_group_size",
        "int unsigned",
        "NO",
        "",
        NULL,
        "",
        "Checkpoint_group_bitmap",
        "blob",
        "NO",
        "",
        NULL,
        "",
        "Channel_name",
        "varchar(64)",
        "NO",
        "PRI",
        NULL,
        "",
    };
    static const char *const worker_index_values[] = {
        "slave_worker_info",
        "0",
        "PRIMARY",
        "1",
        "Channel_name",
        "A",
        "0",
        NULL,
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
        "slave_worker_info",
        "0",
        "PRIMARY",
        "2",
        "Id",
        "A",
        "0",
        NULL,
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const worker_key_column_usage_values[] = {
        "PRIMARY",
        "Channel_name",
        "1",
        NULL,
        NULL,
        NULL,
        NULL,
        "PRIMARY",
        "Id",
        "2",
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const worker_statistics_values[] = {
        "PRIMARY", "1", "Channel_name", "A", "0", NULL, NULL, "", "BTREE", "", "", "YES", NULL,
        "PRIMARY", "2", "Id",           "A", "0", NULL, NULL, "", "BTREE", "", "", "YES", NULL,
    };
    static const char *const master_compression_column_values[] = {
        "Master_compression_algorithm",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "",
        "Compression algorithm supported for data transfer between source and replica.",
    };
    static const char *const relay_policy_column_values[] = {
        "Require_table_primary_key_check",
        "STREAM",
        "NO",
        "enum",
        "8",
        "24",
        "utf8mb3",
        "utf8mb3_general_ci",
        "enum('STREAM','ON','OFF','GENERATE')",
        "",
        ("Indicates what is the channel policy regarding tables without primary keys on create and "
         "alter table queries"),
    };
    static const char *const worker_blob_column_values[] = {
        "Checkpoint_group_bitmap",
        NULL,
        "NO",
        "blob",
        "65535",
        "65535",
        NULL,
        NULL,
        "blob",
        "",
        "",
    };
    static const char *const table_status_values[] = {
        "slave_worker_info",
        "InnoDB",
        "10",
        "Dynamic",
        "0",
        "0",
        "16384",
        "0",
        "0",
        "4194304",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "Worker Information",
    };
    static const char *const information_schema_tables_values[] = {
        "slave_master_info",
        "BASE TABLE",
        "InnoDB",
        "10",
        "Dynamic",
        "0",
        "0",
        "16384",
        "0",
        "0",
        "4194304",
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        "utf8mb3_general_ci",
        NULL,
        "row_format=DYNAMIC stats_persistent=0",
        "Master Information",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;
    int rc = make_test_path(path, sizeof(path), "main");

    if (rc != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    if (failures != 0 || database == NULL) {
        if (database == NULL) {
            fprintf(stderr, "mylite_open did not return a database\n");
        }
        remove_related_files(path);
        return 1;
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM mysql.slave_master_info",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "mysql.slave_master_info count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM mysql.slave_relay_log_info",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "mysql.slave_relay_log_info count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM mysql.slave_worker_info",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "mysql.slave_worker_info count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM mysql.slave_worker_info",
            .column_names = slave_worker_info_columns,
            .column_count = slave_worker_info_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "mysql.slave_worker_info empty read",
        }
    );
    failures += expect_statement_ok(database, "USE mysql");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM slave_relay_log_info",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "selected mysql slave_relay_log_info count",
        }
    );
    failures += expect_row_count_status(database, "mysql replication metadata row_count");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM mysql.slave_master_info WHERE Field = 'Channel_name'",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = show_full_master_channel_values,
            .row_count = 1U,
            .context = "mysql.slave_master_info channel metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM mysql.slave_master_info "
                   "WHERE Field = 'Source_connection_auto_failover'",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = show_full_master_failover_values,
            .row_count = 1U,
            .context = "mysql.slave_master_info failover metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM mysql.slave_relay_log_info "
                   "WHERE Field = 'Require_table_primary_key_check'",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = show_full_relay_policy_values,
            .row_count = 1U,
            .context = "mysql.slave_relay_log_info enum metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE mysql.slave_worker_info",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = describe_worker_values,
            .row_count = slave_worker_info_column_count,
            .context = "mysql.slave_worker_info describe",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM mysql.slave_worker_info WHERE Key_name = 'PRIMARY'",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = worker_index_values,
            .row_count = 2U,
            .context = "mysql.slave_worker_info show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE, "
                   "CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, CHARACTER_SET_NAME, "
                   "COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, COLUMN_COMMENT "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'slave_master_info' "
                   "AND COLUMN_NAME = 'Master_compression_algorithm'",
            .column_names = information_schema_column_probe_columns,
            .column_count = information_schema_column_probe_count,
            .values = master_compression_column_values,
            .row_count = 1U,
            .context = "mysql.slave_master_info information_schema.columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE, "
                   "CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, CHARACTER_SET_NAME, "
                   "COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, COLUMN_COMMENT "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'slave_relay_log_info' "
                   "AND COLUMN_NAME = 'Require_table_primary_key_check'",
            .column_names = information_schema_column_probe_columns,
            .column_count = information_schema_column_probe_count,
            .values = relay_policy_column_values,
            .row_count = 1U,
            .context = "mysql.slave_relay_log_info information_schema.columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE, "
                   "CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, CHARACTER_SET_NAME, "
                   "COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, COLUMN_COMMENT "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'slave_worker_info' "
                   "AND COLUMN_NAME = 'Checkpoint_group_bitmap'",
            .column_names = information_schema_column_probe_columns,
            .column_count = information_schema_column_probe_count,
            .values = worker_blob_column_values,
            .row_count = 1U,
            .context = "mysql.slave_worker_info information_schema.columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION, "
                   "POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA, "
                   "REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME "
                   "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'slave_worker_info'",
            .column_names = information_schema_key_column_usage_columns,
            .column_count = information_schema_key_column_usage_column_count,
            .values = worker_key_column_usage_values,
            .row_count = 2U,
            .context = "mysql.slave_worker_info information_schema.key_column_usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, CARDINALITY, "
                   "SUB_PART, PACKED, NULLABLE, INDEX_TYPE, COMMENT, INDEX_COMMENT, "
                   "IS_VISIBLE, EXPRESSION FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'slave_worker_info'",
            .column_names = information_schema_statistics_columns,
            .column_count = information_schema_statistics_column_count,
            .values = worker_statistics_values,
            .row_count = 2U,
            .context = "mysql.slave_worker_info information_schema.statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE, "
                   "AUTO_INCREMENT, CREATE_TIME, UPDATE_TIME, CHECK_TIME, TABLE_COLLATION, "
                   "CHECKSUM, CREATE_OPTIONS, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'slave_master_info'",
            .column_names = information_schema_tables_columns,
            .column_count = information_schema_tables_column_count,
            .values = information_schema_tables_values,
            .row_count = 1U,
            .context = "mysql.slave_master_info information_schema.tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM mysql WHERE Name = 'slave_worker_info' "
                   "AND Data_free = '4194304'",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = table_status_values,
            .row_count = 1U,
            .context = "mysql.slave_worker_info show table status",
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

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    for (size_t column_index = 0U;
         expected.column_names != NULL && column_index < expected.column_count;
         ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    if (expected.values != NULL) {
        for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
            for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
                const char *expected_value =
                    expected.values[(row_index * expected.column_count) + column_index];
                const char *actual_value =
                    mylite_result_value_text(result, row_index, column_index);

                if (expected_value == expected_datetime_value) {
                    failures += expect_datetime_text(actual_value, expected.context);
                } else {
                    failures += expect_text_or_null(actual_value, expected_value, expected.context);
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

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_runtime_mysql_replication_metadata_tables_%d_%s.mylite",
        current_process_id(),
        name
    );

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
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "NULL" : expected,
                actual == NULL ? "NULL" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected [%s], got [%s]\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_datetime_text(const char *actual, const char *context) {
    if (actual == NULL) {
        fprintf(stderr, "%s: expected datetime text, got NULL\n", context);
        return 1;
    }
    if (strlen(actual) != datetime_text_length) {
        fprintf(
            stderr,
            "%s: expected datetime length %d, got [%s]\n",
            context,
            datetime_text_length,
            actual
        );
        return 1;
    }
    for (size_t index = 0U; index < datetime_text_length; ++index) {
        bool is_separator =
            index == datetime_year_month_separator || index == datetime_month_day_separator ||
            index == datetime_date_time_separator || index == datetime_hour_minute_separator ||
            index == datetime_minute_second_separator;
        char expected_separator = '\0';

        if (!is_separator) {
            if (actual[index] < '0' || actual[index] > '9') {
                fprintf(stderr, "%s: expected datetime digit, got [%s]\n", context, actual);
                return 1;
            }
            continue;
        }

        if (index == datetime_date_time_separator) {
            expected_separator = ' ';
        } else if (index < datetime_date_time_separator) {
            expected_separator = '-';
        } else {
            expected_separator = ':';
        }
        if (actual[index] != expected_separator) {
            fprintf(stderr, "%s: expected datetime separator, got [%s]\n", context, actual);
            return 1;
        }
    }
    return 0;
}
