#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_host_keyring_placeholders_expectations: $1" >&2
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

tables="'host_cache', 'keyring_component_status', 'keyring_keys'"

expect_output \
    "Performance Schema host/keyring row counts" \
    "$(printf '%b' 'host_cache\t0\n' \
        'keyring_component_status\t0\n' \
        'keyring_keys\t0')" \
    "SELECT 'host_cache', COUNT(*) FROM performance_schema.host_cache
      UNION ALL
     SELECT 'keyring_component_status', COUNT(*)
       FROM performance_schema.keyring_component_status
      UNION ALL
     SELECT 'keyring_keys', COUNT(*) FROM performance_schema.keyring_keys;"

expect_output \
    "Performance Schema host/keyring column counts" \
    "$(printf '%b' 'host_cache\t29\n' \
        'keyring_component_status\t2\n' \
        'keyring_keys\t3')" \
    "SELECT TABLE_NAME, COUNT(*)
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      GROUP BY TABLE_NAME
      ORDER BY FIELD(TABLE_NAME, 'host_cache', 'keyring_component_status', 'keyring_keys');"

expect_output \
    "Performance Schema host/keyring representative columns" \
    "$(printf '%b' 'host_cache\tIP\t1\tNO\tvarchar(64)\tPRI\tutf8mb4_0900_ai_ci\tutf8mb4\t64\t256\tNULL\tNULL\tNULL\n' \
        'host_cache\tHOST\t2\tYES\tvarchar(255)\tMUL\tascii_general_ci\tascii\t255\t255\tNULL\tNULL\tNULL\n' \
        'host_cache\tHOST_VALIDATED\t3\tNO\tenum('\''YES'\'','\''NO'\'')\t\tutf8mb4_0900_ai_ci\tutf8mb4\t3\t12\tNULL\tNULL\tNULL\n' \
        'host_cache\tSUM_CONNECT_ERRORS\t4\tNO\tbigint\t\tNULL\tNULL\tNULL\tNULL\t19\t0\tNULL\n' \
        'host_cache\tFIRST_SEEN\t26\tNO\ttimestamp\t\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\t0\n' \
        'host_cache\tLAST_ERROR_SEEN\t29\tYES\ttimestamp\t\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\t0\n' \
        'keyring_component_status\tSTATUS_KEY\t1\tNO\tvarchar(256)\t\tutf8mb4_0900_ai_ci\tutf8mb4\t256\t1024\tNULL\tNULL\tNULL\n' \
        'keyring_component_status\tSTATUS_VALUE\t2\tNO\tvarchar(1024)\t\tutf8mb4_0900_ai_ci\tutf8mb4\t1024\t4096\tNULL\tNULL\tNULL\n' \
        'keyring_keys\tKEY_ID\t1\tNO\tvarchar(255)\t\tutf8mb4_bin\tutf8mb4\t255\t1020\tNULL\tNULL\tNULL\n' \
        'keyring_keys\tKEY_OWNER\t2\tYES\tvarchar(255)\t\tutf8mb4_bin\tutf8mb4\t255\t1020\tNULL\tNULL\tNULL\n' \
        'keyring_keys\tBACKEND_KEY_ID\t3\tYES\tvarchar(255)\t\tutf8mb4_bin\tutf8mb4\t255\t1020\tNULL\tNULL\tNULL')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COLUMN_KEY, COALESCE(COLLATION_NAME, 'NULL'),
            COALESCE(CHARACTER_SET_NAME, 'NULL'),
            COALESCE(CHARACTER_MAXIMUM_LENGTH, 'NULL'),
            COALESCE(CHARACTER_OCTET_LENGTH, 'NULL'),
            COALESCE(NUMERIC_PRECISION, 'NULL'),
            COALESCE(NUMERIC_SCALE, 'NULL'),
            COALESCE(DATETIME_PRECISION, 'NULL')
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
        AND COLUMN_NAME IN ('IP', 'HOST', 'HOST_VALIDATED', 'SUM_CONNECT_ERRORS',
                            'FIRST_SEEN', 'LAST_ERROR_SEEN', 'STATUS_KEY',
                            'STATUS_VALUE', 'KEY_ID', 'KEY_OWNER',
                            'BACKEND_KEY_ID')
      ORDER BY FIELD(TABLE_NAME, 'host_cache', 'keyring_component_status', 'keyring_keys'),
               ORDINAL_POSITION;"

expect_output \
    "Performance Schema host/keyring statistics" \
    "$(printf '%b' 'host_cache\tHOST\t1\t1\tHOST\t1\t1\tHASH\tYES\n' \
        'host_cache\tPRIMARY\t0\t1\tIP\t1\t1\tHASH\tYES')" \
    "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
            COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, 'host_cache', 'keyring_component_status', 'keyring_keys'),
               INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema host/keyring constraints" \
    "host_cache	PRIMARY	PRIMARY KEY	YES" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, 'host_cache', 'keyring_component_status', 'keyring_keys'),
               CONSTRAINT_NAME;"

expect_output \
    "Performance Schema host/keyring table rows" \
    "$(printf '%b' 'host_cache\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'keyring_component_status\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'keyring_keys\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_bin')" \
    "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, COALESCE(AUTO_INCREMENT, 'NULL'), TABLE_COLLATION
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, 'host_cache', 'keyring_component_status', 'keyring_keys');"

printf '%s\n' "mysql_baseline_performance_schema_host_keyring_placeholders_expectations: ok"
