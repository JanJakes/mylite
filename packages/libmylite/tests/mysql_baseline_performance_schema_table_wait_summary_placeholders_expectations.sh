#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_table_wait_summary_placeholders_expectations: $1" >&2
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

tables="'table_io_waits_summary_by_index_usage',
        'table_io_waits_summary_by_table',
        'table_lock_waits_summary_by_table'"

order_tables="'table_io_waits_summary_by_index_usage',
              'table_io_waits_summary_by_table',
              'table_lock_waits_summary_by_table'"

expect_output \
    "Performance Schema table wait summary column counts" \
    "$(printf '%b' 'table_io_waits_summary_by_index_usage\t39\n' \
        'table_io_waits_summary_by_table\t38\n' \
        'table_lock_waits_summary_by_table\t68')" \
    "SELECT TABLE_NAME, COUNT(*)
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      GROUP BY TABLE_NAME
      ORDER BY FIELD(TABLE_NAME, $order_tables);"

expect_output \
    "Performance Schema table wait summary representative columns" \
    "$(printf '%b' 'table_io_waits_summary_by_index_usage\tOBJECT_TYPE\t1\tYES\tvarchar(64)\tMUL\tutf8mb4_0900_ai_ci\tutf8mb4\t64\tNULL\tNULL\tNULL\n' \
        'table_io_waits_summary_by_index_usage\tINDEX_NAME\t4\tYES\tvarchar(64)\t\tutf8mb4_0900_ai_ci\tutf8mb4\t64\tNULL\tNULL\tNULL\n' \
        'table_io_waits_summary_by_index_usage\tCOUNT_FETCH\t20\tNO\tbigint unsigned\t\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'table_io_waits_summary_by_index_usage\tMAX_TIMER_DELETE\t39\tNO\tbigint unsigned\t\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'table_io_waits_summary_by_table\tOBJECT_TYPE\t1\tYES\tvarchar(64)\tMUL\tutf8mb4_0900_ai_ci\tutf8mb4\t64\tNULL\tNULL\tNULL\n' \
        'table_io_waits_summary_by_table\tCOUNT_FETCH\t19\tNO\tbigint unsigned\t\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'table_lock_waits_summary_by_table\tOBJECT_TYPE\t1\tYES\tvarchar(64)\tMUL\tutf8mb4_0900_ai_ci\tutf8mb4\t64\tNULL\tNULL\tNULL\n' \
        'table_lock_waits_summary_by_table\tCOUNT_READ_WITH_SHARED_LOCKS\t24\tNO\tbigint unsigned\t\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'table_lock_waits_summary_by_table\tMAX_TIMER_WRITE_EXTERNAL\t68\tNO\tbigint unsigned\t\tNULL\tNULL\tNULL\t20\t0\tNULL')" \
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
            (TABLE_NAME = 'table_io_waits_summary_by_index_usage'
             AND COLUMN_NAME IN ('OBJECT_TYPE', 'INDEX_NAME', 'COUNT_FETCH', 'MAX_TIMER_DELETE'))
            OR (TABLE_NAME = 'table_io_waits_summary_by_table'
                AND COLUMN_NAME IN ('OBJECT_TYPE', 'COUNT_FETCH'))
            OR (TABLE_NAME = 'table_lock_waits_summary_by_table'
                AND COLUMN_NAME IN (
                    'OBJECT_TYPE',
                    'COUNT_READ_WITH_SHARED_LOCKS',
                    'MAX_TIMER_WRITE_EXTERNAL'))
        )
      ORDER BY FIELD(TABLE_NAME, $order_tables), ORDINAL_POSITION;"

