#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_thread_status_variable_tables_expectations: $1" >&2
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

columns_expected=$(
    printf '%b' \
        'status_by_account\tUSER\t1\tYES\tchar(32)\tMUL\tutf8mb4_bin\n' \
        'status_by_account\tHOST\t2\tYES\tchar(255)\t\tascii_general_ci\n' \
        'status_by_account\tVARIABLE_NAME\t3\tNO\tvarchar(64)\t\tutf8mb4_0900_ai_ci\n' \
        'status_by_account\tVARIABLE_VALUE\t4\tYES\tvarchar(1024)\t\tutf8mb4_0900_ai_ci\n' \
        'status_by_host\tHOST\t1\tYES\tchar(255)\tMUL\tascii_general_ci\n' \
        'status_by_host\tVARIABLE_NAME\t2\tNO\tvarchar(64)\t\tutf8mb4_0900_ai_ci\n' \
        'status_by_host\tVARIABLE_VALUE\t3\tYES\tvarchar(1024)\t\tutf8mb4_0900_ai_ci\n' \
        'status_by_thread\tTHREAD_ID\t1\tNO\tbigint unsigned\tPRI\tNULL\n' \
        'status_by_thread\tVARIABLE_NAME\t2\tNO\tvarchar(64)\tPRI\tutf8mb4_0900_ai_ci\n' \
        'status_by_thread\tVARIABLE_VALUE\t3\tYES\tvarchar(1024)\t\tutf8mb4_0900_ai_ci\n' \
        'status_by_user\tUSER\t1\tYES\tchar(32)\tMUL\tutf8mb4_bin\n' \
        'status_by_user\tVARIABLE_NAME\t2\tNO\tvarchar(64)\t\tutf8mb4_0900_ai_ci\n' \
        'status_by_user\tVARIABLE_VALUE\t3\tYES\tvarchar(1024)\t\tutf8mb4_0900_ai_ci\n' \
        'variables_by_thread\tTHREAD_ID\t1\tNO\tbigint unsigned\tPRI\tNULL\n' \
        'variables_by_thread\tVARIABLE_NAME\t2\tNO\tvarchar(64)\tPRI\tutf8mb4_0900_ai_ci\n' \
        'variables_by_thread\tVARIABLE_VALUE\t3\tYES\tvarchar(1024)\t\tutf8mb4_0900_ai_ci'
)
expect_output \
    "Performance Schema thread/status columns" \
    "$columns_expected" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COLUMN_KEY, COALESCE(COLLATION_NAME, 'NULL')
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('variables_by_thread', 'status_by_thread',
                           'status_by_account', 'status_by_host', 'status_by_user')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

