#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_object_tls_placeholders_expectations: $1" >&2
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

tables="'objects_summary_global_by_type',
        'tls_channel_status'"

order_tables="'objects_summary_global_by_type',
              'tls_channel_status'"

expect_output \
    "Performance Schema object/TLS columns" \
    "$(printf '%b' 'objects_summary_global_by_type\tOBJECT_TYPE\t1\tNULL\tYES\tvarchar\t64\t256\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\tMUL\t\tselect,insert,update,references\t\n' \
        'objects_summary_global_by_type\tOBJECT_SCHEMA\t2\tNULL\tYES\tvarchar\t64\t256\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\t\t\tselect,insert,update,references\t\n' \
        'objects_summary_global_by_type\tOBJECT_NAME\t3\tNULL\tYES\tvarchar\t64\t256\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\t\t\tselect,insert,update,references\t\n' \
        'objects_summary_global_by_type\tCOUNT_STAR\t4\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t\n' \
        'objects_summary_global_by_type\tSUM_TIMER_WAIT\t5\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t\n' \
        'objects_summary_global_by_type\tMIN_TIMER_WAIT\t6\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t\n' \
        'objects_summary_global_by_type\tAVG_TIMER_WAIT\t7\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t\n' \
        'objects_summary_global_by_type\tMAX_TIMER_WAIT\t8\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t\n' \
        'tls_channel_status\tCHANNEL\t1\tNULL\tNO\tvarchar\t128\t512\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(128)\t\t\tselect,insert,update,references\t\n' \
        'tls_channel_status\tPROPERTY\t2\tNULL\tNO\tvarchar\t128\t512\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(128)\t\t\tselect,insert,update,references\t\n' \
        'tls_channel_status\tVALUE\t3\tNULL\tNO\tvarchar\t2048\t8192\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(2048)\t\t\tselect,insert,update,references\t')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE,
            DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,
            NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION,
            CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, EXTRA,
            PRIVILEGES, COLUMN_COMMENT
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), ORDINAL_POSITION;"

expect_output \
    "Performance Schema object/TLS statistics" \
    "$(printf '%b' 'objects_summary_global_by_type\tOBJECT\t0\t1\tOBJECT_TYPE\t1\t1\tHASH\tYES\n' \
        'objects_summary_global_by_type\tOBJECT\t0\t2\tOBJECT_SCHEMA\t1\t1\tHASH\tYES\n' \
        'objects_summary_global_by_type\tOBJECT\t0\t3\tOBJECT_NAME\t1\t1\tHASH\tYES')" \
    "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
            COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema object/TLS constraints" \
    "objects_summary_global_by_type	OBJECT	UNIQUE	YES" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME;"

expect_output \
    "Performance Schema object/TLS key usage" \
    "$(printf '%b' 'objects_summary_global_by_type\tOBJECT\tOBJECT_TYPE\t1\n' \
        'objects_summary_global_by_type\tOBJECT\tOBJECT_SCHEMA\t2\n' \
        'objects_summary_global_by_type\tOBJECT\tOBJECT_NAME\t3')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema object/TLS constraint extensions" \
    "objects_summary_global_by_type	OBJECT	1	1" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME,
            ENGINE_ATTRIBUTE IS NULL, SECONDARY_ENGINE_ATTRIBUTE IS NULL
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables), CONSTRAINT_NAME;"

expect_output \
    "Performance Schema object/TLS table metadata" \
    "$(printf '%b' 'objects_summary_global_by_type\tPERFORMANCE_SCHEMA\tDynamic\t4096\tNULL\tutf8mb4_0900_ai_ci\n' \
        'tls_channel_status\tPERFORMANCE_SCHEMA\tDynamic\t96\tNULL\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS,
            COALESCE(AUTO_INCREMENT, 'NULL'), TABLE_COLLATION
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, $order_tables);"

expect_output \
    "Performance Schema objects_summary_global_by_type show index" \
    "$(printf '%b' 'objects_summary_global_by_type\t0\tOBJECT\t1\tOBJECT_TYPE\tNULL\tNULL\tNULL\tNULL\tYES\tHASH\t\t\tYES\tNULL\n' \
        'objects_summary_global_by_type\t0\tOBJECT\t2\tOBJECT_SCHEMA\tNULL\tNULL\tNULL\tNULL\tYES\tHASH\t\t\tYES\tNULL\n' \
        'objects_summary_global_by_type\t0\tOBJECT\t3\tOBJECT_NAME\tNULL\tNULL\tNULL\tNULL\tYES\tHASH\t\t\tYES\tNULL')" \
    "SHOW INDEX FROM performance_schema.objects_summary_global_by_type;"

expect_output \
    "Performance Schema tls_channel_status show index" \
    "" \
    "SHOW INDEX FROM performance_schema.tls_channel_status;"

expect_output \
    "Performance Schema object/TLS selectable" \
    "$(printf '%b' 'objects_summary_global_by_type\t1\n' \
        'tls_channel_status\t1')" \
    "SELECT 'objects_summary_global_by_type', COUNT(*) >= 0
       FROM performance_schema.objects_summary_global_by_type
      UNION ALL
     SELECT 'tls_channel_status', COUNT(*) >= 0
       FROM performance_schema.tls_channel_status;"

printf '%s\n' "mysql_baseline_performance_schema_object_tls_placeholders_expectations: ok"
