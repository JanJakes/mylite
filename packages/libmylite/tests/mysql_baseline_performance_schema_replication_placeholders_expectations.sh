#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_replication_placeholders_expectations: $1" >&2
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

tables="'replication_applier_configuration',
        'replication_applier_filters',
        'replication_applier_global_filters',
        'replication_applier_status',
        'replication_applier_status_by_coordinator',
        'replication_applier_status_by_worker',
        'replication_asynchronous_connection_failover',
        'replication_asynchronous_connection_failover_managed',
        'replication_connection_configuration',
        'replication_connection_status',
        'replication_group_member_stats',
        'replication_group_members'"

expect_output \
    "Performance Schema replication row counts" \
    "$(printf '%b' 'replication_applier_configuration\t0\n' \
        'replication_applier_filters\t0\n' \
        'replication_applier_global_filters\t0\n' \
        'replication_applier_status\t0\n' \
        'replication_applier_status_by_coordinator\t0\n' \
        'replication_applier_status_by_worker\t0\n' \
        'replication_asynchronous_connection_failover\t0\n' \
        'replication_asynchronous_connection_failover_managed\t0\n' \
        'replication_connection_configuration\t0\n' \
        'replication_connection_status\t0\n' \
        'replication_group_member_stats\t0\n' \
        'replication_group_members\t0')" \
    "SELECT 'replication_applier_configuration', COUNT(*)
       FROM performance_schema.replication_applier_configuration
      UNION ALL
     SELECT 'replication_applier_filters', COUNT(*)
       FROM performance_schema.replication_applier_filters
      UNION ALL
     SELECT 'replication_applier_global_filters', COUNT(*)
       FROM performance_schema.replication_applier_global_filters
      UNION ALL
     SELECT 'replication_applier_status', COUNT(*)
       FROM performance_schema.replication_applier_status
      UNION ALL
     SELECT 'replication_applier_status_by_coordinator', COUNT(*)
       FROM performance_schema.replication_applier_status_by_coordinator
      UNION ALL
     SELECT 'replication_applier_status_by_worker', COUNT(*)
       FROM performance_schema.replication_applier_status_by_worker
      UNION ALL
     SELECT 'replication_asynchronous_connection_failover', COUNT(*)
       FROM performance_schema.replication_asynchronous_connection_failover
      UNION ALL
     SELECT 'replication_asynchronous_connection_failover_managed', COUNT(*)
       FROM performance_schema.replication_asynchronous_connection_failover_managed
      UNION ALL
     SELECT 'replication_connection_configuration', COUNT(*)
       FROM performance_schema.replication_connection_configuration
      UNION ALL
     SELECT 'replication_connection_status', COUNT(*)
       FROM performance_schema.replication_connection_status
      UNION ALL
     SELECT 'replication_group_member_stats', COUNT(*)
       FROM performance_schema.replication_group_member_stats
      UNION ALL
     SELECT 'replication_group_members', COUNT(*)
       FROM performance_schema.replication_group_members;"

expect_output \
    "Performance Schema replication column counts" \
    "$(printf '%b' 'replication_applier_configuration\t7\n' \
        'replication_applier_filters\t6\n' \
        'replication_applier_global_filters\t4\n' \
        'replication_applier_status\t4\n' \
        'replication_applier_status_by_coordinator\t15\n' \
        'replication_applier_status_by_worker\t24\n' \
        'replication_asynchronous_connection_failover\t6\n' \
        'replication_asynchronous_connection_failover_managed\t4\n' \
        'replication_connection_configuration\t27\n' \
        'replication_connection_status\t20\n' \
        'replication_group_member_stats\t13\n' \
        'replication_group_members\t8')" \
    "SELECT TABLE_NAME, COUNT(*)
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      GROUP BY TABLE_NAME
      ORDER BY TABLE_NAME;"

