#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_statement_event_placeholders_expectations: $1" >&2
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

tables="'events_statements_current',
        'events_statements_histogram_by_digest',
        'events_statements_histogram_global',
        'events_statements_history',
        'events_statements_summary_by_account_by_event_name',
        'events_statements_summary_by_digest',
        'events_statements_summary_by_host_by_event_name',
        'events_statements_summary_by_thread_by_event_name',
        'events_statements_summary_by_user_by_event_name',
        'events_statements_summary_global_by_event_name'"

order_tables="'events_statements_current',
              'events_statements_histogram_by_digest',
              'events_statements_histogram_global',
              'events_statements_history',
              'events_statements_summary_by_account_by_event_name',
              'events_statements_summary_by_digest',
              'events_statements_summary_by_host_by_event_name',
              'events_statements_summary_by_thread_by_event_name',
              'events_statements_summary_by_user_by_event_name',
              'events_statements_summary_global_by_event_name'"

expect_output \
    "Performance Schema statement event column counts" \
    "$(printf '%b' 'events_statements_current\t46\n' \
        'events_statements_histogram_by_digest\t8\n' \
        'events_statements_histogram_global\t6\n' \
        'events_statements_history\t46\n' \
        'events_statements_summary_by_account_by_event_name\t31\n' \
        'events_statements_summary_by_digest\t39\n' \
        'events_statements_summary_by_host_by_event_name\t30\n' \
        'events_statements_summary_by_thread_by_event_name\t30\n' \
        'events_statements_summary_by_user_by_event_name\t30\n' \
        'events_statements_summary_global_by_event_name\t29')" \
    "SELECT TABLE_NAME, COUNT(*)
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      GROUP BY TABLE_NAME
      ORDER BY FIELD(TABLE_NAME, $order_tables);"

expect_output \
    "Performance Schema statement event representative columns" \
    "$(printf '%b' 'events_statements_current\tTHREAD_ID\t1\tNO\tbigint unsigned\tPRI\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'events_statements_current\tEVENT_ID\t2\tNO\tbigint unsigned\tPRI\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'events_statements_current\tEXECUTION_ENGINE\t46\tYES\tenum('\''PRIMARY'\'','\''SECONDARY'\'')\t\tutf8mb4_0900_ai_ci\tutf8mb4\t9\tNULL\tNULL\tNULL\n' \
        'events_statements_histogram_by_digest\tSCHEMA_NAME\t1\tYES\tvarchar(64)\tMUL\tutf8mb4_0900_ai_ci\tutf8mb4\t64\tNULL\tNULL\tNULL\n' \
        'events_statements_histogram_by_digest\tBUCKET_QUANTILE\t8\tNO\tdouble(7,6)\t\tNULL\tNULL\tNULL\t7\t6\tNULL\n' \
        'events_statements_histogram_global\tBUCKET_NUMBER\t1\tNO\tint unsigned\tPRI\tNULL\tNULL\tNULL\t10\t0\tNULL\n' \
        'events_statements_history\tSQL_TEXT\t10\tYES\tlongtext\t\tutf8mb4_0900_ai_ci\tutf8mb4\t4294967295\tNULL\tNULL\tNULL\n' \
        'events_statements_summary_by_account_by_event_name\tUSER\t1\tYES\tchar(32)\tMUL\tutf8mb4_bin\tutf8mb4\t32\tNULL\tNULL\tNULL\n' \
        'events_statements_summary_by_account_by_event_name\tCOUNT_SECONDARY\t31\tNO\tbigint unsigned\t\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'events_statements_summary_by_digest\tFIRST_SEEN\t32\tNO\ttimestamp(6)\t\tNULL\tNULL\tNULL\tNULL\tNULL\t6\n' \
        'events_statements_summary_by_digest\tQUERY_SAMPLE_TEXT\t37\tYES\tlongtext\t\tutf8mb4_0900_ai_ci\tutf8mb4\t4294967295\tNULL\tNULL\tNULL\n' \
        'events_statements_summary_by_host_by_event_name\tHOST\t1\tYES\tchar(255)\tMUL\tascii_general_ci\tascii\t255\tNULL\tNULL\tNULL\n' \
        'events_statements_summary_by_thread_by_event_name\tTHREAD_ID\t1\tNO\tbigint unsigned\tPRI\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'events_statements_summary_by_user_by_event_name\tUSER\t1\tYES\tchar(32)\tMUL\tutf8mb4_bin\tutf8mb4\t32\tNULL\tNULL\tNULL\n' \
        'events_statements_summary_global_by_event_name\tEVENT_NAME\t1\tNO\tvarchar(128)\tPRI\tutf8mb4_0900_ai_ci\tutf8mb4\t128\tNULL\tNULL\tNULL')" \
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
            (TABLE_NAME = 'events_statements_current'
             AND COLUMN_NAME IN ('THREAD_ID', 'EVENT_ID', 'EXECUTION_ENGINE'))
            OR (TABLE_NAME = 'events_statements_histogram_by_digest'
                AND COLUMN_NAME IN ('SCHEMA_NAME', 'BUCKET_QUANTILE'))
            OR (TABLE_NAME = 'events_statements_histogram_global'
                AND COLUMN_NAME = 'BUCKET_NUMBER')
            OR (TABLE_NAME = 'events_statements_history'
                AND COLUMN_NAME = 'SQL_TEXT')
            OR (TABLE_NAME = 'events_statements_summary_by_account_by_event_name'
                AND COLUMN_NAME IN ('USER', 'COUNT_SECONDARY'))
            OR (TABLE_NAME = 'events_statements_summary_by_digest'
                AND COLUMN_NAME IN ('FIRST_SEEN', 'QUERY_SAMPLE_TEXT'))
            OR (TABLE_NAME = 'events_statements_summary_by_host_by_event_name'
                AND COLUMN_NAME = 'HOST')
            OR (TABLE_NAME = 'events_statements_summary_by_thread_by_event_name'
                AND COLUMN_NAME = 'THREAD_ID')
            OR (TABLE_NAME = 'events_statements_summary_by_user_by_event_name'
                AND COLUMN_NAME = 'USER')
            OR (TABLE_NAME = 'events_statements_summary_global_by_event_name'
                AND COLUMN_NAME = 'EVENT_NAME')
        )
      ORDER BY FIELD(TABLE_NAME, $order_tables), ORDINAL_POSITION;"

