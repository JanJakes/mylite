#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_stage_wait_summary_placeholders_expectations: $1" >&2
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

tables="'events_stages_summary_by_account_by_event_name',
        'events_stages_summary_by_host_by_event_name',
        'events_stages_summary_by_thread_by_event_name',
        'events_stages_summary_by_user_by_event_name',
        'events_stages_summary_global_by_event_name',
        'events_waits_summary_by_account_by_event_name',
        'events_waits_summary_by_host_by_event_name',
        'events_waits_summary_by_instance',
        'events_waits_summary_by_thread_by_event_name',
        'events_waits_summary_by_user_by_event_name',
        'events_waits_summary_global_by_event_name'"

order_tables="'events_stages_summary_by_account_by_event_name',
              'events_stages_summary_by_host_by_event_name',
              'events_stages_summary_by_thread_by_event_name',
              'events_stages_summary_by_user_by_event_name',
              'events_stages_summary_global_by_event_name',
              'events_waits_summary_by_account_by_event_name',
              'events_waits_summary_by_host_by_event_name',
              'events_waits_summary_by_instance',
              'events_waits_summary_by_thread_by_event_name',
              'events_waits_summary_by_user_by_event_name',
              'events_waits_summary_global_by_event_name'"

expect_output \
    "Performance Schema stage/wait summary nonempty rows" \
    "$(printf '%b' 'events_stages_summary_by_account_by_event_name\t1\n' \
        'events_stages_summary_by_host_by_event_name\t1\n' \
        'events_stages_summary_by_thread_by_event_name\t1\n' \
        'events_stages_summary_by_user_by_event_name\t1\n' \
        'events_stages_summary_global_by_event_name\t1\n' \
        'events_waits_summary_by_account_by_event_name\t1\n' \
        'events_waits_summary_by_host_by_event_name\t1\n' \
        'events_waits_summary_by_instance\t1\n' \
        'events_waits_summary_by_thread_by_event_name\t1\n' \
        'events_waits_summary_by_user_by_event_name\t1\n' \
        'events_waits_summary_global_by_event_name\t1')" \
    "SELECT 'events_stages_summary_by_account_by_event_name',
            COUNT(*) > 0
       FROM performance_schema.events_stages_summary_by_account_by_event_name
      UNION ALL
     SELECT 'events_stages_summary_by_host_by_event_name',
            COUNT(*) > 0
       FROM performance_schema.events_stages_summary_by_host_by_event_name
      UNION ALL
     SELECT 'events_stages_summary_by_thread_by_event_name',
            COUNT(*) > 0
       FROM performance_schema.events_stages_summary_by_thread_by_event_name
      UNION ALL
     SELECT 'events_stages_summary_by_user_by_event_name',
            COUNT(*) > 0
       FROM performance_schema.events_stages_summary_by_user_by_event_name
      UNION ALL
     SELECT 'events_stages_summary_global_by_event_name',
            COUNT(*) > 0
       FROM performance_schema.events_stages_summary_global_by_event_name
      UNION ALL
     SELECT 'events_waits_summary_by_account_by_event_name',
            COUNT(*) > 0
       FROM performance_schema.events_waits_summary_by_account_by_event_name
      UNION ALL
     SELECT 'events_waits_summary_by_host_by_event_name',
            COUNT(*) > 0
       FROM performance_schema.events_waits_summary_by_host_by_event_name
      UNION ALL
     SELECT 'events_waits_summary_by_instance',
            COUNT(*) > 0
       FROM performance_schema.events_waits_summary_by_instance
      UNION ALL
     SELECT 'events_waits_summary_by_thread_by_event_name',
            COUNT(*) > 0
       FROM performance_schema.events_waits_summary_by_thread_by_event_name
      UNION ALL
     SELECT 'events_waits_summary_by_user_by_event_name',
            COUNT(*) > 0
       FROM performance_schema.events_waits_summary_by_user_by_event_name
      UNION ALL
     SELECT 'events_waits_summary_global_by_event_name',
            COUNT(*) > 0
       FROM performance_schema.events_waits_summary_global_by_event_name;"

