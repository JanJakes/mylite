#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_event_history_placeholders_expectations: $1" >&2
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

tables="'events_stages_current',
        'events_stages_history',
        'events_stages_history_long',
        'events_statements_history_long',
        'events_statements_summary_by_program',
        'events_waits_current',
        'events_waits_history',
        'events_waits_history_long'"

order_tables="'events_stages_current',
              'events_stages_history',
              'events_stages_history_long',
              'events_statements_history_long',
              'events_statements_summary_by_program',
              'events_waits_current',
              'events_waits_history',
              'events_waits_history_long'"

expect_output \
    "Performance Schema event history row counts" \
    "$(printf '%b' 'events_stages_current\t0\n' \
        'events_stages_history\t0\n' \
        'events_stages_history_long\t0\n' \
        'events_statements_history_long\t0\n' \
        'events_statements_summary_by_program\t0\n' \
        'events_waits_current\t0\n' \
        'events_waits_history\t0\n' \
        'events_waits_history_long\t0')" \
    "SELECT 'events_stages_current', COUNT(*)
       FROM performance_schema.events_stages_current
      UNION ALL
     SELECT 'events_stages_history', COUNT(*)
       FROM performance_schema.events_stages_history
      UNION ALL
     SELECT 'events_stages_history_long', COUNT(*)
       FROM performance_schema.events_stages_history_long
      UNION ALL
     SELECT 'events_statements_history_long', COUNT(*)
       FROM performance_schema.events_statements_history_long
      UNION ALL
     SELECT 'events_statements_summary_by_program', COUNT(*)
       FROM performance_schema.events_statements_summary_by_program
      UNION ALL
     SELECT 'events_waits_current', COUNT(*)
       FROM performance_schema.events_waits_current
      UNION ALL
     SELECT 'events_waits_history', COUNT(*)
       FROM performance_schema.events_waits_history
      UNION ALL
     SELECT 'events_waits_history_long', COUNT(*)
       FROM performance_schema.events_waits_history_long;"

expect_output \
    "Performance Schema event history column counts" \
    "$(printf '%b' 'events_stages_current\t12\n' \
        'events_stages_history\t12\n' \
        'events_stages_history_long\t12\n' \
        'events_statements_history_long\t46\n' \
        'events_statements_summary_by_program\t36\n' \
        'events_waits_current\t19\n' \
        'events_waits_history\t19\n' \
        'events_waits_history_long\t19')" \
    "SELECT TABLE_NAME, COUNT(*)
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      GROUP BY TABLE_NAME
      ORDER BY FIELD(TABLE_NAME, $order_tables);"

expect_output \
    "Performance Schema event history representative columns" \
    "$(printf '%b' 'events_stages_current\tTHREAD_ID\t1\tNO\tbigint unsigned\tPRI\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'events_stages_current\tEVENT_ID\t2\tNO\tbigint unsigned\tPRI\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'events_stages_current\tNESTING_EVENT_TYPE\t12\tYES\tenum('\''TRANSACTION'\'','\''STATEMENT'\'','\''STAGE'\'','\''WAIT'\'')\t\tutf8mb4_0900_ai_ci\tutf8mb4\t11\tNULL\tNULL\tNULL\n' \
        'events_stages_history_long\tTHREAD_ID\t1\tNO\tbigint unsigned\t\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'events_statements_history_long\tLOCK_TIME\t9\tNO\tbigint unsigned\t\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'events_statements_history_long\tSQL_TEXT\t10\tYES\tlongtext\t\tutf8mb4_0900_ai_ci\tutf8mb4\t4294967295\tNULL\tNULL\tNULL\n' \
        'events_statements_history_long\tEXECUTION_ENGINE\t46\tYES\tenum('\''PRIMARY'\'','\''SECONDARY'\'')\t\tutf8mb4_0900_ai_ci\tutf8mb4\t9\tNULL\tNULL\tNULL\n' \
        'events_statements_summary_by_program\tOBJECT_TYPE\t1\tNO\tenum('\''EVENT'\'','\''FUNCTION'\'','\''PROCEDURE'\'','\''TABLE'\'','\''TRIGGER'\'')\tPRI\tutf8mb4_0900_ai_ci\tutf8mb4\t9\tNULL\tNULL\tNULL\n' \
        'events_statements_summary_by_program\tOBJECT_SCHEMA\t2\tNO\tvarchar(64)\tPRI\tutf8mb4_0900_ai_ci\tutf8mb4\t64\tNULL\tNULL\tNULL\n' \
        'events_statements_summary_by_program\tCOUNT_SECONDARY\t36\tNO\tbigint unsigned\t\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'events_waits_current\tOBJECT_NAME\t11\tYES\tvarchar(512)\t\tutf8mb4_0900_ai_ci\tutf8mb4\t512\tNULL\tNULL\tNULL\n' \
        'events_waits_current\tOBJECT_INSTANCE_BEGIN\t14\tNO\tbigint unsigned\t\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'events_waits_current\tNUMBER_OF_BYTES\t18\tYES\tbigint\t\tNULL\tNULL\tNULL\t19\t0\tNULL\n' \
        'events_waits_history_long\tTHREAD_ID\t1\tNO\tbigint unsigned\t\tNULL\tNULL\tNULL\t20\t0\tNULL')" \
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
            (TABLE_NAME = 'events_stages_current'
             AND COLUMN_NAME IN ('THREAD_ID', 'EVENT_ID', 'NESTING_EVENT_TYPE'))
            OR (TABLE_NAME = 'events_stages_history_long'
                AND COLUMN_NAME = 'THREAD_ID')
            OR (TABLE_NAME = 'events_statements_history_long'
                AND COLUMN_NAME IN ('SQL_TEXT', 'LOCK_TIME', 'EXECUTION_ENGINE'))
            OR (TABLE_NAME = 'events_statements_summary_by_program'
                AND COLUMN_NAME IN ('OBJECT_TYPE', 'OBJECT_SCHEMA',
                                    'COUNT_SECONDARY'))
            OR (TABLE_NAME = 'events_waits_current'
                AND COLUMN_NAME IN ('OBJECT_NAME', 'OBJECT_INSTANCE_BEGIN',
                                    'NUMBER_OF_BYTES'))
            OR (TABLE_NAME = 'events_waits_history_long'
                AND COLUMN_NAME = 'THREAD_ID')
        )
      ORDER BY FIELD(TABLE_NAME, $order_tables), ORDINAL_POSITION;"

