#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_remaining_placeholders_expectations: $1" >&2
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

tables="'error_log','log_status','setup_instruments'"
order_tables="'error_log','log_status','setup_instruments'"

expect_output \
    "Performance Schema remaining column counts" \
    "$(printf '%b' 'error_log\t6\nlog_status\t4\nsetup_instruments\t7')" \
    "SELECT TABLE_NAME, COUNT(*)
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      GROUP BY TABLE_NAME
      ORDER BY FIELD(TABLE_NAME, $order_tables);"

expect_output \
    "Performance Schema remaining representative columns" \
    "$(printf '%b' 'error_log\tLOGGED\t1\tNO\ttimestamp(6)\tPRI\tNULL\tNULL\tNULL\tNULL\tNULL\t6\n' \
        'error_log\tTHREAD_ID\t2\tYES\tbigint unsigned\tMUL\tNULL\tNULL\tNULL\t20\t0\tNULL\n' \
        'error_log\tPRIO\t3\tNO\tenum(\047System\047,\047Error\047,\047Warning\047,\047Note\047)\tMUL\tutf8mb4_0900_ai_ci\tutf8mb4\t7\tNULL\tNULL\tNULL\n' \
        'error_log\tDATA\t6\tNO\ttext\t\tutf8mb4_0900_ai_ci\tutf8mb4\t65535\tNULL\tNULL\tNULL\n' \
        'log_status\tSERVER_UUID\t1\tNO\tchar(36)\t\tutf8mb4_bin\tutf8mb4\t36\tNULL\tNULL\tNULL\n' \
        'log_status\tLOCAL\t2\tNO\tjson\t\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\n' \
        'setup_instruments\tNAME\t1\tNO\tvarchar(128)\tPRI\tutf8mb4_0900_ai_ci\tutf8mb4\t128\tNULL\tNULL\tNULL\n' \
        'setup_instruments\tTIMED\t3\tYES\tenum(\047YES\047,\047NO\047)\t\tutf8mb4_0900_ai_ci\tutf8mb4\t3\tNULL\tNULL\tNULL\n' \
        'setup_instruments\tPROPERTIES\t4\tNO\tset(\047singleton\047,\047progress\047,\047user\047,\047global_statistics\047,\047mutable\047,\047controlled_by_default\047)\t\tutf8mb4_0900_ai_ci\tutf8mb4\t71\tNULL\tNULL\tNULL\n' \
        'setup_instruments\tFLAGS\t5\tYES\tset(\047controlled\047)\t\tutf8mb4_0900_ai_ci\tutf8mb4\t10\tNULL\tNULL\tNULL\n' \
        'setup_instruments\tDOCUMENTATION\t7\tYES\tlongtext\t\tutf8mb4_0900_ai_ci\tutf8mb4\t4294967295\tNULL\tNULL\tNULL')" \
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
            (TABLE_NAME = 'error_log'
             AND COLUMN_NAME IN ('LOGGED', 'THREAD_ID', 'PRIO', 'DATA'))
            OR (TABLE_NAME = 'log_status'
                AND COLUMN_NAME IN ('SERVER_UUID', 'LOCAL'))
            OR (TABLE_NAME = 'setup_instruments'
                AND COLUMN_NAME IN ('NAME', 'TIMED', 'PROPERTIES', 'FLAGS', 'DOCUMENTATION'))
        )
      ORDER BY FIELD(TABLE_NAME, $order_tables), ORDINAL_POSITION;"

expect_output \
    "Performance Schema remaining statistics" \
    "$(printf '%b' 'error_log\tERROR_CODE\t1\t1\tERROR_CODE\t1\t1\tHASH\tYES\n' \
        'error_log\tPRIMARY\t0\t1\tLOGGED\t1\t1\tHASH\tYES\n' \
        'error_log\tPRIO\t1\t1\tPRIO\t1\t1\tHASH\tYES\n' \
        'error_log\tSUBSYSTEM\t1\t1\tSUBSYSTEM\t1\t1\tHASH\tYES\n' \
        'error_log\tTHREAD_ID\t1\t1\tTHREAD_ID\t1\t1\tHASH\tYES\n' \
        'setup_instruments\tPRIMARY\t0\t1\tNAME\t1\t1\tHASH\tYES')" \
    "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
            COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema remaining constraints" \
    "$(printf '%b' 'error_log\tPRIMARY\tPRIMARY KEY\tYES\nsetup_instruments\tPRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME;"

expect_output \
    "Performance Schema remaining key usage" \
    "$(printf '%b' 'error_log\tPRIMARY\tLOGGED\t1\nsetup_instruments\tPRIMARY\tNAME\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema remaining constraint extensions" \
    "$(printf '%b' 'error_log\tERROR_CODE\t1\t1\n' \
        'error_log\tPRIMARY\t1\t1\n' \
        'error_log\tPRIO\t1\t1\n' \
        'error_log\tSUBSYSTEM\t1\t1\n' \
        'error_log\tTHREAD_ID\t1\t1\n' \
        'setup_instruments\tPRIMARY\t1\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME,
            ENGINE_ATTRIBUTE IS NULL, SECONDARY_ENGINE_ATTRIBUTE IS NULL
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME;"

expect_output \
    "Performance Schema remaining table metadata" \
    "$(printf '%b' 'error_log\tPERFORMANCE_SCHEMA\tDynamic\t1\tNULL\tutf8mb4_0900_ai_ci\n' \
        'log_status\tPERFORMANCE_SCHEMA\tDynamic\t1\tNULL\tutf8mb4_0900_ai_ci\n' \
        'setup_instruments\tPERFORMANCE_SCHEMA\tDynamic\t1561\tNULL\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_NAME, ENGINE, ROW_FORMAT,
            CASE WHEN TABLE_NAME = 'error_log' THEN TABLE_ROWS IS NOT NULL ELSE TABLE_ROWS END,
            COALESCE(AUTO_INCREMENT, 'NULL'), TABLE_COLLATION
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables);"

