#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_setup_objects_expectations: $1" >&2
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
    "Performance Schema setup_objects columns" \
    "$(printf '%b' 'setup_objects\tOBJECT_TYPE\t1\tTABLE\tNO\tenum\t9\t36\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tenum(\047EVENT\047,\047FUNCTION\047,\047PROCEDURE\047,\047TABLE\047,\047TRIGGER\047)\tMUL\t\tselect,insert,update,references\t\t\n' \
        'setup_objects\tOBJECT_SCHEMA\t2\t%\tYES\tvarchar\t64\t256\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\t\t\tselect,insert,update,references\t\t\n' \
        'setup_objects\tOBJECT_NAME\t3\t%\tNO\tvarchar\t64\t256\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\t\t\tselect,insert,update,references\t\t\n' \
        'setup_objects\tENABLED\t4\tYES\tNO\tenum\t3\t12\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tenum(\047YES\047,\047NO\047)\t\t\tselect,insert,update,references\t\t\n' \
        'setup_objects\tTIMED\t5\tYES\tNO\tenum\t3\t12\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tenum(\047YES\047,\047NO\047)\t\t\tselect,insert,update,references\t\t')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE,
            DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,
            NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION,
            CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, EXTRA,
            PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_objects'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "Performance Schema setup_objects statistics" \
    "$(printf '%b' 'OBJECT\t0\t1\tOBJECT_TYPE\t1\t1\tHASH\tYES\n' \
        'OBJECT\t0\t2\tOBJECT_SCHEMA\t1\t1\tHASH\tYES\n' \
        'OBJECT\t0\t3\tOBJECT_NAME\t1\t1\tHASH\tYES')" \
    "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION IS NULL,
            CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_objects'
      ORDER BY INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema setup_objects constraints" \
    "OBJECT	UNIQUE	YES" \
    "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_objects';"

expect_output \
    "Performance Schema setup_objects key usage" \
    "$(printf '%b' 'OBJECT\tOBJECT_TYPE\t1\n' \
        'OBJECT\tOBJECT_SCHEMA\t2\n' \
        'OBJECT\tOBJECT_NAME\t3')" \
    "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_objects'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "Performance Schema setup_objects constraint extensions" \
    "performance_schema	setup_objects	OBJECT	1	1" \
    "SELECT CONSTRAINT_SCHEMA, TABLE_NAME, CONSTRAINT_NAME,
            ENGINE_ATTRIBUTE IS NULL, SECONDARY_ENGINE_ATTRIBUTE IS NULL
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_objects';"

setup_objects_status=$(
    run_mysql "SHOW TABLE STATUS FROM performance_schema
                WHERE Name = 'setup_objects'
                  AND Engine = 'PERFORMANCE_SCHEMA'
                  AND Row_format = 'Dynamic'
                  AND Auto_increment IS NULL
                  AND Collation = 'utf8mb4_0900_ai_ci';" \
        | awk -F '\t' '{print $1 "\t" $2 "\t" $4 "\t" $5 "\t" $11 "\t" $15}'
)
if [ "$setup_objects_status" != "setup_objects	PERFORMANCE_SCHEMA	Dynamic	128	NULL	utf8mb4_0900_ai_ci" ]; then
    fail "Performance Schema setup_objects SHOW TABLE STATUS: got [$setup_objects_status]"
fi

expect_output \
    "Performance Schema setup_objects table metadata" \
    "setup_objects	BASE TABLE	PERFORMANCE_SCHEMA	10	Dynamic	128	NULL	1	1	1	utf8mb4_0900_ai_ci	1	1	1" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL,
            CREATE_OPTIONS = '', TABLE_COMMENT = ''
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'setup_objects';"

expect_output \
    "Performance Schema setup_objects rows" \
    "$(printf '%b' 'EVENT\t%\t%\tYES\tYES\n' \
        'EVENT\tinformation_schema\t%\tNO\tNO\n' \
        'EVENT\tmysql\t%\tNO\tNO\n' \
        'EVENT\tperformance_schema\t%\tNO\tNO\n' \
        'FUNCTION\t%\t%\tYES\tYES\n' \
        'FUNCTION\tinformation_schema\t%\tNO\tNO\n' \
        'FUNCTION\tmysql\t%\tNO\tNO\n' \
        'FUNCTION\tperformance_schema\t%\tNO\tNO\n' \
        'PROCEDURE\t%\t%\tYES\tYES\n' \
        'PROCEDURE\tinformation_schema\t%\tNO\tNO\n' \
        'PROCEDURE\tmysql\t%\tNO\tNO\n' \
        'PROCEDURE\tperformance_schema\t%\tNO\tNO\n' \
        'TABLE\t%\t%\tYES\tYES\n' \
        'TABLE\tinformation_schema\t%\tNO\tNO\n' \
        'TABLE\tmysql\t%\tNO\tNO\n' \
        'TABLE\tperformance_schema\t%\tNO\tNO\n' \
        'TRIGGER\t%\t%\tYES\tYES\n' \
        'TRIGGER\tinformation_schema\t%\tNO\tNO\n' \
        'TRIGGER\tmysql\t%\tNO\tNO\n' \
        'TRIGGER\tperformance_schema\t%\tNO\tNO')" \
    "SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, ENABLED, TIMED
       FROM performance_schema.setup_objects
      ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME;"

printf '%s\n' "mysql_baseline_performance_schema_setup_objects_expectations: ok"