expect_output \
    "Performance Schema event history statistics" \
    "$(printf '%b' 'events_stages_current\tPRIMARY\t0\t1\tTHREAD_ID\t1\t1\tHASH\tYES\n' \
        'events_stages_current\tPRIMARY\t0\t2\tEVENT_ID\t1\t1\tHASH\tYES\n' \
        'events_stages_history\tPRIMARY\t0\t1\tTHREAD_ID\t1\t1\tHASH\tYES\n' \
        'events_stages_history\tPRIMARY\t0\t2\tEVENT_ID\t1\t1\tHASH\tYES\n' \
        'events_statements_summary_by_program\tPRIMARY\t0\t1\tOBJECT_TYPE\t1\t1\tHASH\tYES\n' \
        'events_statements_summary_by_program\tPRIMARY\t0\t2\tOBJECT_SCHEMA\t1\t1\tHASH\tYES\n' \
        'events_statements_summary_by_program\tPRIMARY\t0\t3\tOBJECT_NAME\t1\t1\tHASH\tYES\n' \
        'events_waits_current\tPRIMARY\t0\t1\tTHREAD_ID\t1\t1\tHASH\tYES\n' \
        'events_waits_current\tPRIMARY\t0\t2\tEVENT_ID\t1\t1\tHASH\tYES\n' \
        'events_waits_history\tPRIMARY\t0\t1\tTHREAD_ID\t1\t1\tHASH\tYES\n' \
        'events_waits_history\tPRIMARY\t0\t2\tEVENT_ID\t1\t1\tHASH\tYES')" \
    "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
            COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema event history constraints" \
    "$(printf '%b' 'events_stages_current\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'events_stages_history\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'events_statements_summary_by_program\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'events_waits_current\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'events_waits_history\tPRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME;"

expect_output \
    "Performance Schema event history key usage" \
    "$(printf '%b' 'events_stages_current\tPRIMARY\tTHREAD_ID\t1\n' \
        'events_stages_current\tPRIMARY\tEVENT_ID\t2\n' \
        'events_stages_history\tPRIMARY\tTHREAD_ID\t1\n' \
        'events_stages_history\tPRIMARY\tEVENT_ID\t2\n' \
        'events_statements_summary_by_program\tPRIMARY\tOBJECT_TYPE\t1\n' \
        'events_statements_summary_by_program\tPRIMARY\tOBJECT_SCHEMA\t2\n' \
        'events_statements_summary_by_program\tPRIMARY\tOBJECT_NAME\t3\n' \
        'events_waits_current\tPRIMARY\tTHREAD_ID\t1\n' \
        'events_waits_current\tPRIMARY\tEVENT_ID\t2\n' \
        'events_waits_history\tPRIMARY\tTHREAD_ID\t1\n' \
        'events_waits_history\tPRIMARY\tEVENT_ID\t2')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema event history constraint extensions" \
    "$(printf '%b' 'events_stages_current\tPRIMARY\t1\t1\n' \
        'events_stages_history\tPRIMARY\t1\t1\n' \
        'events_statements_summary_by_program\tPRIMARY\t1\t1\n' \
        'events_waits_current\tPRIMARY\t1\t1\n' \
        'events_waits_history\tPRIMARY\t1\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME,
            ENGINE_ATTRIBUTE IS NULL, SECONDARY_ENGINE_ATTRIBUTE IS NULL
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME;"

expect_output \
    "Performance Schema event history table metadata" \
    "$(printf '%b' 'events_stages_current\tPERFORMANCE_SCHEMA\tDynamic\t256\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_stages_history\tPERFORMANCE_SCHEMA\tDynamic\t2560\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_stages_history_long\tPERFORMANCE_SCHEMA\tDynamic\t10000\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_statements_history_long\tPERFORMANCE_SCHEMA\tDynamic\t10000\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_statements_summary_by_program\tPERFORMANCE_SCHEMA\tDynamic\t0\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_waits_current\tPERFORMANCE_SCHEMA\tDynamic\t1536\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_waits_history\tPERFORMANCE_SCHEMA\tDynamic\t2560\tNULL\tutf8mb4_0900_ai_ci\n' \
        'events_waits_history_long\tPERFORMANCE_SCHEMA\tDynamic\t10000\tNULL\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS,
            COALESCE(AUTO_INCREMENT, 'NULL'), TABLE_COLLATION
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables);"

printf '%s\n' "mysql_baseline_performance_schema_event_history_placeholders_expectations: ok"