expect_output \
    "Performance Schema stage/wait summary column counts" \
    "$(printf '%b' 'events_stages_summary_by_account_by_event_name\t8\n' \
        'events_stages_summary_by_host_by_event_name\t7\n' \
        'events_stages_summary_by_thread_by_event_name\t7\n' \
        'events_stages_summary_by_user_by_event_name\t7\n' \
        'events_stages_summary_global_by_event_name\t6\n' \
        'events_waits_summary_by_account_by_event_name\t8\n' \
        'events_waits_summary_by_host_by_event_name\t7\n' \
        'events_waits_summary_by_instance\t7\n' \
        'events_waits_summary_by_thread_by_event_name\t7\n' \
        'events_waits_summary_by_user_by_event_name\t7\n' \
        'events_waits_summary_global_by_event_name\t6')" \
    "SELECT TABLE_NAME, COUNT(*)
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      GROUP BY TABLE_NAME
      ORDER BY FIELD(TABLE_NAME, $order_tables);"

expect_output \
    "Performance Schema stage/wait summary representative columns" \
    "$(printf '%b' 'events_stages_summary_by_account_by_event_name\tUSER\t1\tYES\tchar(32)\tMUL\tutf8mb4_bin\tutf8mb4\t32\tNULL\tNULL\tNULL\n' \
        'events_stages_summary_by_account_by_event_name\tHOST\t2\tYES\tchar(255)\t\tascii_general_ci\tascii\t255\tNULL\tNULL\tNULL\n' \
        'events_stages_summary_by_account_by_event_name\tCOUNT_STAR\t4\tNO\tbigint unsigned\t\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'events_stages_summary_by_thread_by_event_name\tTHREAD_ID\t1\tNO\tbigint unsigned\tPRI\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'events_stages_summary_global_by_event_name\tEVENT_NAME\t1\tNO\tvarchar(128)\tPRI\tutf8mb4_0900_ai_ci\tutf8mb4\t128\tNULL\tNULL\tNULL\n' \
        'events_waits_summary_by_account_by_event_name\tUSER\t1\tYES\tchar(32)\tMUL\tutf8mb4_bin\tutf8mb4\t32\tNULL\tNULL\tNULL\n' \
        'events_waits_summary_by_instance\tEVENT_NAME\t1\tNO\tvarchar(128)\tMUL\tutf8mb4_0900_ai_ci\tutf8mb4\t128\tNULL\tNULL\tNULL\n' \
        'events_waits_summary_by_instance\tOBJECT_INSTANCE_BEGIN\t2\tNO\tbigint unsigned\tPRI\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'events_waits_summary_by_thread_by_event_name\tTHREAD_ID\t1\tNO\tbigint unsigned\tPRI\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'events_waits_summary_global_by_event_name\tMAX_TIMER_WAIT\t6\tNO\tbigint unsigned\t\tNULL\tNULL\tNULL\t20\t0\tNULL')" \
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
            (TABLE_NAME = 'events_stages_summary_by_account_by_event_name'
             AND COLUMN_NAME IN ('USER', 'HOST', 'COUNT_STAR'))
            OR (TABLE_NAME = 'events_stages_summary_by_thread_by_event_name'
                AND COLUMN_NAME = 'THREAD_ID')
            OR (TABLE_NAME = 'events_stages_summary_global_by_event_name'
                AND COLUMN_NAME = 'EVENT_NAME')
            OR (TABLE_NAME = 'events_waits_summary_by_account_by_event_name'
                AND COLUMN_NAME = 'USER')
            OR (TABLE_NAME = 'events_waits_summary_by_instance'
                AND COLUMN_NAME IN ('EVENT_NAME', 'OBJECT_INSTANCE_BEGIN'))
            OR (TABLE_NAME = 'events_waits_summary_by_thread_by_event_name'
                AND COLUMN_NAME = 'THREAD_ID')
            OR (TABLE_NAME = 'events_waits_summary_global_by_event_name'
                AND COLUMN_NAME = 'MAX_TIMER_WAIT')
        )
      ORDER BY FIELD(TABLE_NAME, $order_tables), ORDINAL_POSITION;"