table_status=$(
    run_mysql "SHOW TABLE STATUS FROM performance_schema
                WHERE Name IN ('error_log', 'log_status', 'setup_instruments');" \
        | awk -F '\t' \
            '{ print $1 "\t" $2 "\t" $4 "\t" ($1 == "error_log" ? ($5 != "" ? 1 : 0) : $5) "\t" ($11 == "" ? "NULL" : $11) "\t" $15 }'
)
expected_table_status=$(
    printf '%b' 'error_log\tPERFORMANCE_SCHEMA\tDynamic\t1\tNULL\tutf8mb4_0900_ai_ci\n' \
        'log_status\tPERFORMANCE_SCHEMA\tDynamic\t1\tNULL\tutf8mb4_0900_ai_ci\n' \
        'setup_instruments\tPERFORMANCE_SCHEMA\tDynamic\t1561\tNULL\tutf8mb4_0900_ai_ci'
)
if [ "$table_status" != "$expected_table_status" ]; then
    fail "Performance Schema remaining table status: expected [$expected_table_status], got [$table_status]"
fi

expect_output \
    "Performance Schema log_status row shape" \
    "1	OBJECT	OBJECT	OBJECT" \
    "SELECT SERVER_UUID REGEXP '^[0-9a-f-]{36}$',
            JSON_TYPE(LOCAL), JSON_TYPE(REPLICATION), JSON_TYPE(STORAGE_ENGINES)
       FROM performance_schema.log_status;"

expect_output \
    "Performance Schema setup_instruments representative rows" \
    "$(printf '%b' 'idle\tYES\tYES\tuser\tNULL\t0\tNULL\n' \
        'memory/sql/TABLE\tYES\tNULL\tglobal_statistics\t\t0\tMemory used by TABLE objects and their mem root.\n' \
        'stage/sql/starting\tNO\tNO\t\tNULL\t0\tNULL\n' \
        'statement/sql/select\tYES\tYES\t\tNULL\t0\tNULL\n' \
        'wait/io/socket/sql/client_connection\tNO\tNO\tuser\tNULL\t0\tNULL\n' \
        'wait/synch/mutex/pfs/LOCK_pfs_share_list\tNO\tNO\tsingleton\tNULL\t1\tComponents can provide their own performance_schema tables. This lock protects the list of such tables definitions.\n' \
        'wait/synch/mutex/sql/MYSQL_BIN_LOG::LOCK_commit\tNO\tNO\t\tNULL\t0\tNULL\n' \
        'wait/synch/mutex/sql/TC_LOG_MMAP::LOCK_tc\tNO\tNO\t\tNULL\t0\tNULL')" \
    "SELECT NAME, ENABLED, COALESCE(TIMED, 'NULL'), PROPERTIES,
            COALESCE(FLAGS, 'NULL'), VOLATILITY, COALESCE(DOCUMENTATION, 'NULL')
       FROM performance_schema.setup_instruments
      WHERE NAME IN (
            'idle',
            'memory/sql/TABLE',
            'stage/sql/starting',
            'statement/sql/select',
            'wait/io/socket/sql/client_connection',
            'wait/synch/mutex/pfs/LOCK_pfs_share_list',
            'wait/synch/mutex/sql/MYSQL_BIN_LOG::LOCK_commit',
            'wait/synch/mutex/sql/TC_LOG_MMAP::LOCK_tc')
      ORDER BY NAME;"

printf '%s\n' "mysql_baseline_performance_schema_remaining_placeholders_expectations: ok"
