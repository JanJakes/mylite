#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_error_summary_placeholders_expectations: $1" >&2
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

tables="'events_errors_summary_by_account_by_error',
        'events_errors_summary_by_host_by_error',
        'events_errors_summary_by_thread_by_error',
        'events_errors_summary_by_user_by_error',
        'events_errors_summary_global_by_error'"

order_tables="'events_errors_summary_by_account_by_error',
              'events_errors_summary_by_host_by_error',
              'events_errors_summary_by_thread_by_error',
              'events_errors_summary_by_user_by_error',
              'events_errors_summary_global_by_error'"

expect_output \
    "Performance Schema error summary row counts" \
    "$(printf '%b' 'events_errors_summary_by_account_by_error\t5331\n' \
        'events_errors_summary_by_host_by_error\t5331\n' \
        'events_errors_summary_by_thread_by_error\t76411\n' \
        'events_errors_summary_by_user_by_error\t5331\n' \
        'events_errors_summary_global_by_error\t5556')" \
    "SELECT 'events_errors_summary_by_account_by_error',
            COUNT(*)
       FROM performance_schema.events_errors_summary_by_account_by_error
      UNION ALL
     SELECT 'events_errors_summary_by_host_by_error',
            COUNT(*)
       FROM performance_schema.events_errors_summary_by_host_by_error
      UNION ALL
     SELECT 'events_errors_summary_by_thread_by_error',
            COUNT(*)
       FROM performance_schema.events_errors_summary_by_thread_by_error
      UNION ALL
     SELECT 'events_errors_summary_by_user_by_error',
            COUNT(*)
       FROM performance_schema.events_errors_summary_by_user_by_error
      UNION ALL
     SELECT 'events_errors_summary_global_by_error',
            COUNT(*)
       FROM performance_schema.events_errors_summary_global_by_error;"

expect_output \
    "Performance Schema error summary column counts" \
    "$(printf '%b' 'events_errors_summary_by_account_by_error\t9\n' \
        'events_errors_summary_by_host_by_error\t8\n' \
        'events_errors_summary_by_thread_by_error\t8\n' \
        'events_errors_summary_by_user_by_error\t8\n' \
        'events_errors_summary_global_by_error\t7')" \
    "SELECT TABLE_NAME, COUNT(*)
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      GROUP BY TABLE_NAME
      ORDER BY FIELD(TABLE_NAME, $order_tables);"

expect_output \
    "Performance Schema error summary representative columns" \
    "$(printf '%b' 'events_errors_summary_by_account_by_error\tUSER\t1\tYES\tchar(32)\tMUL\tutf8mb4_bin\tutf8mb4\t32\tNULL\tNULL\tNULL\n' \
        'events_errors_summary_by_account_by_error\tERROR_NUMBER\t3\tYES\tint\t\tNULL\tNULL\tNULL\t10\t0\tNULL\n' \
        'events_errors_summary_by_account_by_error\tFIRST_SEEN\t8\tYES\ttimestamp\t\tNULL\tNULL\tNULL\tNULL\tNULL\t0\n' \
        'events_errors_summary_by_thread_by_error\tTHREAD_ID\t1\tNO\tbigint unsigned\tMUL\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'events_errors_summary_by_user_by_error\tUSER\t1\tYES\tchar(32)\tMUL\tutf8mb4_bin\tutf8mb4\t32\tNULL\tNULL\tNULL\n' \
        'events_errors_summary_global_by_error\tERROR_NUMBER\t1\tYES\tint\tUNI\tNULL\tNULL\tNULL\t10\t0\tNULL\n' \
        'events_errors_summary_global_by_error\tLAST_SEEN\t7\tYES\ttimestamp\t\tNULL\tNULL\tNULL\tNULL\tNULL\t0')" \
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
            (TABLE_NAME = 'events_errors_summary_by_account_by_error'
             AND COLUMN_NAME IN ('USER', 'ERROR_NUMBER', 'FIRST_SEEN'))
            OR (TABLE_NAME = 'events_errors_summary_by_thread_by_error'
                AND COLUMN_NAME = 'THREAD_ID')
            OR (TABLE_NAME = 'events_errors_summary_by_user_by_error'
                AND COLUMN_NAME = 'USER')
            OR (TABLE_NAME = 'events_errors_summary_global_by_error'
                AND COLUMN_NAME IN ('ERROR_NUMBER', 'LAST_SEEN'))
        )
      ORDER BY FIELD(TABLE_NAME, $order_tables), ORDINAL_POSITION;"

