#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_setup_actor_consumer_tables_expectations: $1" >&2
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

expect_output \
    "Performance Schema setup table columns" \
    "$(printf '%b' 'setup_actors\tHOST\t1\t%\tNO\tchar\t255\t255\tNULL\tNULL\tNULL\tascii\tascii_general_ci\tchar(255)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'setup_actors\tUSER\t2\t%\tNO\tchar\t32\t128\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_bin\tchar(32)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'setup_actors\tROLE\t3\t%\tNO\tchar\t32\t128\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_bin\tchar(32)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'setup_actors\tENABLED\t4\tYES\tNO\tenum\t3\t12\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tenum(\047YES\047,\047NO\047)\t\t\tselect,insert,update,references\t\t\n' \
        'setup_actors\tHISTORY\t5\tYES\tNO\tenum\t3\t12\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tenum(\047YES\047,\047NO\047)\t\t\tselect,insert,update,references\t\t\n' \
        'setup_consumers\tNAME\t1\tNULL\tNO\tvarchar\t64\t256\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'setup_consumers\tENABLED\t2\tNULL\tNO\tenum\t3\t12\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tenum(\047YES\047,\047NO\047)\t\t\tselect,insert,update,references\t\t')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE,
            DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,
            NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION,
            CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, EXTRA,
            PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('setup_actors', 'setup_consumers')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema setup table statistics" \
    "$(printf '%b' 'setup_actors\tPRIMARY\t1\tHOST\t1\t1\tHASH\tYES\n' \
        'setup_actors\tPRIMARY\t2\tUSER\t1\t1\tHASH\tYES\n' \
        'setup_actors\tPRIMARY\t3\tROLE\t1\t1\tHASH\tYES\n' \
        'setup_consumers\tPRIMARY\t1\tNAME\t1\t1\tHASH\tYES')" \
    "SELECT TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION IS NULL,
            CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('setup_actors', 'setup_consumers')
      ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema setup table constraints" \
    "$(printf '%b' 'setup_actors\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'setup_consumers\tPRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('setup_actors', 'setup_consumers')
      ORDER BY TABLE_NAME, CONSTRAINT_NAME;"

expect_output \
    "Performance Schema setup table key usage" \
    "$(printf '%b' 'setup_actors\tPRIMARY\tHOST\t1\n' \
        'setup_actors\tPRIMARY\tUSER\t2\n' \
        'setup_actors\tPRIMARY\tROLE\t3\n' \
        'setup_consumers\tPRIMARY\tNAME\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('setup_actors', 'setup_consumers')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema setup table constraint extensions" \
    "$(printf '%b' 'performance_schema\tsetup_actors\tPRIMARY\t1\t1\n' \
        'performance_schema\tsetup_consumers\tPRIMARY\t1\t1')" \
    "SELECT CONSTRAINT_SCHEMA, TABLE_NAME, CONSTRAINT_NAME,
            ENGINE_ATTRIBUTE IS NULL, SECONDARY_ENGINE_ATTRIBUTE IS NULL
       FROM information_schema.table_constraints_extensions
      WHERE CONSTRAINT_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('setup_actors', 'setup_consumers')
      ORDER BY TABLE_NAME, CONSTRAINT_NAME;"

setup_actors_status=$(
    run_mysql "SHOW TABLE STATUS FROM performance_schema
                WHERE Name = 'setup_actors'
                  AND Engine = 'PERFORMANCE_SCHEMA'
                  AND Row_format = 'Fixed'
                  AND Auto_increment IS NULL
                  AND Collation = 'utf8mb4_0900_ai_ci';" \
        | awk -F '\t' '{print $1 "\t" $2 "\t" $4 "\t" $5 "\t" $11 "\t" $15}'
)
if [ "$setup_actors_status" != "setup_actors	PERFORMANCE_SCHEMA	Fixed	128	NULL	utf8mb4_0900_ai_ci" ]; then
    fail "Performance Schema setup_actors SHOW TABLE STATUS: got [$setup_actors_status]"
fi

setup_consumers_status=$(
    run_mysql "SHOW TABLE STATUS FROM performance_schema
                WHERE Name = 'setup_consumers'
                  AND Engine = 'PERFORMANCE_SCHEMA'
                  AND Row_format = 'Dynamic'
                  AND Auto_increment IS NULL
                  AND Collation = 'utf8mb4_0900_ai_ci';" \
        | awk -F '\t' '{print $1 "\t" $2 "\t" $4 "\t" $5 "\t" $11 "\t" $15}'
)
if [ "$setup_consumers_status" != "setup_consumers	PERFORMANCE_SCHEMA	Dynamic	16	NULL	utf8mb4_0900_ai_ci" ]; then
    fail "Performance Schema setup_consumers SHOW TABLE STATUS: got [$setup_consumers_status]"
fi

expect_output \
    "Performance Schema setup table metadata" \
    "$(printf '%b' 'setup_actors\tBASE TABLE\tPERFORMANCE_SCHEMA\t10\tFixed\t128\tNULL\t1\t1\t1\tutf8mb4_0900_ai_ci\t1\t1\t1\n' \
        'setup_consumers\tBASE TABLE\tPERFORMANCE_SCHEMA\t10\tDynamic\t16\tNULL\t1\t1\t1\tutf8mb4_0900_ai_ci\t1\t1\t1')" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL,
            CREATE_OPTIONS = '', TABLE_COMMENT = ''
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('setup_actors', 'setup_consumers')
      ORDER BY TABLE_NAME;"

expect_output \
    "Performance Schema setup_actors rows" \
    "$(printf '%b' '%\t%\t%\tYES\tYES')" \
    "SELECT HOST, USER, ROLE, ENABLED, HISTORY
       FROM performance_schema.setup_actors
      ORDER BY HOST, USER, ROLE;"

expect_output \
    "Performance Schema setup_consumers rows" \
    "$(printf '%b' 'events_stages_current\tNO\n' \
        'events_stages_history\tNO\n' \
        'events_stages_history_long\tNO\n' \
        'events_statements_cpu\tNO\n' \
        'events_statements_current\tYES\n' \
        'events_statements_history\tYES\n' \
        'events_statements_history_long\tNO\n' \
        'events_transactions_current\tYES\n' \
        'events_transactions_history\tYES\n' \
        'events_transactions_history_long\tNO\n' \
        'events_waits_current\tNO\n' \
        'events_waits_history\tNO\n' \
        'events_waits_history_long\tNO\n' \
        'global_instrumentation\tYES\n' \
        'statements_digest\tYES\n' \
        'thread_instrumentation\tYES')" \
    "SELECT NAME, ENABLED
       FROM performance_schema.setup_consumers
      ORDER BY NAME;"

printf '%s\n' "mysql_baseline_performance_schema_setup_actor_consumer_tables_expectations: ok"