expect_output \
    "Performance Schema replication representative columns" \
    "$(printf '%b' 'replication_applier_configuration\tCHANNEL_NAME\t1\tNO\tchar(64)\tPRI\tutf8mb4_0900_ai_ci\tutf8mb4\t64\t256\tNULL\tNULL\tNULL\tNULL\n' \
        'replication_applier_configuration\tPRIVILEGE_CHECKS_USER\t3\tYES\ttext\t\tutf8mb3_bin\tutf8mb3\t65535\t65535\tNULL\tNULL\tNULL\tNULL\n' \
        'replication_applier_configuration\tREQUIRE_TABLE_PRIMARY_KEY_CHECK\t5\tNO\tenum('\''STREAM'\'','\''ON'\'','\''OFF'\'','\''GENERATE'\'')\t\tutf8mb4_0900_ai_ci\tutf8mb4\t8\t32\tNULL\tNULL\tNULL\tNULL\n' \
        'replication_applier_filters\tACTIVE_SINCE\t5\tNO\ttimestamp(6)\t\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\t6\tNULL\n' \
        'replication_applier_filters\tCOUNTER\t6\tNO\tbigint unsigned\t\tNULL\tNULL\tNULL\tNULL\t20\t0\tNULL\t0\n' \
        'replication_applier_status_by_worker\tWORKER_ID\t2\tNO\tbigint unsigned\tPRI\tNULL\tNULL\tNULL\tNULL\t20\t0\tNULL\tNULL\n' \
        'replication_applier_status_by_worker\tTHREAD_ID\t3\tYES\tbigint unsigned\tMUL\tNULL\tNULL\tNULL\tNULL\t20\t0\tNULL\tNULL\n' \
        'replication_asynchronous_connection_failover\tMANAGED_NAME\t6\tNO\tchar(64)\t\tutf8mb3_general_ci\tutf8mb3\t64\t192\tNULL\tNULL\tNULL\tEMPTY\n' \
        'replication_asynchronous_connection_failover_managed\tCONFIGURATION\t4\tYES\tjson\t\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\n' \
        'replication_connection_configuration\tHEARTBEAT_INTERVAL\t18\tNO\tdouble(10,3)\t\tNULL\tNULL\tNULL\tNULL\t10\t3\tNULL\tNULL\n' \
        'replication_connection_status\tSERVICE_STATE\t5\tNO\tenum('\''ON'\'','\''OFF'\'','\''CONNECTING'\'')\t\tutf8mb4_0900_ai_ci\tutf8mb4\t10\t40\tNULL\tNULL\tNULL\tNULL\n' \
        'replication_group_members\tMEMBER_HOST\t3\tNO\tchar(255)\t\tascii_general_ci\tascii\t255\t255\tNULL\tNULL\tNULL\tNULL')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COLUMN_KEY, COALESCE(COLLATION_NAME, 'NULL'),
            COALESCE(CHARACTER_SET_NAME, 'NULL'),
            COALESCE(CHARACTER_MAXIMUM_LENGTH, 'NULL'),
            COALESCE(CHARACTER_OCTET_LENGTH, 'NULL'),
            COALESCE(NUMERIC_PRECISION, 'NULL'),
            COALESCE(NUMERIC_SCALE, 'NULL'),
            COALESCE(DATETIME_PRECISION, 'NULL'),
            CASE
                WHEN COLUMN_DEFAULT IS NULL THEN 'NULL'
                WHEN COLUMN_DEFAULT = '' THEN 'EMPTY'
                ELSE COLUMN_DEFAULT
            END
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND (
            (TABLE_NAME = 'replication_applier_configuration'
             AND COLUMN_NAME IN ('CHANNEL_NAME', 'PRIVILEGE_CHECKS_USER',
                                 'REQUIRE_TABLE_PRIMARY_KEY_CHECK'))
            OR (TABLE_NAME = 'replication_applier_filters'
                AND COLUMN_NAME IN ('ACTIVE_SINCE', 'COUNTER'))
            OR (TABLE_NAME = 'replication_applier_status_by_worker'
                AND COLUMN_NAME IN ('WORKER_ID', 'THREAD_ID'))
            OR (TABLE_NAME = 'replication_asynchronous_connection_failover'
                AND COLUMN_NAME = 'MANAGED_NAME')
            OR (TABLE_NAME = 'replication_asynchronous_connection_failover_managed'
                AND COLUMN_NAME = 'CONFIGURATION')
            OR (TABLE_NAME = 'replication_connection_configuration'
                AND COLUMN_NAME = 'HEARTBEAT_INTERVAL')
            OR (TABLE_NAME = 'replication_connection_status'
                AND COLUMN_NAME = 'SERVICE_STATE')
            OR (TABLE_NAME = 'replication_group_members'
                AND COLUMN_NAME = 'MEMBER_HOST')
        )
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema replication statistics" \
    "$(printf '%b' 'replication_applier_configuration\tPRIMARY\t0\t1\tCHANNEL_NAME\t1\t1\tHASH\tYES\n' \
        'replication_applier_status\tPRIMARY\t0\t1\tCHANNEL_NAME\t1\t1\tHASH\tYES\n' \
        'replication_applier_status_by_coordinator\tPRIMARY\t0\t1\tCHANNEL_NAME\t1\t1\tHASH\tYES\n' \
        'replication_applier_status_by_coordinator\tTHREAD_ID\t1\t1\tTHREAD_ID\t1\t1\tHASH\tYES\n' \
        'replication_applier_status_by_worker\tPRIMARY\t0\t1\tCHANNEL_NAME\t1\t1\tHASH\tYES\n' \
        'replication_applier_status_by_worker\tPRIMARY\t0\t2\tWORKER_ID\t1\t1\tHASH\tYES\n' \
        'replication_applier_status_by_worker\tTHREAD_ID\t1\t1\tTHREAD_ID\t1\t1\tHASH\tYES\n' \
        'replication_connection_configuration\tPRIMARY\t0\t1\tCHANNEL_NAME\t1\t1\tHASH\tYES\n' \
        'replication_connection_status\tPRIMARY\t0\t1\tCHANNEL_NAME\t1\t1\tHASH\tYES\n' \
        'replication_connection_status\tTHREAD_ID\t1\t1\tTHREAD_ID\t1\t1\tHASH\tYES')" \
    "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
            COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema replication constraints" \
    "$(printf '%b' 'replication_applier_configuration\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'replication_applier_status\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'replication_applier_status_by_coordinator\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'replication_applier_status_by_worker\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'replication_connection_configuration\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'replication_connection_status\tPRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY TABLE_NAME, CONSTRAINT_NAME;"

expect_output \
    "Performance Schema replication key column usage" \
    "$(printf '%b' 'replication_applier_configuration\tPRIMARY\tCHANNEL_NAME\t1\n' \
        'replication_applier_status\tPRIMARY\tCHANNEL_NAME\t1\n' \
        'replication_applier_status_by_coordinator\tPRIMARY\tCHANNEL_NAME\t1\n' \
        'replication_applier_status_by_worker\tPRIMARY\tCHANNEL_NAME\t1\n' \
        'replication_applier_status_by_worker\tPRIMARY\tWORKER_ID\t2\n' \
        'replication_connection_configuration\tPRIMARY\tCHANNEL_NAME\t1\n' \
        'replication_connection_status\tPRIMARY\tCHANNEL_NAME\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY TABLE_NAME, CONSTRAINT_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema replication constraint extensions" \
    "$(printf '%b' 'replication_applier_configuration\tPRIMARY\t1\t1\n' \
        'replication_applier_status\tPRIMARY\t1\t1\n' \
        'replication_applier_status_by_coordinator\tPRIMARY\t1\t1\n' \
        'replication_applier_status_by_coordinator\tTHREAD_ID\t1\t1\n' \
        'replication_applier_status_by_worker\tPRIMARY\t1\t1\n' \
        'replication_applier_status_by_worker\tTHREAD_ID\t1\t1\n' \
        'replication_connection_configuration\tPRIMARY\t1\t1\n' \
        'replication_connection_status\tPRIMARY\t1\t1\n' \
        'replication_connection_status\tTHREAD_ID\t1\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME,
            ENGINE_ATTRIBUTE IS NULL, SECONDARY_ENGINE_ATTRIBUTE IS NULL
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY TABLE_NAME, CONSTRAINT_NAME;"

expect_output \
    "Performance Schema replication table status metadata" \
    "$(printf '%b' 'replication_applier_configuration\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'replication_applier_status\tPERFORMANCE_SCHEMA\tFixed\tNULL\tutf8mb4_0900_ai_ci\n' \
        'replication_applier_status_by_worker\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'replication_asynchronous_connection_failover\tPERFORMANCE_SCHEMA\tFixed\tNULL\tutf8mb4_0900_ai_ci\n' \
        'replication_connection_configuration\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'replication_connection_status\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'replication_group_members\tPERFORMANCE_SCHEMA\tFixed\tNULL\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, COALESCE(AUTO_INCREMENT, 'NULL'), TABLE_COLLATION
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('replication_applier_configuration',
                           'replication_applier_status',
                           'replication_applier_status_by_worker',
                           'replication_asynchronous_connection_failover',
                           'replication_connection_configuration',
                           'replication_connection_status',
                           'replication_group_members')
      ORDER BY TABLE_NAME;"

printf '%s\n' "mysql_baseline_performance_schema_replication_placeholders_expectations: ok"