expect_output \
    "Performance Schema error summary statistics" \
    "$(printf '%b' 'events_errors_summary_by_account_by_error\tACCOUNT\t0\t1\tUSER\t1\t1\tHASH\tYES\n' \
        'events_errors_summary_by_account_by_error\tACCOUNT\t0\t2\tHOST\t1\t1\tHASH\tYES\n' \
        'events_errors_summary_by_account_by_error\tACCOUNT\t0\t3\tERROR_NUMBER\t1\t1\tHASH\tYES\n' \
        'events_errors_summary_by_host_by_error\tHOST\t0\t1\tHOST\t1\t1\tHASH\tYES\n' \
        'events_errors_summary_by_host_by_error\tHOST\t0\t2\tERROR_NUMBER\t1\t1\tHASH\tYES\n' \
        'events_errors_summary_by_thread_by_error\tTHREAD_ID\t0\t1\tTHREAD_ID\t1\t1\tHASH\tYES\n' \
        'events_errors_summary_by_thread_by_error\tTHREAD_ID\t0\t2\tERROR_NUMBER\t1\t1\tHASH\tYES\n' \
        'events_errors_summary_by_user_by_error\tUSER\t0\t1\tUSER\t1\t1\tHASH\tYES\n' \
        'events_errors_summary_by_user_by_error\tUSER\t0\t2\tERROR_NUMBER\t1\t1\tHASH\tYES\n' \
        'events_errors_summary_global_by_error\tERROR_NUMBER\t0\t1\tERROR_NUMBER\t1\t1\tHASH\tYES')" \
    "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
            COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema error summary constraints" \
    "$(printf '%b' 'events_errors_summary_by_account_by_error\tACCOUNT\tUNIQUE\tYES\n' \
        'events_errors_summary_by_host_by_error\tHOST\tUNIQUE\tYES\n' \
        'events_errors_summary_by_thread_by_error\tTHREAD_ID\tUNIQUE\tYES\n' \
        'events_errors_summary_by_user_by_error\tUSER\tUNIQUE\tYES\n' \
        'events_errors_summary_global_by_error\tERROR_NUMBER\tUNIQUE\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME;"

expect_output \
    "Performance Schema error summary key usage" \
    "$(printf '%b' 'events_errors_summary_by_account_by_error\tACCOUNT\tUSER\t1\n' \
        'events_errors_summary_by_account_by_error\tACCOUNT\tHOST\t2\n' \
        'events_errors_summary_by_account_by_error\tACCOUNT\tERROR_NUMBER\t3\n' \
        'events_errors_summary_by_host_by_error\tHOST\tHOST\t1\n' \
        'events_errors_summary_by_host_by_error\tHOST\tERROR_NUMBER\t2\n' \
        'events_errors_summary_by_thread_by_error\tTHREAD_ID\tTHREAD_ID\t1\n' \
        'events_errors_summary_by_thread_by_error\tTHREAD_ID\tERROR_NUMBER\t2\n' \
        'events_errors_summary_by_user_by_error\tUSER\tUSER\t1\n' \
        'events_errors_summary_by_user_by_error\tUSER\tERROR_NUMBER\t2\n' \
        'events_errors_summary_global_by_error\tERROR_NUMBER\tERROR_NUMBER\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema error summary constraint extensions" \
    "$(printf '%b' 'events_errors_summary_by_account_by_error\tACCOUNT\t1\t1\n' \
        'events_errors_summary_by_host_by_error\tHOST\t1\t1\n' \
        'events_errors_summary_by_thread_by_error\tTHREAD_ID\t1\t1\n' \
        'events_errors_summary_by_user_by_error\tUSER\t1\t1\n' \
        'events_errors_summary_global_by_error\tERROR_NUMBER\t1\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME,
            ENGINE_ATTRIBUTE IS NULL, SECONDARY_ENGINE_ATTRIBUTE IS NULL
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME;"

expect_output \
    "Performance Schema error summary table metadata" \
    "$(printf '%b' 'events_errors_summary_by_account_by_error\tPERFORMANCE_SCHEMA\tDynamic\t227456\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_errors_summary_by_host_by_error\tPERFORMANCE_SCHEMA\tDynamic\t227456\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_errors_summary_by_thread_by_error\tPERFORMANCE_SCHEMA\tDynamic\t454912\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_errors_summary_by_user_by_error\tPERFORMANCE_SCHEMA\tDynamic\t227456\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_errors_summary_global_by_error\tPERFORMANCE_SCHEMA\tDynamic\t5556\tNULL\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS,
            COALESCE(AUTO_INCREMENT, 'NULL'), TABLE_COLLATION
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables);"

printf '%s\n' "mysql_baseline_performance_schema_error_summary_placeholders_expectations: ok"
