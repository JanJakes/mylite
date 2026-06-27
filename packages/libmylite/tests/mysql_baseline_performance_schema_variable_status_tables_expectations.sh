#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_variable_status_tables_expectations: $1" >&2
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
        'global_status\tVARIABLE_NAME\t1\tNULL\tNO\tvarchar\t64\t256\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'global_status\tVARIABLE_VALUE\t2\tNULL\tYES\tvarchar\t1024\t4096\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(1024)\t\t\tselect,insert,update,references\t\t\n' \
        'global_variables\tVARIABLE_NAME\t1\tNULL\tNO\tvarchar\t64\t256\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'global_variables\tVARIABLE_VALUE\t2\tNULL\tYES\tvarchar\t1024\t4096\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(1024)\t\t\tselect,insert,update,references\t\t\n' \
        'session_status\tVARIABLE_NAME\t1\tNULL\tNO\tvarchar\t64\t256\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'session_status\tVARIABLE_VALUE\t2\tNULL\tYES\tvarchar\t1024\t4096\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(1024)\t\t\tselect,insert,update,references\t\t\n' \
        'session_variables\tVARIABLE_NAME\t1\tNULL\tNO\tvarchar\t64\t256\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\tPRI\t\tselect,insert,update,references\t\t\n' \
        'session_variables\tVARIABLE_VALUE\t2\tNULL\tYES\tvarchar\t1024\t4096\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(1024)\t\t\tselect,insert,update,references\t\t'
)
expect_output \
    "Performance Schema variable/status INFORMATION_SCHEMA.COLUMNS rows" \
    "$columns_expected" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE,
            DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,
            NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION,
            CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, EXTRA,
            PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('global_variables', 'session_variables', 'global_status', 'session_status')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema variable/status INFORMATION_SCHEMA.STATISTICS rows" \
    "$(printf '%b' 'global_status\tPRIMARY\t1\tVARIABLE_NAME\tNULL\tHASH\tYES\n' \
        'global_variables\tPRIMARY\t1\tVARIABLE_NAME\tNULL\tHASH\tYES\n' \
        'session_status\tPRIMARY\t1\tVARIABLE_NAME\tNULL\tHASH\tYES\n' \
        'session_variables\tPRIMARY\t1\tVARIABLE_NAME\tNULL\tHASH\tYES')" \
    "SELECT TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, CARDINALITY, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('global_variables', 'session_variables', 'global_status', 'session_status')
      ORDER BY TABLE_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema variable/status constraints" \
    "$(printf '%b' 'global_status\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'global_variables\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'session_status\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'session_variables\tPRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('global_variables', 'session_variables', 'global_status', 'session_status')
      ORDER BY TABLE_NAME;"

expect_output \
    "Performance Schema variable/status key columns" \
    "$(printf '%b' 'global_status\tPRIMARY\tVARIABLE_NAME\t1\n' \
        'global_variables\tPRIMARY\tVARIABLE_NAME\t1\n' \
        'session_status\tPRIMARY\tVARIABLE_NAME\t1\n' \
        'session_variables\tPRIMARY\tVARIABLE_NAME\t1')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('global_variables', 'session_variables', 'global_status', 'session_status')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema variable/status table metadata" \
    "$(printf '%b' 'global_status\tBASE TABLE\tPERFORMANCE_SCHEMA\t10\tDynamic\tNULL\t1\t1\t1\tutf8mb4_0900_ai_ci\t1\t1\t1\n' \
        'global_variables\tBASE TABLE\tPERFORMANCE_SCHEMA\t10\tDynamic\tNULL\t1\t1\t1\tutf8mb4_0900_ai_ci\t1\t1\t1\n' \
        'session_status\tBASE TABLE\tPERFORMANCE_SCHEMA\t10\tDynamic\tNULL\t1\t1\t1\tutf8mb4_0900_ai_ci\t1\t1\t1\n' \
        'session_variables\tBASE TABLE\tPERFORMANCE_SCHEMA\t10\tDynamic\tNULL\t1\t1\t1\tutf8mb4_0900_ai_ci\t1\t1\t1')" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL,
            CREATE_OPTIONS = '', TABLE_COMMENT = ''
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ('global_variables', 'session_variables', 'global_status', 'session_status')
      ORDER BY TABLE_NAME;"

expect_output \
    "Performance Schema session variables reflect session scope" \
    "$(printf '%b' 'autocommit\tOFF\n' \
        'performance_schema\tON\n' \
        'time_zone\t+02:00\n' \
        'global_autocommit\tON')" \
    "SET SESSION autocommit = 0;
     SET SESSION time_zone = '+02:00';
     SELECT VARIABLE_NAME, VARIABLE_VALUE
       FROM performance_schema.session_variables
      WHERE VARIABLE_NAME IN ('autocommit', 'performance_schema', 'time_zone')
      ORDER BY VARIABLE_NAME;
     SELECT 'global_autocommit', VARIABLE_VALUE
       FROM performance_schema.global_variables
      WHERE VARIABLE_NAME = 'autocommit';"

expect_output \
    "Performance Schema status command counter filtering" \
    "$(printf '%b' 'Com_stmt_reprepare\nCom_stmt_reprepare')" \
    "SELECT VARIABLE_NAME
       FROM performance_schema.global_status
      WHERE VARIABLE_NAME LIKE 'Com\\\\_%'
      ORDER BY VARIABLE_NAME;
     SELECT VARIABLE_NAME
       FROM performance_schema.session_status
      WHERE VARIABLE_NAME LIKE 'Com\\\\_%'
      ORDER BY VARIABLE_NAME;"

expect_output \
    "Performance Schema session-only status rows" \
    "$(printf '%b' 'global\t0\nsession\t6')" \
    "SELECT 'global', COUNT(*)
       FROM performance_schema.global_status
      WHERE VARIABLE_NAME IN ('Compression', 'Compression_algorithm', 'Compression_level',
                              'Last_query_cost', 'Last_query_partial_plans',
                              'Tls_sni_server_name')
     UNION ALL
     SELECT 'session', COUNT(*)
       FROM performance_schema.session_status
      WHERE VARIABLE_NAME IN ('Compression', 'Compression_algorithm', 'Compression_level',
                              'Last_query_cost', 'Last_query_partial_plans',
                              'Tls_sni_server_name');"

expect_output \
    "Performance Schema selected schema resolution" \
    "ON" \
    "USE performance_schema; SELECT VARIABLE_VALUE FROM session_variables WHERE VARIABLE_NAME = 'autocommit';"

printf '%s\n' "mysql_baseline_performance_schema_variable_status_tables_expectations: ok"
