#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_memory_summary_placeholders_expectations: $1" >&2
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

tables="'memory_summary_by_account_by_event_name',
        'memory_summary_by_host_by_event_name',
        'memory_summary_by_thread_by_event_name',
        'memory_summary_by_user_by_event_name',
        'memory_summary_global_by_event_name'"

order_tables="'memory_summary_by_account_by_event_name',
              'memory_summary_by_host_by_event_name',
              'memory_summary_by_thread_by_event_name',
              'memory_summary_by_user_by_event_name',
              'memory_summary_global_by_event_name'"

expect_output \
    "Performance Schema memory summary nonempty rows" \
    "$(printf '%b' 'memory_summary_by_account_by_event_name\t1\n' \
        'memory_summary_by_host_by_event_name\t1\n' \
        'memory_summary_by_thread_by_event_name\t1\n' \
        'memory_summary_by_user_by_event_name\t1\n' \
        'memory_summary_global_by_event_name\t1')" \
    "SELECT 'memory_summary_by_account_by_event_name',
            COUNT(*) > 0
       FROM performance_schema.memory_summary_by_account_by_event_name
      UNION ALL
     SELECT 'memory_summary_by_host_by_event_name',
            COUNT(*) > 0
       FROM performance_schema.memory_summary_by_host_by_event_name
      UNION ALL
     SELECT 'memory_summary_by_thread_by_event_name',
            COUNT(*) > 0
       FROM performance_schema.memory_summary_by_thread_by_event_name
      UNION ALL
     SELECT 'memory_summary_by_user_by_event_name',
            COUNT(*) > 0
       FROM performance_schema.memory_summary_by_user_by_event_name
      UNION ALL
     SELECT 'memory_summary_global_by_event_name',
            COUNT(*) > 0
       FROM performance_schema.memory_summary_global_by_event_name;"

expect_output \
    "Performance Schema memory summary column counts" \
    "$(printf '%b' 'memory_summary_by_account_by_event_name\t13\n' \
        'memory_summary_by_host_by_event_name\t12\n' \
        'memory_summary_by_thread_by_event_name\t12\n' \
        'memory_summary_by_user_by_event_name\t12\n' \
        'memory_summary_global_by_event_name\t11')" \
    "SELECT TABLE_NAME, COUNT(*)
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      GROUP BY TABLE_NAME
      ORDER BY FIELD(TABLE_NAME, $order_tables);"

expect_output \
    "Performance Schema memory summary representative columns" \
    "$(printf '%b' 'memory_summary_by_account_by_event_name\tUSER\t1\tYES\tchar(32)\tMUL\tutf8mb4_bin\tutf8mb4\t32\tNULL\tNULL\tNULL\n' \
        'memory_summary_by_account_by_event_name\tEVENT_NAME\t3\tNO\tvarchar(128)\t\tutf8mb4_0900_ai_ci\tutf8mb4\t128\tNULL\tNULL\tNULL\n' \
        'memory_summary_by_account_by_event_name\tCURRENT_NUMBER_OF_BYTES_USED\t12\tNO\tbigint\t\tNULL\tNULL\tNULL\t19\t0\tNULL\n' \
        'memory_summary_by_thread_by_event_name\tTHREAD_ID\t1\tNO\tbigint unsigned\tPRI\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'memory_summary_by_thread_by_event_name\tEVENT_NAME\t2\tNO\tvarchar(128)\tPRI\tutf8mb4_0900_ai_ci\tutf8mb4\t128\tNULL\tNULL\tNULL\n' \
        'memory_summary_global_by_event_name\tEVENT_NAME\t1\tNO\tvarchar(128)\tPRI\tutf8mb4_0900_ai_ci\tutf8mb4\t128\tNULL\tNULL\tNULL')" \
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
            (TABLE_NAME = 'memory_summary_by_account_by_event_name'
             AND COLUMN_NAME IN ('USER', 'EVENT_NAME', 'CURRENT_NUMBER_OF_BYTES_USED'))
            OR (TABLE_NAME = 'memory_summary_by_thread_by_event_name'
                AND COLUMN_NAME IN ('THREAD_ID', 'EVENT_NAME'))
            OR (TABLE_NAME = 'memory_summary_global_by_event_name'
                AND COLUMN_NAME = 'EVENT_NAME')
        )
      ORDER BY FIELD(TABLE_NAME, $order_tables), ORDINAL_POSITION;"