expect_output \
    "Performance Schema statement event statistics" \
    "$(printf '%b' 'events_statements_current\tPRIMARY\t0\t1\tTHREAD_ID\t1\t1\tHASH\tYES\n' \
        'events_statements_current\tPRIMARY\t0\t2\tEVENT_ID\t1\t1\tHASH\tYES\n' \
        'events_statements_histogram_by_digest\tSCHEMA_NAME\t0\t1\tSCHEMA_NAME\t1\t1\tHASH\tYES\n' \
        'events_statements_histogram_by_digest\tSCHEMA_NAME\t0\t2\tDIGEST\t1\t1\tHASH\tYES\n' \
        'events_statements_histogram_by_digest\tSCHEMA_NAME\t0\t3\tBUCKET_NUMBER\t1\t1\tHASH\tYES\n' \
        'events_statements_histogram_global\tPRIMARY\t0\t1\tBUCKET_NUMBER\t1\t1\tHASH\tYES\n' \
        'events_statements_history\tPRIMARY\t0\t1\tTHREAD_ID\t1\t1\tHASH\tYES\n' \
        'events_statements_history\tPRIMARY\t0\t2\tEVENT_ID\t1\t1\tHASH\tYES\n' \
        'events_statements_summary_by_account_by_event_name\tACCOUNT\t0\t1\tUSER\t1\t1\tHASH\tYES\n' \
        'events_statements_summary_by_account_by_event_name\tACCOUNT\t0\t2\tHOST\t1\t1\tHASH\tYES\n' \
        'events_statements_summary_by_account_by_event_name\tACCOUNT\t0\t3\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'events_statements_summary_by_digest\tSCHEMA_NAME\t0\t1\tSCHEMA_NAME\t1\t1\tHASH\tYES\n' \
        'events_statements_summary_by_digest\tSCHEMA_NAME\t0\t2\tDIGEST\t1\t1\tHASH\tYES\n' \
        'events_statements_summary_by_host_by_event_name\tHOST\t0\t1\tHOST\t1\t1\tHASH\tYES\n' \
        'events_statements_summary_by_host_by_event_name\tHOST\t0\t2\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'events_statements_summary_by_thread_by_event_name\tPRIMARY\t0\t1\tTHREAD_ID\t1\t1\tHASH\tYES\n' \
        'events_statements_summary_by_thread_by_event_name\tPRIMARY\t0\t2\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'events_statements_summary_by_user_by_event_name\tUSER\t0\t1\tUSER\t1\t1\tHASH\tYES\n' \
        'events_statements_summary_by_user_by_event_name\tUSER\t0\t2\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'events_statements_summary_global_by_event_name\tPRIMARY\t0\t1\tEVENT_NAME\t1\t1\tHASH\tYES')" \
    "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
            COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema statement event constraints" \
    "$(printf '%b' 'events_statements_current\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'events_statements_histogram_by_digest\tSCHEMA_NAME\tUNIQUE\tYES\n' \
        'events_statements_histogram_global\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'events_statements_history\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'events_statements_summary_by_account_by_event_name\tACCOUNT\tUNIQUE\tYES\n' \
        'events_statements_summary_by_digest\tSCHEMA_NAME\tUNIQUE\tYES\n' \
        'events_statements_summary_by_host_by_event_name\tHOST\tUNIQUE\tYES\n' \
        'events_statements_summary_by_thread_by_event_name\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'events_statements_summary_by_user_by_event_name\tUSER\tUNIQUE\tYES\n' \
        'events_statements_summary_global_by_event_name\tPRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME;"

expect_output \
    "Performance Schema statement event key usage" \
    "$(printf '%b' 'events_statements_current\tPRIMARY\tTHREAD_ID\t1\n' \
        'events_statements_current\tPRIMARY\tEVENT_ID\t2\n' \
        'events_statements_histogram_by_digest\tSCHEMA_NAME\tSCHEMA_NAME\t1\n' \
        'events_statements_histogram_by_digest\tSCHEMA_NAME\tDIGEST\t2\n' \
        'events_statements_histogram_by_digest\tSCHEMA_NAME\tBUCKET_NUMBER\t3\n' \
        'events_statements_histogram_global\tPRIMARY\tBUCKET_NUMBER\t1\n' \
        'events_statements_history\tPRIMARY\tTHREAD_ID\t1\n' \
        'events_statements_history\tPRIMARY\tEVENT_ID\t2\n' \
        'events_statements_summary_by_account_by_event_name\tACCOUNT\tUSER\t1\n' \
        'events_statements_summary_by_account_by_event_name\tACCOUNT\tHOST\t2\n' \
        'events_statements_summary_by_account_by_event_name\tACCOUNT\tEVENT_NAME\t3\n' \
        'events_statements_summary_by_digest\tSCHEMA_NAME\tSCHEMA_NAME\t1\n' \
        'events_statements_summary_by_digest\tSCHEMA_NAME\tDIGEST\t2\n' \
        'events_statements_summary_by_host_by_event_name\tHOST\tHOST\t1\n' \
        'events_statements_summary_by_host_by_event_name\tHOST\tEVENT_NAME\t2\n' \
        'events_statements_summary_by_thread_by_event_name\tPRIMARY\tTHREAD_ID\t1\n' \
        'events_statements_summary_by_thread_by_event_name\tPRIMARY\tEVENT_NAME\t2\n' \
        'events_statements_summary_by_user_by_event_name\tUSER\tUSER\t1\n' \
        'events_statements_summary_by_user_by_event_name\tUSER\tEVENT_NAME\t2\n' \
        'events_statements_summary_global_by_event_name\tPRIMARY\tEVENT_NAME\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema statement event constraint extensions" \
    "$(printf '%b' 'events_statements_current\tPRIMARY\t1\t1\n' \
        'events_statements_histogram_by_digest\tSCHEMA_NAME\t1\t1\n' \
        'events_statements_histogram_global\tPRIMARY\t1\t1\n' \
        'events_statements_history\tPRIMARY\t1\t1\n' \
        'events_statements_summary_by_account_by_event_name\tACCOUNT\t1\t1\n' \
        'events_statements_summary_by_digest\tSCHEMA_NAME\t1\t1\n' \
        'events_statements_summary_by_host_by_event_name\tHOST\t1\t1\n' \
        'events_statements_summary_by_thread_by_event_name\tPRIMARY\t1\t1\n' \
        'events_statements_summary_by_user_by_event_name\tUSER\t1\t1\n' \
        'events_statements_summary_global_by_event_name\tPRIMARY\t1\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME,
            ENGINE_ATTRIBUTE IS NULL, SECONDARY_ENGINE_ATTRIBUTE IS NULL
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME;"

expect_output \
    "Performance Schema statement event table metadata" \
    "$(printf '%b' 'events_statements_current\tPERFORMANCE_SCHEMA\tDynamic\t2560\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_statements_histogram_by_digest\tPERFORMANCE_SCHEMA\tDynamic\t10000\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_statements_histogram_global\tPERFORMANCE_SCHEMA\tFixed\t450\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_statements_history\tPERFORMANCE_SCHEMA\tDynamic\t2560\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_statements_summary_by_account_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t28160\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_statements_summary_by_digest\tPERFORMANCE_SCHEMA\tDynamic\t10000\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_statements_summary_by_host_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t28160\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_statements_summary_by_thread_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t56320\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_statements_summary_by_user_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t28160\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_statements_summary_global_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t220\tNULL\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS,
            COALESCE(AUTO_INCREMENT, 'NULL'), TABLE_COLLATION
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables);"

printf '%s\n' "mysql_baseline_performance_schema_statement_event_placeholders_expectations: ok"
