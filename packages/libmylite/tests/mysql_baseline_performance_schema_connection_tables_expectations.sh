#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_connection_tables_expectations: $1" >&2
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
        'accounts\tUSER\t1\tYES\tchar(32)\tMUL\tutf8mb4_bin\n' \
        'accounts\tHOST\t2\tYES\tchar(255)\t\tascii_general_ci\n' \
        'accounts\tCURRENT_CONNECTIONS\t3\tNO\tbigint\t\tNULL\n' \
        'accounts\tTOTAL_CONNECTIONS\t4\tNO\tbigint\t\tNULL\n' \
        'accounts\tMAX_SESSION_CONTROLLED_MEMORY\t5\tNO\tbigint unsigned\t\tNULL\n' \
        'accounts\tMAX_SESSION_TOTAL_MEMORY\t6\tNO\tbigint unsigned\t\tNULL\n' \
        'hosts\tHOST\t1\tYES\tchar(255)\tUNI\tascii_general_ci\n' \
        'hosts\tCURRENT_CONNECTIONS\t2\tNO\tbigint\t\tNULL\n' \
        'hosts\tTOTAL_CONNECTIONS\t3\tNO\tbigint\t\tNULL\n' \
        'hosts\tMAX_SESSION_CONTROLLED_MEMORY\t4\tNO\tbigint unsigned\t\tNULL\n' \
        'hosts\tMAX_SESSION_TOTAL_MEMORY\t5\tNO\tbigint unsigned\t\tNULL\n' \
        'users\tUSER\t1\tYES\tchar(32)\tUNI\tutf8mb4_bin\n' \
        'users\tCURRENT_CONNECTIONS\t2\tNO\tbigint\t\tNULL\n' \
        'users\tTOTAL_CONNECTIONS\t3\tNO\tbigint\t\tNULL\n' \
        'users\tMAX_SESSION_CONTROLLED_MEMORY\t4\tNO\tbigint unsigned\t\tNULL\n' \
        'users\tMAX_SESSION_TOTAL_MEMORY\t5\tNO\tbigint unsigned\t\tNULL\n' \
        'processlist\tID\t1\tNO\tbigint unsigned\tPRI\tNULL\n' \
        'processlist\tUSER\t2\tYES\tvarchar(32)\t\tutf8mb4_0900_ai_ci\n' \
        'processlist\tHOST\t3\tYES\tvarchar(261)\t\tascii_general_ci\n' \
        'processlist\tDB\t4\tYES\tvarchar(64)\t\tutf8mb4_0900_ai_ci\n' \
        'processlist\tCOMMAND\t5\tYES\tvarchar(16)\t\tutf8mb4_0900_ai_ci\n' \
        'processlist\tTIME\t6\tYES\tbigint\t\tNULL\n' \
        'processlist\tSTATE\t7\tYES\tvarchar(64)\t\tutf8mb4_0900_ai_ci\n' \
        'processlist\tINFO\t8\tYES\tlongtext\t\tutf8mb4_0900_ai_ci\n' \
        "processlist\tEXECUTION_ENGINE\t9\tYES\tenum('PRIMARY','SECONDARY')\t\tutf8mb4_0900_ai_ci\n" \
        'threads\tTHREAD_ID\t1\tNO\tbigint unsigned\tPRI\tNULL\n' \
        'threads\tNAME\t2\tNO\tvarchar(128)\tMUL\tutf8mb4_0900_ai_ci\n' \
        'threads\tTYPE\t3\tNO\tvarchar(10)\t\tutf8mb4_0900_ai_ci\n' \
        'threads\tPROCESSLIST_ID\t4\tYES\tbigint unsigned\tMUL\tNULL\n' \
        'threads\tPROCESSLIST_USER\t5\tYES\tvarchar(32)\tMUL\tutf8mb4_0900_ai_ci\n' \
        'threads\tPROCESSLIST_HOST\t6\tYES\tvarchar(255)\tMUL\tascii_general_ci\n' \
        'threads\tPROCESSLIST_DB\t7\tYES\tvarchar(64)\t\tutf8mb4_0900_ai_ci\n' \
        'threads\tPROCESSLIST_COMMAND\t8\tYES\tvarchar(16)\t\tutf8mb4_0900_ai_ci\n' \
        'threads\tPROCESSLIST_TIME\t9\tYES\tbigint\t\tNULL\n' \
        'threads\tPROCESSLIST_STATE\t10\tYES\tvarchar(64)\t\tutf8mb4_0900_ai_ci\n' \
        'threads\tPROCESSLIST_INFO\t11\tYES\tlongtext\t\tutf8mb4_0900_ai_ci\n' \
        'threads\tPARENT_THREAD_ID\t12\tYES\tbigint unsigned\t\tNULL\n' \
        'threads\tROLE\t13\tYES\tvarchar(64)\t\tutf8mb4_0900_ai_ci\n' \
        "threads\tINSTRUMENTED\t14\tNO\tenum('YES','NO')\t\tutf8mb4_0900_ai_ci\n" \
        "threads\tHISTORY\t15\tNO\tenum('YES','NO')\t\tutf8mb4_0900_ai_ci\n" \
        'threads\tCONNECTION_TYPE\t16\tYES\tvarchar(16)\t\tutf8mb4_0900_ai_ci\n' \
        'threads\tTHREAD_OS_ID\t17\tYES\tbigint unsigned\tMUL\tNULL\n' \
        'threads\tRESOURCE_GROUP\t18\tYES\tvarchar(64)\tMUL\tutf8mb4_0900_ai_ci\n' \
        "threads\tEXECUTION_ENGINE\t19\tYES\tenum('PRIMARY','SECONDARY')\t\tutf8mb4_0900_ai_ci\n" \
        'threads\tCONTROLLED_MEMORY\t20\tNO\tbigint unsigned\t\tNULL\n' \
        'threads\tMAX_CONTROLLED_MEMORY\t21\tNO\tbigint unsigned\t\tNULL\n' \
        'threads\tTOTAL_MEMORY\t22\tNO\tbigint unsigned\t\tNULL\n' \
        'threads\tMAX_TOTAL_MEMORY\t23\tNO\tbigint unsigned\t\tNULL\n' \
        "threads\tTELEMETRY_ACTIVE\t24\tNO\tenum('YES','NO')\t\tutf8mb4_0900_ai_ci"
)
expect_output \
    "Performance Schema connection columns" \
    "$columns_expected" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COLUMN_KEY, COALESCE(COLLATION_NAME, 'NULL')
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('accounts', 'hosts', 'users', 'processlist', 'threads')
      ORDER BY FIELD(TABLE_NAME, 'accounts', 'hosts', 'users', 'processlist', 'threads'),
               ORDINAL_POSITION;"

