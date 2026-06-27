#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_variable_info_tables_expectations: $1" >&2
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

tables="'persisted_variables', 'variables_info'"

columns_expected=$(
    printf '%b' \
        'persisted_variables\tVARIABLE_NAME\t1\tNO\tvarchar(64)\tPRI\tutf8mb4_0900_ai_ci\n' \
        'persisted_variables\tVARIABLE_VALUE\t2\tYES\tvarchar(1024)\t\tutf8mb4_0900_ai_ci\n' \
        "variables_info\tVARIABLE_NAME\t1\tNO\tvarchar(64)\t\tutf8mb4_0900_ai_ci\n" \
        "variables_info\tVARIABLE_SOURCE\t2\tYES\tenum('COMPILED','GLOBAL','SERVER','EXPLICIT','EXTRA','USER','LOGIN','COMMAND_LINE','PERSISTED','DYNAMIC')\t\tutf8mb4_0900_ai_ci\n" \
        'variables_info\tVARIABLE_PATH\t3\tYES\tvarchar(1024)\t\tutf8mb4_0900_ai_ci\n' \
        'variables_info\tMIN_VALUE\t4\tYES\tvarchar(64)\t\tutf8mb4_0900_ai_ci\n' \
        'variables_info\tMAX_VALUE\t5\tYES\tvarchar(64)\t\tutf8mb4_0900_ai_ci\n' \
        'variables_info\tSET_TIME\t6\tYES\ttimestamp(6)\t\tNULL\n' \
        'variables_info\tSET_USER\t7\tYES\tchar(32)\t\tutf8mb4_bin\n' \
        'variables_info\tSET_HOST\t8\tYES\tchar(255)\t\tascii_general_ci'
)
expect_output \
    "Performance Schema variable info columns" \
    "$columns_expected" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COLUMN_KEY, COALESCE(COLLATION_NAME, 'NULL')
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, 'persisted_variables', 'variables_info'),
               ORDINAL_POSITION;"

expect_output \
    "Performance Schema variable info statistics" \
    'persisted_variables	PRIMARY	0	1	VARIABLE_NAME	1	1	HASH	YES' \
    "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
            COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema variable info constraints" \
    'persisted_variables	PRIMARY	PRIMARY KEY	YES' \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY TABLE_NAME, CONSTRAINT_NAME;"

expect_output \
    "Performance Schema variable info key usage" \
    'persisted_variables	PRIMARY	VARIABLE_NAME	1' \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION
       FROM information_schema.key_column_usage
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY TABLE_NAME, CONSTRAINT_NAME, ORDINAL_POSITION;"

expect_output \
    "Performance Schema variable info table rows" \
    "$(printf '%b' 'persisted_variables\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'variables_info\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, COALESCE(AUTO_INCREMENT, 'NULL'), TABLE_COLLATION
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, 'persisted_variables', 'variables_info');"

expect_output \
    "Performance Schema variable info representative rows" \
    "$(printf '%b' '1\n1\n1\n1')" \
    "SELECT COUNT(*) = 0
       FROM performance_schema.persisted_variables;
     SELECT COUNT(*) > 0
       FROM performance_schema.variables_info;
     SELECT COUNT(*) = 0
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME = 'variables_info';
     SELECT COUNT(*) = 6
       FROM performance_schema.variables_info
      WHERE VARIABLE_NAME IN ('autocommit', 'max_connections', 'performance_schema',
                              'sql_mode', 'time_zone', 'version')
        AND VARIABLE_SOURCE IN ('COMPILED', 'GLOBAL', 'SERVER', 'EXPLICIT',
                                'EXTRA', 'USER', 'LOGIN', 'COMMAND_LINE',
                                'PERSISTED', 'DYNAMIC')
        AND VARIABLE_PATH IS NOT NULL
        AND MIN_VALUE IS NOT NULL
        AND MAX_VALUE IS NOT NULL;"

printf '%s\n' "mysql_baseline_performance_schema_variable_info_tables_expectations: ok"