expect_output \
    "Performance Schema table wait summary statistics" \
    "$(printf '%b' 'table_io_waits_summary_by_index_usage\tOBJECT\t0\t1\tOBJECT_TYPE\t1\t1\tHASH\tYES\n' \
        'table_io_waits_summary_by_index_usage\tOBJECT\t0\t2\tOBJECT_SCHEMA\t1\t1\tHASH\tYES\n' \
        'table_io_waits_summary_by_index_usage\tOBJECT\t0\t3\tOBJECT_NAME\t1\t1\tHASH\tYES\n' \
        'table_io_waits_summary_by_index_usage\tOBJECT\t0\t4\tINDEX_NAME\t1\t1\tHASH\tYES\n' \
        'table_io_waits_summary_by_table\tOBJECT\t0\t1\tOBJECT_TYPE\t1\t1\tHASH\tYES\n' \
        'table_io_waits_summary_by_table\tOBJECT\t0\t2\tOBJECT_SCHEMA\t1\t1\tHASH\tYES\n' \
        'table_io_waits_summary_by_table\tOBJECT\t0\t3\tOBJECT_NAME\t1\t1\tHASH\tYES\n' \
        'table_lock_waits_summary_by_table\tOBJECT\t0\t1\tOBJECT_TYPE\t1\t1\tHASH\tYES\n' \
        'table_lock_waits_summary_by_table\tOBJECT\t0\t2\tOBJECT_SCHEMA\t1\t1\tHASH\tYES\n' \
        'table_lock_waits_summary_by_table\tOBJECT\t0\t3\tOBJECT_NAME\t1\t1\tHASH\tYES')" \
    "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
            COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema table wait summary constraints" \
    "$(printf '%b' 'table_io_waits_summary_by_index_usage\tOBJECT\tUNIQUE\tYES\n' \
        'table_io_waits_summary_by_table\tOBJECT\tUNIQUE\tYES\n' \
        'table_lock_waits_summary_by_table\tOBJECT\tUNIQUE\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME;"

expect_output \
    "Performance Schema table wait summary key usage" \
    "$(printf '%b' 'table_io_waits_summary_by_index_usage\tOBJECT\tOBJECT_TYPE\t1\n' \
        'table_io_waits_summary_by_index_usage\tOBJECT\tOBJECT_SCHEMA\t2\n' \
        'table_io_waits_summary_by_index_usage\tOBJECT\tOBJECT_NAME\t3\n' \
        'table_io_waits_summary_by_index_usage\tOBJECT\tINDEX_NAME\t4\n' \
        'table_io_waits_summary_by_table\tOBJECT\tOBJECT_TYPE\t1\n' \
        'table_io_waits_summary_by_table\tOBJECT\tOBJECT_SCHEMA\t2\n' \
        'table_io_waits_summary_by_table\tOBJECT\tOBJECT_NAME\t3\n' \
        'table_lock_waits_summary_by_table\tOBJECT\tOBJECT_TYPE\t1\n' \
        'table_lock_waits_summary_by_table\tOBJECT\tOBJECT_SCHEMA\t2\n' \
        'table_lock_waits_summary_by_table\tOBJECT\tOBJECT_NAME\t3')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema table wait summary constraint extensions" \
    "$(printf '%b' 'table_io_waits_summary_by_index_usage\tOBJECT\t1\t1\n' \
        'table_io_waits_summary_by_table\tOBJECT\t1\t1\n' \
        'table_lock_waits_summary_by_table\tOBJECT\t1\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME,
            ENGINE_ATTRIBUTE IS NULL, SECONDARY_ENGINE_ATTRIBUTE IS NULL
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME;"

expect_output \
    "Performance Schema table wait summary table metadata" \
    "$(printf '%b' 'table_io_waits_summary_by_index_usage\tPERFORMANCE_SCHEMA\tDynamic\t8192\tNULL\tutf8mb4_0900_ai_ci\n' \
        'table_io_waits_summary_by_table\tPERFORMANCE_SCHEMA\tDynamic\t4096\tNULL\tutf8mb4_0900_ai_ci\n' \
        'table_lock_waits_summary_by_table\tPERFORMANCE_SCHEMA\tDynamic\t4096\tNULL\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS,
            COALESCE(AUTO_INCREMENT, 'NULL'), TABLE_COLLATION
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables);"

printf '%s\n' "mysql_baseline_performance_schema_table_wait_summary_placeholders_expectations: ok"
