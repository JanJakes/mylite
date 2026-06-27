#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_setup_logger_meter_tables_expectations: $1" >&2
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
    "Performance Schema setup_loggers columns" \
    "$(printf '%b' 'setup_loggers\tNAME\t1\tNULL\tNO\tvarchar\t128\t512\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(128)\t\t\tselect,insert,update,references\t\t\n' \
        'setup_loggers\tLEVEL\t2\tNULL\tNO\tenum\t5\t20\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tenum(\047none\047,\047error\047,\047warn\047,\047info\047,\047debug\047)\t\t\tselect,insert,update,references\t\t\n' \
        'setup_loggers\tDESCRIPTION\t3\tNULL\tYES\tvarchar\t1023\t4092\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(1023)\t\t\tselect,insert,update,references\t\t')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE,
            DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,
            NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION,
            CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, EXTRA,
            PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_loggers'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "Performance Schema setup_loggers no constraints" \
    "$(printf '%b' '0\n0\n0\n0')" \
    "SELECT COUNT(*)
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_loggers';
     SELECT COUNT(*)
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_loggers';
     SELECT COUNT(*)
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_loggers';
     SELECT COUNT(*)
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_loggers';"

setup_loggers_status=$(
    run_mysql "SHOW TABLE STATUS FROM performance_schema
                WHERE Name = 'setup_loggers'
                  AND Engine = 'PERFORMANCE_SCHEMA'
                  AND Row_format = 'Dynamic'
                  AND Auto_increment IS NULL
                  AND Collation = 'utf8mb4_0900_ai_ci';" \
        | awk -F '\t' '{print $1 "\t" $2 "\t" $4 "\t" $5 "\t" $11 "\t" $15}'
)
if [ "$setup_loggers_status" != "setup_loggers	PERFORMANCE_SCHEMA	Dynamic	1	NULL	utf8mb4_0900_ai_ci" ]; then
    fail "Performance Schema setup_loggers SHOW TABLE STATUS: got [$setup_loggers_status]"
fi

expect_output \
    "Performance Schema setup_loggers table metadata" \
    "setup_loggers	BASE TABLE	PERFORMANCE_SCHEMA	10	Dynamic	1	NULL	1	1	1	utf8mb4_0900_ai_ci	1	1	1" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL,
            CREATE_OPTIONS = '', TABLE_COMMENT = ''
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_loggers';"

expect_output \
    "Performance Schema setup_loggers rows" \
    "logger/error/error_log	info	MySQL error logger" \
    "SELECT NAME, LEVEL, DESCRIPTION
       FROM performance_schema.setup_loggers
      ORDER BY NAME;"

expect_output \
    "Performance Schema setup_meters columns" \
    "$(printf '%b' 'setup_meters\tNAME\t1\tNULL\tNO\tvarchar\t63\t252\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(63)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'setup_meters\tFREQUENCY\t2\tNULL\tNO\tmediumint\tNULL\tNULL\t7\t0\tNULL\tNULL\tNULL\tmediumint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'setup_meters\tENABLED\t3\tNULL\tNO\tenum\t3\t12\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tenum(\047YES\047,\047NO\047)\t\t\tselect,insert,update,references\t\t\n' \
        'setup_meters\tDESCRIPTION\t4\tNULL\tYES\tvarchar\t1023\t4092\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(1023)\t\t\tselect,insert,update,references\t\t')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE,
            DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,
            NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION,
            CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, EXTRA,
            PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_meters'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "Performance Schema setup_meters statistics" \
    "PRIMARY	0	1	NAME	1	1	HASH	YES" \
    "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION IS NULL,
            CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_meters';"

expect_output \
    "Performance Schema setup_meters constraints" \
    "PRIMARY	PRIMARY KEY	YES" \
    "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_meters';"

expect_output \
    "Performance Schema setup_meters key usage" \
    "PRIMARY	NAME	1" \
    "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_meters';"

expect_output \
    "Performance Schema setup_meters constraint extensions" \
    "performance_schema	setup_meters	PRIMARY	1	1" \
    "SELECT CONSTRAINT_SCHEMA, TABLE_NAME, CONSTRAINT_NAME,
            ENGINE_ATTRIBUTE IS NULL, SECONDARY_ENGINE_ATTRIBUTE IS NULL
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_meters';"

setup_meters_status=$(
    run_mysql "SHOW TABLE STATUS FROM performance_schema
                WHERE Name = 'setup_meters'
                  AND Engine = 'PERFORMANCE_SCHEMA'
                  AND Row_format = 'Dynamic'
                  AND Auto_increment IS NULL
                  AND Collation = 'utf8mb4_0900_ai_ci';" \
        | awk -F '\t' '{print $1 "\t" $2 "\t" $4 "\t" $5 "\t" $11 "\t" $15}'
)
if [ "$setup_meters_status" != "setup_meters	PERFORMANCE_SCHEMA	Dynamic	12	NULL	utf8mb4_0900_ai_ci" ]; then
    fail "Performance Schema setup_meters SHOW TABLE STATUS: got [$setup_meters_status]"
fi

expect_output \
    "Performance Schema setup_meters table metadata" \
    "setup_meters	BASE TABLE	PERFORMANCE_SCHEMA	10	Dynamic	12	NULL	1	1	1	utf8mb4_0900_ai_ci	1	1	1" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL,
            CREATE_OPTIONS = '', TABLE_COMMENT = ''
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_meters';"

expect_output \
    "Performance Schema setup_meters rows" \
    "$(printf '%b' 'mysql.inno\t10\tYES\tMySql InnoDB metrics\n' \
        'mysql.inno.buffer_pool\t10\tYES\tMySql InnoDB buffer pool metrics\n' \
        'mysql.inno.data\t10\tYES\tMySql InnoDB data metrics\n' \
        'mysql.myisam\t10\tYES\tMySql MyISAM storage engine stats\n' \
        'mysql.perf_schema\t10\tYES\tMySql performance_schema lost instruments\n' \
        'mysql.stats\t10\tYES\tMySql core metrics\n' \
        'mysql.stats.com\t10\tYES\tMySql command stats\n' \
        'mysql.stats.connection\t10\tYES\tMySql connection stats\n' \
        'mysql.stats.handler\t10\tYES\tMySql handler stats\n' \
        'mysql.stats.ssl\t10\tYES\tMySql TLS related stats\n' \
        'mysql.x\t10\tYES\tMySql X plugin metrics\n' \
        'mysql.x.stmt\t10\tYES\tMySql X plugin statement statistics')" \
    "SELECT NAME, FREQUENCY, ENABLED, DESCRIPTION
       FROM performance_schema.setup_meters
      ORDER BY NAME;"

printf '%s\n' "mysql_baseline_performance_schema_setup_logger_meter_tables_expectations: ok"
