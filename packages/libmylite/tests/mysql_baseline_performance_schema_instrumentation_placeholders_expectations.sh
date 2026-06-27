#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_instrumentation_placeholders_expectations: $1" >&2
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

tables="'binary_log_transaction_compression_stats', 'data_locks', 'data_lock_waits', 'prepared_statements_instances'"

expect_output \
    "Performance Schema instrumentation row counts" \
    "$(printf '%b' 'binary_log_transaction_compression_stats\t0\n' \
        'data_locks\t0\n' \
        'data_lock_waits\t0\n' \
        'prepared_statements_instances\t0')" \
    "SELECT 'binary_log_transaction_compression_stats', COUNT(*)
       FROM performance_schema.binary_log_transaction_compression_stats
      UNION ALL
     SELECT 'data_locks', COUNT(*) FROM performance_schema.data_locks
      UNION ALL
     SELECT 'data_lock_waits', COUNT(*) FROM performance_schema.data_lock_waits
      UNION ALL
     SELECT 'prepared_statements_instances', COUNT(*)
       FROM performance_schema.prepared_statements_instances;"

expect_output \
    "Performance Schema instrumentation column counts" \
    "$(printf '%b' 'binary_log_transaction_compression_stats\t14\n' \
        'data_locks\t15\n' \
        'data_lock_waits\t11\n' \
        'prepared_statements_instances\t40')" \
    "SELECT TABLE_NAME, COUNT(*)
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      GROUP BY TABLE_NAME
      ORDER BY FIELD(TABLE_NAME, 'binary_log_transaction_compression_stats',
                     'data_locks', 'data_lock_waits',
                     'prepared_statements_instances');"

expect_output \
    "Performance Schema instrumentation representative columns" \
    "$(printf '%b' 'binary_log_transaction_compression_stats\tLOG_TYPE\t1\tNO\tenum('\''BINARY'\'','\''RELAY'\'')\t\tutf8mb4_0900_ai_ci\t6\tNULL\tNULL\n' \
        'binary_log_transaction_compression_stats\tCOMPRESSION_TYPE\t2\tNO\tvarchar(64)\t\tutf8mb4_0900_ai_ci\t64\tNULL\tNULL\n' \
        'binary_log_transaction_compression_stats\tLAST_TRANSACTION_TIMESTAMP\t14\tYES\ttimestamp(6)\t\tNULL\tNULL\tNULL\t6\n' \
        'data_locks\tENGINE\t1\tNO\tvarchar(32)\tPRI\tutf8mb4_0900_ai_ci\t32\tNULL\tNULL\n' \
        'data_locks\tENGINE_LOCK_ID\t2\tNO\tvarchar(128)\tPRI\tutf8mb4_0900_ai_ci\t128\tNULL\tNULL\n' \
        'data_locks\tENGINE_TRANSACTION_ID\t3\tYES\tbigint unsigned\tMUL\tNULL\tNULL\t20\tNULL\n' \
        'data_locks\tOBJECT_INSTANCE_BEGIN\t11\tNO\tbigint unsigned\t\tNULL\tNULL\t20\tNULL\n' \
        'data_locks\tLOCK_DATA\t15\tYES\tvarchar(8192)\t\tutf8mb4_0900_ai_ci\t8192\tNULL\tNULL\n' \
        'data_lock_waits\tENGINE\t1\tNO\tvarchar(32)\tPRI\tutf8mb4_0900_ai_ci\t32\tNULL\tNULL\n' \
        'data_lock_waits\tREQUESTING_ENGINE_LOCK_ID\t2\tNO\tvarchar(128)\tPRI\tutf8mb4_0900_ai_ci\t128\tNULL\tNULL\n' \
        'data_lock_waits\tBLOCKING_ENGINE_LOCK_ID\t7\tNO\tvarchar(128)\tPRI\tutf8mb4_0900_ai_ci\t128\tNULL\tNULL\n' \
        'prepared_statements_instances\tOBJECT_INSTANCE_BEGIN\t1\tNO\tbigint unsigned\tPRI\tNULL\tNULL\t20\tNULL\n' \
        'prepared_statements_instances\tSTATEMENT_NAME\t3\tYES\tvarchar(64)\tMUL\tutf8mb4_0900_ai_ci\t64\tNULL\tNULL\n' \
        'prepared_statements_instances\tSQL_TEXT\t4\tNO\tlongtext\t\tutf8mb4_0900_ai_ci\t4294967295\tNULL\tNULL\n' \
        'prepared_statements_instances\tOWNER_OBJECT_TYPE\t7\tYES\tenum('\''EVENT'\'','\''FUNCTION'\'','\''PROCEDURE'\'','\''TABLE'\'','\''TRIGGER'\'')\tMUL\tutf8mb4_0900_ai_ci\t9\tNULL\tNULL\n' \
        'prepared_statements_instances\tCOUNT_SECONDARY\t40\tNO\tbigint unsigned\t\tNULL\tNULL\t20\tNULL')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COLUMN_KEY, COALESCE(COLLATION_NAME, 'NULL'),
            COALESCE(CHARACTER_MAXIMUM_LENGTH, 'NULL'),
            COALESCE(NUMERIC_PRECISION, 'NULL'),
            COALESCE(DATETIME_PRECISION, 'NULL')
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
        AND COLUMN_NAME IN ('LOG_TYPE', 'COMPRESSION_TYPE', 'LAST_TRANSACTION_TIMESTAMP',
                            'ENGINE', 'ENGINE_LOCK_ID', 'ENGINE_TRANSACTION_ID',
                            'LOCK_DATA', 'REQUESTING_ENGINE_LOCK_ID',
                            'BLOCKING_ENGINE_LOCK_ID', 'OBJECT_INSTANCE_BEGIN',
                            'STATEMENT_NAME', 'SQL_TEXT', 'OWNER_OBJECT_TYPE',
                            'COUNT_SECONDARY')
      ORDER BY FIELD(TABLE_NAME, 'binary_log_transaction_compression_stats',
                     'data_locks', 'data_lock_waits',
                     'prepared_statements_instances'),
               ORDINAL_POSITION;"