expect_output \
    "Performance Schema memory summary statistics" \
    "$(printf '%b' 'memory_summary_by_account_by_event_name\tACCOUNT\t0\t1\tUSER\t1\t1\tHASH\tYES\n' \
        'memory_summary_by_account_by_event_name\tACCOUNT\t0\t2\tHOST\t1\t1\tHASH\tYES\n' \
        'memory_summary_by_account_by_event_name\tACCOUNT\t0\t3\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'memory_summary_by_host_by_event_name\tHOST\t0\t1\tHOST\t1\t1\tHASH\tYES\n' \
        'memory_summary_by_host_by_event_name\tHOST\t0\t2\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'memory_summary_by_thread_by_event_name\tPRIMARY\t0\t1\tTHREAD_ID\t1\t1\tHASH\tYES\n' \
        'memory_summary_by_thread_by_event_name\tPRIMARY\t0\t2\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'memory_summary_by_user_by_event_name\tUSER\t0\t1\tUSER\t1\t1\tHASH\tYES\n' \
        'memory_summary_by_user_by_event_name\tUSER\t0\t2\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'memory_summary_global_by_event_name\tPRIMARY\t0\t1\tEVENT_NAME\t1\t1\tHASH\tYES')" \
    "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
            COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema memory summary constraints" \
    "$(printf '%b' 'memory_summary_by_account_by_event_name\tACCOUNT\tUNIQUE\tYES\n' \
        'memory_summary_by_host_by_event_name\tHOST\tUNIQUE\tYES\n' \
        'memory_summary_by_thread_by_event_name\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'memory_summary_by_user_by_event_name\tUSER\tUNIQUE\tYES\n' \
        'memory_summary_global_by_event_name\tPRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME;"

expect_output \
    "Performance Schema memory summary key usage" \
    "$(printf '%b' 'memory_summary_by_account_by_event_name\tACCOUNT\tUSER\t1\n' \
        'memory_summary_by_account_by_event_name\tACCOUNT\tHOST\t2\n' \
        'memory_summary_by_account_by_event_name\tACCOUNT\tEVENT_NAME\t3\n' \
        'memory_summary_by_host_by_event_name\tHOST\tHOST\t1\n' \
        'memory_summary_by_host_by_event_name\tHOST\tEVENT_NAME\t2\n' \
        'memory_summary_by_thread_by_event_name\tPRIMARY\tTHREAD_ID\t1\n' \
        'memory_summary_by_thread_by_event_name\tPRIMARY\tEVENT_NAME\t2\n' \
        'memory_summary_by_user_by_event_name\tUSER\tUSER\t1\n' \
        'memory_summary_by_user_by_event_name\tUSER\tEVENT_NAME\t2\n' \
        'memory_summary_global_by_event_name\tPRIMARY\tEVENT_NAME\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema memory summary constraint extensions" \
    "$(printf '%b' 'memory_summary_by_account_by_event_name\tACCOUNT\t1\t1\n' \
        'memory_summary_by_host_by_event_name\tHOST\t1\t1\n' \
        'memory_summary_by_thread_by_event_name\tPRIMARY\t1\t1\n' \
        'memory_summary_by_user_by_event_name\tUSER\t1\t1\n' \
        'memory_summary_global_by_event_name\tPRIMARY\t1\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME,
            ENGINE_ATTRIBUTE IS NULL, SECONDARY_ENGINE_ATTRIBUTE IS NULL
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME;"

expect_output \
    "Performance Schema memory summary table metadata" \
    "$(printf '%b' 'memory_summary_by_account_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t60160\tNULL\tutf8mb4_0900_ai_ci\n' \
        'memory_summary_by_host_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t60160\tNULL\tutf8mb4_0900_ai_ci\n' \
        'memory_summary_by_thread_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t120320\tNULL\tutf8mb4_0900_ai_ci\n' \
        'memory_summary_by_user_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t60160\tNULL\tutf8mb4_0900_ai_ci\n' \
        'memory_summary_global_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t470\tNULL\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS,
            COALESCE(AUTO_INCREMENT, 'NULL'), TABLE_COLLATION
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables);"

printf '%s\n' "mysql_baseline_performance_schema_memory_summary_placeholders_expectations: ok"
