#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_table_handles_table_expectations: $1" >&2
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
    "Performance Schema table_handles columns" \
    "$(printf '%b' 'table_handles\tOBJECT_TYPE\t1\tNULL\tNO\tvarchar\t64\t256\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\tMUL\t\tselect,insert,update,references\t\t\n' \
        'table_handles\tOBJECT_SCHEMA\t2\tNULL\tNO\tvarchar\t64\t256\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\t\t\tselect,insert,update,references\t\t\n' \
        'table_handles\tOBJECT_NAME\t3\tNULL\tNO\tvarchar\t64\t256\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\t\t\tselect,insert,update,references\t\t\n' \
        'table_handles\tOBJECT_INSTANCE_BEGIN\t4\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tNULL\tbigint unsigned\tPRI\t\tselect,insert,update,references\t\t\n' \
        'table_handles\tOWNER_THREAD_ID\t5\tNULL\tYES\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tNULL\tbigint unsigned\tMUL\t\tselect,insert,update,references\t\t\n' \
        'table_handles\tOWNER_EVENT_ID\t6\tNULL\tYES\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t\t\n' \
        'table_handles\tINTERNAL_LOCK\t7\tNULL\tYES\tvarchar\t64\t256\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\t\t\tselect,insert,update,references\t\t\n' \
        'table_handles\tEXTERNAL_LOCK\t8\tNULL\tYES\tvarchar\t64\t256\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\t\t\tselect,insert,update,references\t\t')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE,
            DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,
            NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION,
            CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, EXTRA,
            PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'table_handles'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "Performance Schema table_handles statistics" \
    "$(printf '%b' 'OBJECT_TYPE\t1\t1\tOBJECT_TYPE\t1\t1\tHASH\tYES\n' \
        'OBJECT_TYPE\t1\t2\tOBJECT_SCHEMA\t1\t1\tHASH\tYES\n' \
        'OBJECT_TYPE\t1\t3\tOBJECT_NAME\t1\t1\tHASH\tYES\n' \
        'OWNER_THREAD_ID\t1\t1\tOWNER_THREAD_ID\t1\t1\tHASH\tYES\n' \
        'OWNER_THREAD_ID\t1\t2\tOWNER_EVENT_ID\t1\t1\tHASH\tYES\n' \
        'PRIMARY\t0\t1\tOBJECT_INSTANCE_BEGIN\t1\t1\tHASH\tYES')" \
    "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION IS NULL,
            CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'table_handles'
      ORDER BY INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema table_handles constraints" \
    "PRIMARY	PRIMARY KEY	YES" \
    "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'table_handles';"

expect_output \
    "Performance Schema table_handles key usage" \
    "PRIMARY	OBJECT_INSTANCE_BEGIN	1" \
    "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'table_handles';"

expect_output \
    "Performance Schema table_handles constraint extensions" \
    "$(printf '%b' 'performance_schema\ttable_handles\tOBJECT_TYPE\t1\t1\n' \
        'performance_schema\ttable_handles\tOWNER_THREAD_ID\t1\t1\n' \
        'performance_schema\ttable_handles\tPRIMARY\t1\t1')" \
    "SELECT CONSTRAINT_SCHEMA, TABLE_NAME, CONSTRAINT_NAME,
            ENGINE_ATTRIBUTE IS NULL, SECONDARY_ENGINE_ATTRIBUTE IS NULL
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'table_handles'
      ORDER BY CONSTRAINT_NAME;"

table_handles_status=$(
    run_mysql "SHOW TABLE STATUS FROM performance_schema
                WHERE Name = 'table_handles'
                  AND Engine = 'PERFORMANCE_SCHEMA'
                  AND Row_format = 'Dynamic'
                  AND Auto_increment IS NULL
                  AND Collation = 'utf8mb4_0900_ai_ci';" \
        | awk -F '\t' '{print $1 "\t" $2 "\t" $4 "\t" $5 "\t" $11 "\t" $15}'
)
if [ "$table_handles_status" != "table_handles	PERFORMANCE_SCHEMA	Dynamic	0	NULL	utf8mb4_0900_ai_ci" ]; then
    fail "Performance Schema table_handles SHOW TABLE STATUS: got [$table_handles_status]"
fi

expect_output \
    "Performance Schema table_handles table metadata" \
    "table_handles	BASE TABLE	PERFORMANCE_SCHEMA	10	Dynamic	0	NULL	1	1	1	utf8mb4_0900_ai_ci	1	1	1" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL,
            CREATE_OPTIONS = '', TABLE_COMMENT = ''
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'table_handles';"

expect_output \
    "Performance Schema table_handles show columns" \
    "$(printf '%b' 'OBJECT_TYPE\tvarchar(64)\tNO\tMUL\tNULL\t\n' \
        'OBJECT_SCHEMA\tvarchar(64)\tNO\t\tNULL\t\n' \
        'OBJECT_NAME\tvarchar(64)\tNO\t\tNULL\t\n' \
        'OBJECT_INSTANCE_BEGIN\tbigint unsigned\tNO\tPRI\tNULL\t\n' \
        'OWNER_THREAD_ID\tbigint unsigned\tYES\tMUL\tNULL\t\n' \
        'OWNER_EVENT_ID\tbigint unsigned\tYES\t\tNULL\t\n' \
        'INTERNAL_LOCK\tvarchar(64)\tYES\t\tNULL\t\n' \
        'EXTERNAL_LOCK\tvarchar(64)\tYES\t\tNULL\t')" \
    "SHOW COLUMNS FROM performance_schema.table_handles;"

expect_output \
    "Performance Schema table_handles show index" \
    "$(printf '%b' 'table_handles\t0\tPRIMARY\t1\tOBJECT_INSTANCE_BEGIN\tNULL\tNULL\tNULL\tNULL\t\tHASH\t\t\tYES\tNULL\n' \
        'table_handles\t1\tOBJECT_TYPE\t1\tOBJECT_TYPE\tNULL\tNULL\tNULL\tNULL\t\tHASH\t\t\tYES\tNULL\n' \
        'table_handles\t1\tOBJECT_TYPE\t2\tOBJECT_SCHEMA\tNULL\tNULL\tNULL\tNULL\t\tHASH\t\t\tYES\tNULL\n' \
        'table_handles\t1\tOBJECT_TYPE\t3\tOBJECT_NAME\tNULL\tNULL\tNULL\tNULL\t\tHASH\t\t\tYES\tNULL\n' \
        'table_handles\t1\tOWNER_THREAD_ID\t1\tOWNER_THREAD_ID\tNULL\tNULL\tNULL\tNULL\tYES\tHASH\t\t\tYES\tNULL\n' \
        'table_handles\t1\tOWNER_THREAD_ID\t2\tOWNER_EVENT_ID\tNULL\tNULL\tNULL\tNULL\tYES\tHASH\t\t\tYES\tNULL')" \
    "SHOW INDEX FROM performance_schema.table_handles;"

expect_output \
    "Performance Schema table_handles empty row set" \
    "0" \
    "SELECT COUNT(*) FROM performance_schema.table_handles;"

printf '%s\n' "mysql_baseline_performance_schema_table_handles_table_expectations: ok"