expect_output \
    "Performance Schema instrumentation statistics" \
    "$(printf '%b' 'data_locks\tENGINE_TRANSACTION_ID\t1\t1\tENGINE_TRANSACTION_ID\t1\t1\tHASH\tYES\n' \
        'data_locks\tENGINE_TRANSACTION_ID\t1\t2\tENGINE\t1\t1\tHASH\tYES\n' \
        'data_locks\tOBJECT_SCHEMA\t1\t1\tOBJECT_SCHEMA\t1\t1\tHASH\tYES\n' \
        'data_locks\tOBJECT_SCHEMA\t1\t2\tOBJECT_NAME\t1\t1\tHASH\tYES\n' \
        'data_locks\tOBJECT_SCHEMA\t1\t3\tPARTITION_NAME\t1\t1\tHASH\tYES\n' \
        'data_locks\tOBJECT_SCHEMA\t1\t4\tSUBPARTITION_NAME\t1\t1\tHASH\tYES\n' \
        'data_locks\tPRIMARY\t0\t1\tENGINE_LOCK_ID\t1\t1\tHASH\tYES\n' \
        'data_locks\tPRIMARY\t0\t2\tENGINE\t1\t1\tHASH\tYES\n' \
        'data_locks\tTHREAD_ID\t1\t1\tTHREAD_ID\t1\t1\tHASH\tYES\n' \
        'data_locks\tTHREAD_ID\t1\t2\tEVENT_ID\t1\t1\tHASH\tYES\n' \
        'data_lock_waits\tBLOCKING_ENGINE_LOCK_ID\t1\t1\tBLOCKING_ENGINE_LOCK_ID\t1\t1\tHASH\tYES\n' \
        'data_lock_waits\tBLOCKING_ENGINE_LOCK_ID\t1\t2\tENGINE\t1\t1\tHASH\tYES\n' \
        'data_lock_waits\tBLOCKING_ENGINE_TRANSACTION_ID\t1\t1\tBLOCKING_ENGINE_TRANSACTION_ID\t1\t1\tHASH\tYES\n' \
        'data_lock_waits\tBLOCKING_ENGINE_TRANSACTION_ID\t1\t2\tENGINE\t1\t1\tHASH\tYES\n' \
        'data_lock_waits\tBLOCKING_THREAD_ID\t1\t1\tBLOCKING_THREAD_ID\t1\t1\tHASH\tYES\n' \
        'data_lock_waits\tBLOCKING_THREAD_ID\t1\t2\tBLOCKING_EVENT_ID\t1\t1\tHASH\tYES\n' \
        'data_lock_waits\tPRIMARY\t0\t1\tREQUESTING_ENGINE_LOCK_ID\t1\t1\tHASH\tYES\n' \
        'data_lock_waits\tPRIMARY\t0\t2\tBLOCKING_ENGINE_LOCK_ID\t1\t1\tHASH\tYES\n' \
        'data_lock_waits\tPRIMARY\t0\t3\tENGINE\t1\t1\tHASH\tYES\n' \
        'data_lock_waits\tREQUESTING_ENGINE_LOCK_ID\t1\t1\tREQUESTING_ENGINE_LOCK_ID\t1\t1\tHASH\tYES\n' \
        'data_lock_waits\tREQUESTING_ENGINE_LOCK_ID\t1\t2\tENGINE\t1\t1\tHASH\tYES\n' \
        'data_lock_waits\tREQUESTING_ENGINE_TRANSACTION_ID\t1\t1\tREQUESTING_ENGINE_TRANSACTION_ID\t1\t1\tHASH\tYES\n' \
        'data_lock_waits\tREQUESTING_ENGINE_TRANSACTION_ID\t1\t2\tENGINE\t1\t1\tHASH\tYES\n' \
        'data_lock_waits\tREQUESTING_THREAD_ID\t1\t1\tREQUESTING_THREAD_ID\t1\t1\tHASH\tYES\n' \
        'data_lock_waits\tREQUESTING_THREAD_ID\t1\t2\tREQUESTING_EVENT_ID\t1\t1\tHASH\tYES\n' \
        'prepared_statements_instances\tOWNER_OBJECT_TYPE\t1\t1\tOWNER_OBJECT_TYPE\t1\t1\tHASH\tYES\n' \
        'prepared_statements_instances\tOWNER_OBJECT_TYPE\t1\t2\tOWNER_OBJECT_SCHEMA\t1\t1\tHASH\tYES\n' \
        'prepared_statements_instances\tOWNER_OBJECT_TYPE\t1\t3\tOWNER_OBJECT_NAME\t1\t1\tHASH\tYES\n' \
        'prepared_statements_instances\tOWNER_THREAD_ID\t0\t1\tOWNER_THREAD_ID\t1\t1\tHASH\tYES\n' \
        'prepared_statements_instances\tOWNER_THREAD_ID\t0\t2\tOWNER_EVENT_ID\t1\t1\tHASH\tYES\n' \
        'prepared_statements_instances\tPRIMARY\t0\t1\tOBJECT_INSTANCE_BEGIN\t1\t1\tHASH\tYES\n' \
        'prepared_statements_instances\tSTATEMENT_ID\t1\t1\tSTATEMENT_ID\t1\t1\tHASH\tYES\n' \
        'prepared_statements_instances\tSTATEMENT_NAME\t1\t1\tSTATEMENT_NAME\t1\t1\tHASH\tYES')" \
    "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
            COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, 'binary_log_transaction_compression_stats',
                     'data_locks', 'data_lock_waits',
                     'prepared_statements_instances'),
               INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema instrumentation constraints" \
    "$(printf '%b' 'data_locks\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'data_lock_waits\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'prepared_statements_instances\tOWNER_THREAD_ID\tUNIQUE\tYES\n' \
        'prepared_statements_instances\tPRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, 'binary_log_transaction_compression_stats',
                     'data_locks', 'data_lock_waits',
                     'prepared_statements_instances'),
               CONSTRAINT_NAME;"

expect_output \
    "Performance Schema instrumentation table rows" \
    "$(printf '%b' 'binary_log_transaction_compression_stats\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'data_locks\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'data_lock_waits\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'prepared_statements_instances\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, COALESCE(AUTO_INCREMENT, 'NULL'), TABLE_COLLATION
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, 'binary_log_transaction_compression_stats',
                     'data_locks', 'data_lock_waits',
                     'prepared_statements_instances');"

printf '%s\n' "mysql_baseline_performance_schema_instrumentation_placeholders_expectations: ok"
