#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_connection_attribute_tables_expectations: $1" >&2
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

tables="'session_account_connect_attrs', 'session_connect_attrs'"

columns_expected=$(
    printf '%b' \
        'session_account_connect_attrs\tPROCESSLIST_ID\t1\tNO\tbigint unsigned\tPRI\tNULL\n' \
        'session_account_connect_attrs\tATTR_NAME\t2\tNO\tvarchar(32)\tPRI\tutf8mb4_bin\n' \
        'session_account_connect_attrs\tATTR_VALUE\t3\tYES\tvarchar(1024)\t\tutf8mb4_bin\n' \
        'session_account_connect_attrs\tORDINAL_POSITION\t4\tYES\tint\t\tNULL\n' \
        'session_connect_attrs\tPROCESSLIST_ID\t1\tNO\tbigint unsigned\tPRI\tNULL\n' \
        'session_connect_attrs\tATTR_NAME\t2\tNO\tvarchar(32)\tPRI\tutf8mb4_bin\n' \
        'session_connect_attrs\tATTR_VALUE\t3\tYES\tvarchar(1024)\t\tutf8mb4_bin\n' \
        'session_connect_attrs\tORDINAL_POSITION\t4\tYES\tint\t\tNULL'
)
expect_output \
    "Performance Schema connection attribute columns" \
    "$columns_expected" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COLUMN_KEY, COALESCE(COLLATION_NAME, 'NULL')
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, 'session_account_connect_attrs', 'session_connect_attrs'),
               ORDINAL_POSITION;"

statistics_expected=$(
    printf '%b' \
        'session_account_connect_attrs\tPRIMARY\t0\t1\tPROCESSLIST_ID\t1\t1\tHASH\tYES\t\n' \
        'session_account_connect_attrs\tPRIMARY\t0\t2\tATTR_NAME\t1\t1\tHASH\tYES\t\n' \
        'session_connect_attrs\tPRIMARY\t0\t1\tPROCESSLIST_ID\t1\t1\tHASH\tYES\t\n' \
        'session_connect_attrs\tPRIMARY\t0\t2\tATTR_NAME\t1\t1\tHASH\tYES\t'
)
expect_output \
    "Performance Schema connection attribute statistics" \
    "$statistics_expected" \
    "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
            COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE, NULLABLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, 'session_account_connect_attrs', 'session_connect_attrs'),
               INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema connection attribute constraints" \
    "$(printf '%b' 'session_account_connect_attrs\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'session_connect_attrs\tPRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, 'session_account_connect_attrs', 'session_connect_attrs'),
               CONSTRAINT_NAME;"

expect_output \
    "Performance Schema connection attribute key usage" \
    "$(printf '%b' 'session_account_connect_attrs\tPRIMARY\tPROCESSLIST_ID\t1\n' \
        'session_account_connect_attrs\tPRIMARY\tATTR_NAME\t2\n' \
        'session_connect_attrs\tPRIMARY\tPROCESSLIST_ID\t1\n' \
        'session_connect_attrs\tPRIMARY\tATTR_NAME\t2')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, 'session_account_connect_attrs', 'session_connect_attrs'),
               CONSTRAINT_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema connection attribute table rows" \
    "$(printf '%b' 'session_account_connect_attrs\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_bin\n' \
        'session_connect_attrs\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_bin')" \
    "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, COALESCE(AUTO_INCREMENT, 'NULL'), TABLE_COLLATION
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, 'session_account_connect_attrs', 'session_connect_attrs');"

expect_output \
    "Performance Schema connection attribute representative rows" \
    "$(printf '%b' '1\n1\n1')" \
    "SELECT COUNT(*) > 0
       FROM performance_schema.session_connect_attrs
      WHERE PROCESSLIST_ID = CONNECTION_ID();
     SELECT COUNT(*) > 0
       FROM performance_schema.session_account_connect_attrs
      WHERE PROCESSLIST_ID = CONNECTION_ID();
     SELECT COUNT(*) > 0
       FROM performance_schema.session_connect_attrs
      WHERE PROCESSLIST_ID = CONNECTION_ID()
        AND ATTR_NAME IN ('_client_name', 'program_name');"

printf '%s\n' "mysql_baseline_performance_schema_connection_attribute_tables_expectations: ok"
