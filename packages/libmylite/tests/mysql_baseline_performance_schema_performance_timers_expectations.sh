#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_performance_timers_expectations: $1" >&2
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
    "Performance Schema performance_timers columns" \
    "$(printf '%b' 'performance_timers\tTIMER_NAME\t1\tNULL\tNO\tenum\t11\t44\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tenum(\047CYCLE\047,\047NANOSECOND\047,\047MICROSECOND\047,\047MILLISECOND\047,\047THREAD_CPU\047)\t\t\tselect,insert,update,references\t\t\n' \
        'performance_timers\tTIMER_FREQUENCY\t2\tNULL\tYES\tbigint\tNULL\tNULL\t19\t0\tNULL\tNULL\tNULL\tbigint\t\t\tselect,insert,update,references\t\t\n' \
        'performance_timers\tTIMER_RESOLUTION\t3\tNULL\tYES\tbigint\tNULL\tNULL\t19\t0\tNULL\tNULL\tNULL\tbigint\t\t\tselect,insert,update,references\t\t\n' \
        'performance_timers\tTIMER_OVERHEAD\t4\tNULL\tYES\tbigint\tNULL\tNULL\t19\t0\tNULL\tNULL\tNULL\tbigint\t\t\tselect,insert,update,references\t\t')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE,
            DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,
            NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION,
            CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, EXTRA,
            PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'performance_timers'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "Performance Schema performance_timers has no indexes" \
    "" \
    "SHOW INDEX FROM performance_schema.performance_timers;"

expect_output \
    "Performance Schema performance_timers constraints" \
    "$(printf '%b' '0\n0\n0\n0')" \
    "SELECT COUNT(*)
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'performance_timers';
     SELECT COUNT(*)
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'performance_timers';
     SELECT COUNT(*)
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'performance_timers';
     SELECT COUNT(*)
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'performance_timers';"

show_table_status_stable=$(
    run_mysql "SHOW TABLE STATUS FROM performance_schema
                WHERE Name = 'performance_timers'
                  AND Engine = 'PERFORMANCE_SCHEMA'
                  AND Row_format = 'Fixed'
                  AND Auto_increment IS NULL
                  AND Collation = 'utf8mb4_0900_ai_ci';" \
        | awk -F '\t' '{print $1 "\t" $2 "\t" $4 "\t" $5 "\t" $11 "\t" $15}'
)
if [ "$show_table_status_stable" != "performance_timers	PERFORMANCE_SCHEMA	Fixed	5	NULL	utf8mb4_0900_ai_ci" ]; then
    fail "Performance Schema performance_timers SHOW TABLE STATUS: got [$show_table_status_stable]"
fi

expect_output \
    "Performance Schema performance_timers table metadata" \
    "performance_timers	BASE TABLE	PERFORMANCE_SCHEMA	10	Fixed	5	NULL	1	1	1	utf8mb4_0900_ai_ci	1	1	1" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL,
            CREATE_OPTIONS = '', TABLE_COMMENT = ''
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'performance_timers';"

expect_output \
    "Performance Schema performance_timers rows" \
    "$(printf '%b' 'CYCLE\t0\t0\t0\n' \
        'NANOSECOND\t0\t0\t0\n' \
        'MICROSECOND\t0\t0\t0\n' \
        'MILLISECOND\t0\t0\t0\n' \
        'THREAD_CPU\t0\t0\t0')" \
    "SELECT TIMER_NAME, TIMER_FREQUENCY IS NULL, TIMER_RESOLUTION IS NULL,
            TIMER_OVERHEAD IS NULL
       FROM performance_schema.performance_timers
      ORDER BY TIMER_NAME;"

expect_output \
    "Performance Schema performance_timers row count" \
    "5" \
    "SELECT COUNT(*) FROM performance_schema.performance_timers;"

printf '%s\n' "mysql_baseline_performance_schema_performance_timers_expectations: ok"