expect_output \
    "Performance Schema stage/wait summary statistics" \
    "$(printf '%b' 'events_stages_summary_by_account_by_event_name\tACCOUNT\t0\t1\tUSER\t1\t1\tHASH\tYES\n' \
        'events_stages_summary_by_account_by_event_name\tACCOUNT\t0\t2\tHOST\t1\t1\tHASH\tYES\n' \
        'events_stages_summary_by_account_by_event_name\tACCOUNT\t0\t3\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'events_stages_summary_by_host_by_event_name\tHOST\t0\t1\tHOST\t1\t1\tHASH\tYES\n' \
        'events_stages_summary_by_host_by_event_name\tHOST\t0\t2\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'events_stages_summary_by_thread_by_event_name\tPRIMARY\t0\t1\tTHREAD_ID\t1\t1\tHASH\tYES\n' \
        'events_stages_summary_by_thread_by_event_name\tPRIMARY\t0\t2\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'events_stages_summary_by_user_by_event_name\tUSER\t0\t1\tUSER\t1\t1\tHASH\tYES\n' \
        'events_stages_summary_by_user_by_event_name\tUSER\t0\t2\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'events_stages_summary_global_by_event_name\tPRIMARY\t0\t1\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'events_waits_summary_by_account_by_event_name\tACCOUNT\t0\t1\tUSER\t1\t1\tHASH\tYES\n' \
        'events_waits_summary_by_account_by_event_name\tACCOUNT\t0\t2\tHOST\t1\t1\tHASH\tYES\n' \
        'events_waits_summary_by_account_by_event_name\tACCOUNT\t0\t3\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'events_waits_summary_by_host_by_event_name\tHOST\t0\t1\tHOST\t1\t1\tHASH\tYES\n' \
        'events_waits_summary_by_host_by_event_name\tHOST\t0\t2\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'events_waits_summary_by_instance\tEVENT_NAME\t1\t1\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'events_waits_summary_by_instance\tPRIMARY\t0\t1\tOBJECT_INSTANCE_BEGIN\t1\t1\tHASH\tYES\n' \
        'events_waits_summary_by_thread_by_event_name\tPRIMARY\t0\t1\tTHREAD_ID\t1\t1\tHASH\tYES\n' \
        'events_waits_summary_by_thread_by_event_name\tPRIMARY\t0\t2\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'events_waits_summary_by_user_by_event_name\tUSER\t0\t1\tUSER\t1\t1\tHASH\tYES\n' \
        'events_waits_summary_by_user_by_event_name\tUSER\t0\t2\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'events_waits_summary_global_by_event_name\tPRIMARY\t0\t1\tEVENT_NAME\t1\t1\tHASH\tYES')" \
    "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
            COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema stage/wait summary constraints" \
    "$(printf '%b' 'events_stages_summary_by_account_by_event_name\tACCOUNT\tUNIQUE\tYES\n' \
        'events_stages_summary_by_host_by_event_name\tHOST\tUNIQUE\tYES\n' \
        'events_stages_summary_by_thread_by_event_name\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'events_stages_summary_by_user_by_event_name\tUSER\tUNIQUE\tYES\n' \
        'events_stages_summary_global_by_event_name\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'events_waits_summary_by_account_by_event_name\tACCOUNT\tUNIQUE\tYES\n' \
        'events_waits_summary_by_host_by_event_name\tHOST\tUNIQUE\tYES\n' \
        'events_waits_summary_by_instance\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'events_waits_summary_by_thread_by_event_name\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'events_waits_summary_by_user_by_event_name\tUSER\tUNIQUE\tYES\n' \
        'events_waits_summary_global_by_event_name\tPRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME;"

expect_output \
    "Performance Schema stage/wait summary key usage" \
    "$(printf '%b' 'events_stages_summary_by_account_by_event_name\tACCOUNT\tUSER\t1\n' \
        'events_stages_summary_by_account_by_event_name\tACCOUNT\tHOST\t2\n' \
        'events_stages_summary_by_account_by_event_name\tACCOUNT\tEVENT_NAME\t3\n' \
        'events_stages_summary_by_host_by_event_name\tHOST\tHOST\t1\n' \
        'events_stages_summary_by_host_by_event_name\tHOST\tEVENT_NAME\t2\n' \
        'events_stages_summary_by_thread_by_event_name\tPRIMARY\tTHREAD_ID\t1\n' \
        'events_stages_summary_by_thread_by_event_name\tPRIMARY\tEVENT_NAME\t2\n' \
        'events_stages_summary_by_user_by_event_name\tUSER\tUSER\t1\n' \
        'events_stages_summary_by_user_by_event_name\tUSER\tEVENT_NAME\t2\n' \
        'events_stages_summary_global_by_event_name\tPRIMARY\tEVENT_NAME\t1\n' \
        'events_waits_summary_by_account_by_event_name\tACCOUNT\tUSER\t1\n' \
        'events_waits_summary_by_account_by_event_name\tACCOUNT\tHOST\t2\n' \
        'events_waits_summary_by_account_by_event_name\tACCOUNT\tEVENT_NAME\t3\n' \
        'events_waits_summary_by_host_by_event_name\tHOST\tHOST\t1\n' \
        'events_waits_summary_by_host_by_event_name\tHOST\tEVENT_NAME\t2\n' \
        'events_waits_summary_by_instance\tPRIMARY\tOBJECT_INSTANCE_BEGIN\t1\n' \
        'events_waits_summary_by_thread_by_event_name\tPRIMARY\tTHREAD_ID\t1\n' \
        'events_waits_summary_by_thread_by_event_name\tPRIMARY\tEVENT_NAME\t2\n' \
        'events_waits_summary_by_user_by_event_name\tUSER\tUSER\t1\n' \
        'events_waits_summary_by_user_by_event_name\tUSER\tEVENT_NAME\t2\n' \
        'events_waits_summary_global_by_event_name\tPRIMARY\tEVENT_NAME\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema stage/wait summary constraint extensions" \
    "$(printf '%b' 'events_stages_summary_by_account_by_event_name\tACCOUNT\t1\t1\n' \
        'events_stages_summary_by_host_by_event_name\tHOST\t1\t1\n' \
        'events_stages_summary_by_thread_by_event_name\tPRIMARY\t1\t1\n' \
        'events_stages_summary_by_user_by_event_name\tUSER\t1\t1\n' \
        'events_stages_summary_global_by_event_name\tPRIMARY\t1\t1\n' \
        'events_waits_summary_by_account_by_event_name\tACCOUNT\t1\t1\n' \
        'events_waits_summary_by_host_by_event_name\tHOST\t1\t1\n' \
        'events_waits_summary_by_instance\tEVENT_NAME\t1\t1\n' \
        'events_waits_summary_by_instance\tPRIMARY\t1\t1\n' \
        'events_waits_summary_by_thread_by_event_name\tPRIMARY\t1\t1\n' \
        'events_waits_summary_by_user_by_event_name\tUSER\t1\t1\n' \
        'events_waits_summary_global_by_event_name\tPRIMARY\t1\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME,
            ENGINE_ATTRIBUTE IS NULL, SECONDARY_ENGINE_ATTRIBUTE IS NULL
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME;"

expect_output \
    "Performance Schema stage/wait summary table metadata" \
    "$(printf '%b' 'events_stages_summary_by_account_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t22400\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_stages_summary_by_host_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t22400\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_stages_summary_by_thread_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t44800\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_stages_summary_by_user_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t22400\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_stages_summary_global_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t175\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_waits_summary_by_account_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t88832\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_waits_summary_by_host_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t88832\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_waits_summary_by_instance\tPERFORMANCE_SCHEMA\tDynamic\tpositive\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_waits_summary_by_thread_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t177664\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_waits_summary_by_user_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t88832\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_waits_summary_global_by_event_name\tPERFORMANCE_SCHEMA\tDynamic\t694\tNULL\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_NAME, ENGINE, ROW_FORMAT,
            CASE
                WHEN TABLE_NAME = 'events_waits_summary_by_instance'
                    THEN IF(TABLE_ROWS > 0, 'positive', 'not-positive')
                ELSE CAST(TABLE_ROWS AS CHAR)
            END,
            COALESCE(AUTO_INCREMENT, 'NULL'), TABLE_COLLATION
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables);"

printf '%s\n' "mysql_baseline_performance_schema_stage_wait_summary_placeholders_expectations: ok"
