#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_user_variables_by_thread_expectations: $1" >&2
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
    "Performance Schema user_variables_by_thread columns" \
    "$(printf '%b' 'user_variables_by_thread\tTHREAD_ID\t1\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tNULL\tbigint unsigned\tPRI\t\tselect,insert,update,references\t\t\n' \
        'user_variables_by_thread\tVARIABLE_NAME\t2\tNULL\tNO\tvarchar\t64\t256\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'user_variables_by_thread\tVARIABLE_VALUE\t3\tNULL\tYES\tlongblob\t4294967295\t4294967295\tNULL\tNULL\tNULL\tNULL\tNULL\tlongblob\t\t\tselect,insert,update,references\t\t')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE,
            DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,
            NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION,
            CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, EXTRA,
            PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'user_variables_by_thread'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "Performance Schema user_variables_by_thread statistics" \
    "$(printf '%b' 'PRIMARY\t0\t1\tTHREAD_ID\t1\t1\tHASH\tYES\n' \
        'PRIMARY\t0\t2\tVARIABLE_NAME\t1\t1\tHASH\tYES')" \
    "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION IS NULL,
            CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'user_variables_by_thread'
      ORDER BY INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema user_variables_by_thread constraints" \
    "PRIMARY	PRIMARY KEY	YES" \
    "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'user_variables_by_thread';"

expect_output \
    "Performance Schema user_variables_by_thread key usage" \
    "$(printf '%b' 'PRIMARY\tTHREAD_ID\t1\n' \
        'PRIMARY\tVARIABLE_NAME\t2')" \
    "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'user_variables_by_thread'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "Performance Schema user_variables_by_thread constraint extensions" \
    "performance_schema	user_variables_by_thread	PRIMARY	1	1" \
    "SELECT CONSTRAINT_SCHEMA, TABLE_NAME, CONSTRAINT_NAME,
            ENGINE_ATTRIBUTE IS NULL, SECONDARY_ENGINE_ATTRIBUTE IS NULL
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'user_variables_by_thread';"

user_variables_status=$(
    run_mysql "SHOW TABLE STATUS FROM performance_schema
                WHERE Name = 'user_variables_by_thread'
                  AND Engine = 'PERFORMANCE_SCHEMA'
                  AND Row_format = 'Dynamic'
                  AND Auto_increment IS NULL
                  AND Collation = 'utf8mb4_0900_ai_ci';" \
        | awk -F '\t' '{print $1 "\t" $2 "\t" $4 "\t" $5 "\t" $11 "\t" $15}'
)
if [ "$user_variables_status" != "user_variables_by_thread	PERFORMANCE_SCHEMA	Dynamic	2560	NULL	utf8mb4_0900_ai_ci" ]; then
    fail "Performance Schema user_variables_by_thread SHOW TABLE STATUS: got [$user_variables_status]"
fi

expect_output \
    "Performance Schema user_variables_by_thread table metadata" \
    "user_variables_by_thread	BASE TABLE	PERFORMANCE_SCHEMA	10	Dynamic	2560	NULL	1	1	1	utf8mb4_0900_ai_ci	1	1	1" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL,
            CREATE_OPTIONS = '', TABLE_COMMENT = ''
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'user_variables_by_thread';"

expect_output \
    "Performance Schema user_variables_by_thread starts empty for current session" \
    "0" \
    "SELECT COUNT(*)
       FROM performance_schema.user_variables_by_thread
      WHERE THREAD_ID = PS_CURRENT_THREAD_ID();"

expect_output \
    "Performance Schema user_variables_by_thread rows" \
    "$(printf '%b' '1\tdec\t0\t1.25\n' \
        '1\tMixedName\t0\tA\n' \
        '1\tnullish\t1\tNULL\n' \
        '1\tnum\t0\t42')" \
    "SET @MixedName = 'A', @nullish = NULL, @num = 42, @dec = 1.25;
     SELECT THREAD_ID = PS_CURRENT_THREAD_ID(), VARIABLE_NAME, VARIABLE_VALUE IS NULL,
            VARIABLE_VALUE
       FROM performance_schema.user_variables_by_thread
      WHERE THREAD_ID = PS_CURRENT_THREAD_ID()
        AND VARIABLE_NAME IN ('MixedName', 'nullish', 'num', 'dec')
      ORDER BY VARIABLE_NAME;"

expect_output \
    "Performance Schema user_variables_by_thread preserves first display name" \
    "CaseProbe	second" \
    "SET @CaseProbe = 'first';
     SET @caseprobe = 'second';
     SELECT VARIABLE_NAME, VARIABLE_VALUE
       FROM performance_schema.user_variables_by_thread
      WHERE THREAD_ID = PS_CURRENT_THREAD_ID()
        AND LOWER(VARIABLE_NAME) = 'caseprobe';"

printf '%s\n' "mysql_baseline_performance_schema_user_variables_by_thread_expectations: ok"
