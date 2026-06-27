#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_file_summary_placeholders_expectations: $1" >&2
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

tables="'file_summary_by_event_name',
        'file_summary_by_instance'"

order_tables="'file_summary_by_event_name',
              'file_summary_by_instance'"

expect_output \
    "Performance Schema file summary row counts" \
    "$(printf '%b' 'file_summary_by_event_name\t51\n' \
        'file_summary_by_instance\t170')" \
    "SELECT 'file_summary_by_event_name',
            COUNT(*)
       FROM performance_schema.file_summary_by_event_name
      UNION ALL
     SELECT 'file_summary_by_instance',
            COUNT(*)
       FROM performance_schema.file_summary_by_instance;"

expect_output \
    "Performance Schema file summary column counts" \
    "$(printf '%b' 'file_summary_by_event_name\t23\n' \
        'file_summary_by_instance\t25')" \
    "SELECT TABLE_NAME, COUNT(*)
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      GROUP BY TABLE_NAME
      ORDER BY FIELD(TABLE_NAME, $order_tables);"

expect_output \
    "Performance Schema file summary representative columns" \
    "$(printf '%b' 'file_summary_by_event_name\tEVENT_NAME\t1\tNO\tvarchar(128)\tPRI\tutf8mb4_0900_ai_ci\tutf8mb4\t128\tNULL\tNULL\tNULL\n' \
        'file_summary_by_event_name\tSUM_NUMBER_OF_BYTES_READ\t12\tNO\tbigint\t\tNULL\tNULL\tNULL\t19\t0\tNULL\n' \
        'file_summary_by_event_name\tMAX_TIMER_MISC\t23\tNO\tbigint unsigned\t\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'file_summary_by_instance\tFILE_NAME\t1\tNO\tvarchar(512)\tMUL\tutf8mb4_0900_ai_ci\tutf8mb4\t512\tNULL\tNULL\tNULL\n' \
        'file_summary_by_instance\tEVENT_NAME\t2\tNO\tvarchar(128)\tMUL\tutf8mb4_0900_ai_ci\tutf8mb4\t128\tNULL\tNULL\tNULL\n' \
        'file_summary_by_instance\tOBJECT_INSTANCE_BEGIN\t3\tNO\tbigint unsigned\tPRI\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'file_summary_by_instance\tSUM_NUMBER_OF_BYTES_WRITE\t20\tNO\tbigint\t\tNULL\tNULL\tNULL\t19\t0\tNULL')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COLUMN_KEY, COALESCE(COLLATION_NAME, 'NULL'),
            COALESCE(CHARACTER_SET_NAME, 'NULL'),
            COALESCE(CHARACTER_MAXIMUM_LENGTH, 'NULL'),
            COALESCE(NUMERIC_PRECISION, 'NULL'),
            COALESCE(NUMERIC_SCALE, 'NULL'),
            COALESCE(DATETIME_PRECISION, 'NULL')
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND (
            (TABLE_NAME = 'file_summary_by_event_name'
             AND COLUMN_NAME IN ('EVENT_NAME', 'SUM_NUMBER_OF_BYTES_READ', 'MAX_TIMER_MISC'))
            OR (TABLE_NAME = 'file_summary_by_instance'
                AND COLUMN_NAME IN (
                    'FILE_NAME',
                    'EVENT_NAME',
                    'OBJECT_INSTANCE_BEGIN',
                    'SUM_NUMBER_OF_BYTES_WRITE'
                ))
        )
      ORDER BY FIELD(TABLE_NAME, $order_tables), ORDINAL_POSITION;"

expect_output \
    "Performance Schema file summary statistics" \
    "$(printf '%b' 'file_summary_by_event_name\tPRIMARY\t0\t1\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'file_summary_by_instance\tEVENT_NAME\t1\t1\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'file_summary_by_instance\tFILE_NAME\t1\t1\tFILE_NAME\t1\t1\tHASH\tYES\n' \
        'file_summary_by_instance\tPRIMARY\t0\t1\tOBJECT_INSTANCE_BEGIN\t1\t1\tHASH\tYES')" \
    "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
            COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema file summary constraints" \
    "$(printf '%b' 'file_summary_by_event_name\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'file_summary_by_instance\tPRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME;"

expect_output \
    "Performance Schema file summary key usage" \
    "$(printf '%b' 'file_summary_by_event_name\tPRIMARY\tEVENT_NAME\t1\n' \
        'file_summary_by_instance\tPRIMARY\tOBJECT_INSTANCE_BEGIN\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema file summary constraint extensions" \
    "$(printf '%b' 'file_summary_by_event_name\tPRIMARY\t1\t1\n' \
        'file_summary_by_instance\tEVENT_NAME\t1\t1\n' \
        'file_summary_by_instance\tFILE_NAME\t1\t1\n' \
        'file_summary_by_instance\tPRIMARY\t1\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME,
            ENGINE_ATTRIBUTE IS NULL, SECONDARY_ENGINE_ATTRIBUTE IS NULL
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME;"

expect_output \
    "Performance Schema file summary table metadata" \
    "$(printf '%b' 'file_summary_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t80\tNULL\tutf8mb4_0900_ai_ci\n' \
        'file_summary_by_instance\tPERFORMANCE_SCHEMA\tDynamic\t4096\tNULL\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS,
            COALESCE(AUTO_INCREMENT, 'NULL'), TABLE_COLLATION
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables);"

printf '%s\n' "mysql_baseline_performance_schema_file_summary_placeholders_expectations: ok"
