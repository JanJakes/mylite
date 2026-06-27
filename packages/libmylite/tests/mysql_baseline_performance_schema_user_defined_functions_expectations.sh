#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_user_defined_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "Performance Schema user_defined_functions columns" \
    "$(printf '%b' 'user_defined_functions\tUDF_NAME\t1\tNULL\tNO\tvarchar\t64\t256\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'user_defined_functions\tUDF_RETURN_TYPE\t2\tNULL\tNO\tvarchar\t20\t80\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(20)\t\t\tselect,insert,update,references\t\t\n' \
        'user_defined_functions\tUDF_TYPE\t3\tNULL\tNO\tvarchar\t20\t80\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(20)\t\t\tselect,insert,update,references\t\t\n' \
        'user_defined_functions\tUDF_LIBRARY\t4\tNULL\tYES\tvarchar\t1024\t4096\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(1024)\t\t\tselect,insert,update,references\t\t\n' \
        'user_defined_functions\tUDF_USAGE_COUNT\t5\tNULL\tYES\tbigint\tNULL\tNULL\t19\t0\tNULL\tNULL\tNULL\tbigint\t\t\tselect,insert,update,references\t\t')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE,
            DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,
            NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION,
            CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, EXTRA,
            PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'user_defined_functions'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "Performance Schema user_defined_functions statistics" \
    "PRIMARY	0	1	UDF_NAME	1	1	HASH	YES" \
    "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION IS NULL,
            CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'user_defined_functions';"

expect_output \
    "Performance Schema user_defined_functions constraints" \
    "PRIMARY	PRIMARY KEY	YES" \
    "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'user_defined_functions';"

expect_output \
    "Performance Schema user_defined_functions key usage" \
    "PRIMARY	UDF_NAME	1" \
    "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'user_defined_functions';"

expect_output \
    "Performance Schema user_defined_functions constraint extensions" \
    "performance_schema	user_defined_functions	PRIMARY	1	1" \
    "SELECT CONSTRAINT_SCHEMA, TABLE_NAME, CONSTRAINT_NAME,
            ENGINE_ATTRIBUTE IS NULL, SECONDARY_ENGINE_ATTRIBUTE IS NULL
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'user_defined_functions';"

user_defined_functions_status=$(
    run_mysql "SHOW TABLE STATUS FROM performance_schema
                WHERE Name = 'user_defined_functions'
                  AND Engine = 'PERFORMANCE_SCHEMA'
                  AND Row_format = 'Dynamic'
                  AND Auto_increment IS NULL
                  AND Collation = 'utf8mb4_0900_ai_ci';" \
        | awk -F '\t' '{print $1 "\t" $2 "\t" $4 "\t" $5 "\t" $11 "\t" $15}'
)
if [ "$user_defined_functions_status" != "user_defined_functions	PERFORMANCE_SCHEMA	Dynamic	16	NULL	utf8mb4_0900_ai_ci" ]; then
    fail "Performance Schema user_defined_functions SHOW TABLE STATUS: got [$user_defined_functions_status]"
fi

expect_output \
    "Performance Schema user_defined_functions table metadata" \
    "user_defined_functions	BASE TABLE	PERFORMANCE_SCHEMA	10	Dynamic	16	NULL	1	1	1	utf8mb4_0900_ai_ci	1	1	1" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL,
            CREATE_OPTIONS = '', TABLE_COMMENT = ''
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'user_defined_functions';"

expect_output \
    "Performance Schema user_defined_functions rows" \
    "$(printf '%b' 'asynchronous_connection_failover_add_managed\tchar\tfunction\t1\t1\n' \
        'asynchronous_connection_failover_add_source\tchar\tfunction\t1\t1\n' \
        'asynchronous_connection_failover_delete_managed\tchar\tfunction\t1\t1\n' \
        'asynchronous_connection_failover_delete_source\tchar\tfunction\t1\t1\n' \
        'asynchronous_connection_failover_reset\tchar\tfunction\t1\t1\n' \
        'innodb_redo_log_archive_flush\tinteger\tfunction\t1\t1\n' \
        'innodb_redo_log_archive_start\tinteger\tfunction\t1\t1\n' \
        'innodb_redo_log_archive_stop\tinteger\tfunction\t1\t1\n' \
        'innodb_redo_log_consumer_advance\tinteger\tfunction\t1\t1\n' \
        'innodb_redo_log_consumer_register\tinteger\tfunction\t1\t1\n' \
        'innodb_redo_log_consumer_unregister\tinteger\tfunction\t1\t1\n' \
        'innodb_redo_log_sharp_checkpoint\tinteger\tfunction\t1\t1\n' \
        'innodb_set_open_files_limit\tinteger\tfunction\t1\t1\n' \
        'mysqlx_error\tchar\tfunction\t1\t1\n' \
        'mysqlx_generate_document_id\tchar\tfunction\t1\t1\n' \
        'mysqlx_get_prepared_statement_id\tinteger\tfunction\t1\t1')" \
    "SELECT UDF_NAME, UDF_RETURN_TYPE, UDF_TYPE, UDF_LIBRARY IS NULL,
            UDF_USAGE_COUNT
       FROM performance_schema.user_defined_functions
      ORDER BY UDF_NAME;"

expect_output \
    "Performance Schema user_defined_functions row count" \
    "16" \
    "SELECT COUNT(*) FROM performance_schema.user_defined_functions;"

printf '%s\n' "mysql_baseline_performance_schema_user_defined_functions_expectations: ok"