statistics_expected=$(
    printf '%b' \
        'status_by_account\tACCOUNT\t0\t1\tUSER\t1\t1\tHASH\tYES\n' \
        'status_by_account\tACCOUNT\t0\t2\tHOST\t1\t1\tHASH\tYES\n' \
        'status_by_account\tACCOUNT\t0\t3\tVARIABLE_NAME\t1\t1\tHASH\tYES\n' \
        'status_by_host\tHOST\t0\t1\tHOST\t1\t1\tHASH\tYES\n' \
        'status_by_host\tHOST\t0\t2\tVARIABLE_NAME\t1\t1\tHASH\tYES\n' \
        'status_by_thread\tPRIMARY\t0\t1\tTHREAD_ID\t1\t1\tHASH\tYES\n' \
        'status_by_thread\tPRIMARY\t0\t2\tVARIABLE_NAME\t1\t1\tHASH\tYES\n' \
        'status_by_user\tUSER\t0\t1\tUSER\t1\t1\tHASH\tYES\n' \
        'status_by_user\tUSER\t0\t2\tVARIABLE_NAME\t1\t1\tHASH\tYES\n' \
        'variables_by_thread\tPRIMARY\t0\t1\tTHREAD_ID\t1\t1\tHASH\tYES\n' \
        'variables_by_thread\tPRIMARY\t0\t2\tVARIABLE_NAME\t1\t1\tHASH\tYES'
)
expect_output \
    "Performance Schema thread/status statistics" \
    "$statistics_expected" \
    "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
            COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('variables_by_thread', 'status_by_thread',
                           'status_by_account', 'status_by_host', 'status_by_user')
      ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema thread/status constraints" \
    "$(printf '%b' 'status_by_account\tACCOUNT\tUNIQUE\tYES\n' \
        'status_by_host\tHOST\tUNIQUE\tYES\n' \
        'status_by_thread\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'status_by_user\tUSER\tUNIQUE\tYES\n' \
        'variables_by_thread\tPRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('variables_by_thread', 'status_by_thread',
                           'status_by_account', 'status_by_host', 'status_by_user')
      ORDER BY TABLE_NAME, CONSTRAINT_NAME;"

expect_output \
    "Performance Schema thread/status key usage" \
    "$(printf '%b' 'status_by_account\tACCOUNT\tUSER\t1\n' \
        'status_by_account\tACCOUNT\tHOST\t2\n' \
        'status_by_account\tACCOUNT\tVARIABLE_NAME\t3\n' \
        'status_by_host\tHOST\tHOST\t1\n' \
        'status_by_host\tHOST\tVARIABLE_NAME\t2\n' \
        'status_by_thread\tPRIMARY\tTHREAD_ID\t1\n' \
        'status_by_thread\tPRIMARY\tVARIABLE_NAME\t2\n' \
        'status_by_user\tUSER\tUSER\t1\n' \
        'status_by_user\tUSER\tVARIABLE_NAME\t2\n' \
        'variables_by_thread\tPRIMARY\tTHREAD_ID\t1\n' \
        'variables_by_thread\tPRIMARY\tVARIABLE_NAME\t2')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('variables_by_thread', 'status_by_thread',
                           'status_by_account', 'status_by_host', 'status_by_user')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema thread/status table rows" \
    "$(printf '%b' 'status_by_account\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'status_by_host\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'status_by_thread\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'status_by_user\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'variables_by_thread\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, COALESCE(AUTO_INCREMENT, 'NULL'), TABLE_COLLATION
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('variables_by_thread', 'status_by_thread',
                             'status_by_account', 'status_by_host', 'status_by_user')
      ORDER BY TABLE_NAME;"

run_mysql "SET autocommit = 0; SET time_zone = '+02:00';" >/dev/null
expect_output \
    "Performance Schema variables_by_thread representative rows" \
    "$(printf '%b' 'autocommit\tOFF\n' \
        'time_zone\t+02:00')" \
    "SET autocommit = 0;
     SET time_zone = '+02:00';
     SELECT VARIABLE_NAME, VARIABLE_VALUE
       FROM performance_schema.variables_by_thread
      WHERE THREAD_ID = (
            SELECT THREAD_ID
              FROM performance_schema.threads
             WHERE PROCESSLIST_ID = CONNECTION_ID())
        AND VARIABLE_NAME IN ('autocommit', 'time_zone')
      ORDER BY VARIABLE_NAME;"

expect_output \
    "Performance Schema status_by_thread representative rows" \
    "$(printf '%b' 'Com_stmt_reprepare\t0\n' \
        'Compression\tOFF')" \
    "SELECT VARIABLE_NAME, VARIABLE_VALUE
       FROM performance_schema.status_by_thread
      WHERE THREAD_ID = (
            SELECT THREAD_ID
              FROM performance_schema.threads
             WHERE PROCESSLIST_ID = CONNECTION_ID())
        AND VARIABLE_NAME IN ('Com_stmt_reprepare', 'Compression')
      ORDER BY VARIABLE_NAME;"

expect_output \
    "Performance Schema status summary rows exist" \
    "$(printf '%b' '1\n1\n1')" \
    "SELECT COUNT(*) > 0 FROM performance_schema.status_by_account;
     SELECT COUNT(*) > 0 FROM performance_schema.status_by_host;
     SELECT COUNT(*) > 0 FROM performance_schema.status_by_user;"

printf '%s\n' "mysql_baseline_performance_schema_thread_status_variable_tables_expectations: ok"