statistics_expected=$(
    printf '%b' \
        'accounts\tACCOUNT\t0\t1\tUSER\t1\t1\tHASH\tYES\tYES\n' \
        'accounts\tACCOUNT\t0\t2\tHOST\t1\t1\tHASH\tYES\tYES\n' \
        'hosts\tHOST\t0\t1\tHOST\t1\t1\tHASH\tYES\tYES\n' \
        'users\tUSER\t0\t1\tUSER\t1\t1\tHASH\tYES\tYES\n' \
        'processlist\tPRIMARY\t0\t1\tID\t1\t1\tHASH\tYES\t\n' \
        'threads\tNAME\t1\t1\tNAME\t1\t1\tHASH\tYES\t\n' \
        'threads\tPRIMARY\t0\t1\tTHREAD_ID\t1\t1\tHASH\tYES\t\n' \
        'threads\tPROCESSLIST_ACCOUNT\t1\t1\tPROCESSLIST_USER\t1\t1\tHASH\tYES\tYES\n' \
        'threads\tPROCESSLIST_ACCOUNT\t1\t2\tPROCESSLIST_HOST\t1\t1\tHASH\tYES\tYES\n' \
        'threads\tPROCESSLIST_HOST\t1\t1\tPROCESSLIST_HOST\t1\t1\tHASH\tYES\tYES\n' \
        'threads\tPROCESSLIST_ID\t1\t1\tPROCESSLIST_ID\t1\t1\tHASH\tYES\tYES\n' \
        'threads\tRESOURCE_GROUP\t1\t1\tRESOURCE_GROUP\t1\t1\tHASH\tYES\tYES\n' \
        'threads\tTHREAD_OS_ID\t1\t1\tTHREAD_OS_ID\t1\t1\tHASH\tYES\tYES'
)
expect_output \
    "Performance Schema connection statistics" \
    "$statistics_expected" \
    "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
            COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE, NULLABLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('accounts', 'hosts', 'users', 'processlist', 'threads')
      ORDER BY FIELD(TABLE_NAME, 'accounts', 'hosts', 'users', 'processlist', 'threads'),
               INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema connection constraints" \
    "$(printf '%b' 'accounts\tACCOUNT\tUNIQUE\tYES\n' \
        'hosts\tHOST\tUNIQUE\tYES\n' \
        'users\tUSER\tUNIQUE\tYES\n' \
        'processlist\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'threads\tPRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('accounts', 'hosts', 'users', 'processlist', 'threads')
      ORDER BY FIELD(TABLE_NAME, 'accounts', 'hosts', 'users', 'processlist', 'threads'),
               CONSTRAINT_NAME;"

expect_output \
    "Performance Schema connection key usage" \
    "$(printf '%b' 'accounts\tACCOUNT\tUSER\t1\n' \
        'accounts\tACCOUNT\tHOST\t2\n' \
        'hosts\tHOST\tHOST\t1\n' \
        'users\tUSER\tUSER\t1\n' \
        'processlist\tPRIMARY\tID\t1\n' \
        'threads\tPRIMARY\tTHREAD_ID\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('accounts', 'hosts', 'users', 'processlist', 'threads')
      ORDER BY FIELD(TABLE_NAME, 'accounts', 'hosts', 'users', 'processlist', 'threads'),
               CONSTRAINT_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema connection table rows" \
    "$(printf '%b' 'accounts\tPERFORMANCE_SCHEMA\tFixed\tNULL\tutf8mb4_0900_ai_ci\n' \
        'hosts\tPERFORMANCE_SCHEMA\tFixed\tNULL\tutf8mb4_0900_ai_ci\n' \
        'users\tPERFORMANCE_SCHEMA\tFixed\tNULL\tutf8mb4_0900_ai_ci\n' \
        'processlist\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'threads\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, COALESCE(AUTO_INCREMENT, 'NULL'), TABLE_COLLATION
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('accounts', 'hosts', 'users', 'processlist', 'threads')
      ORDER BY FIELD(TABLE_NAME, 'accounts', 'hosts', 'users', 'processlist', 'threads');"

expect_output \
    "Performance Schema summary rows exist" \
    "$(printf '%b' '1\n1\n1')" \
    "SELECT COUNT(*) > 0
       FROM performance_schema.accounts
      WHERE USER = SUBSTRING_INDEX(CURRENT_USER(), '@', 1);
     SELECT COUNT(*) > 0 FROM performance_schema.hosts;
     SELECT COUNT(*) > 0
       FROM performance_schema.users
      WHERE USER = SUBSTRING_INDEX(CURRENT_USER(), '@', 1);"

expect_output \
    "Performance Schema processlist representative row" \
    "$(printf '%b' '1\troot\tNULL\t1\tPRIMARY')" \
    "SELECT ID = CONNECTION_ID(), USER, COALESCE(DB, 'NULL'),
            COMMAND IN ('Query', 'Sleep'), EXECUTION_ENGINE
       FROM performance_schema.processlist
      WHERE ID = CONNECTION_ID();"

expect_output \
    "Performance Schema threads representative row" \
    "$(printf '%b' '1\tthread/sql/one_connection\tFOREGROUND\t1\troot\tNULL\t1\tYES\tYES\tPRIMARY\tNO')" \
    "SELECT THREAD_ID > 0, NAME, TYPE, PROCESSLIST_ID = CONNECTION_ID(),
            PROCESSLIST_USER, COALESCE(PROCESSLIST_DB, 'NULL'),
            PROCESSLIST_COMMAND IN ('Query', 'Sleep'), INSTRUMENTED, HISTORY,
            EXECUTION_ENGINE, TELEMETRY_ACTIVE
       FROM performance_schema.threads
      WHERE PROCESSLIST_ID = CONNECTION_ID();"

printf '%s\n' "mysql_baseline_performance_schema_connection_tables_expectations: ok"
