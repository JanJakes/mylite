#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_latest_file_io_views_expectations: $1" >&2
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

expect_contains() {
    label=$1
    needle=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    case "$output" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle], got [$output]" ;;
    esac
}

expect_show_table_status_row() {
    table_name=$1
    output=$(run_mysql "SHOW TABLE STATUS FROM sys LIKE '$table_name';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    suffix=$(printf '%s\n' "$output" | cut -f 13-18)

    if [ "$field_count" != "18" ]; then
        fail "SHOW TABLE STATUS sys.$table_name: expected 18 fields, got [$field_count]"
    fi
    expected_prefix=$(printf '%b' "$table_name\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL")
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "SHOW TABLE STATUS sys.$table_name: expected prefix [$expected_prefix], got [$prefix]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *) fail "SHOW TABLE STATUS sys.$table_name: expected Create_time datetime, got [$create_time]" ;;
    esac
    expected_suffix=$(printf '%b' 'NULL\tNULL\tNULL\tNULL\tNULL\tVIEW')
    if [ "$suffix" != "$expected_suffix" ]; then
        fail "SHOW TABLE STATUS sys.$table_name: expected suffix [$expected_suffix], got [$suffix]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

show_columns_formatted_expected=$(
    printf '%b' \
'thread\tvarchar(317)\tYES\t\tNULL\t
file\tvarchar(512)\tYES\t\tNULL\t
latency\tvarchar(11)\tYES\t\tNULL\t
operation\tvarchar(32)\tNO\t\tNULL\t
requested\tvarchar(11)\tYES\t\tNULL\t'
)

show_columns_raw_expected=$(
    printf '%b' \
'thread\tvarchar(317)\tYES\t\tNULL\t
file\tvarchar(512)\tYES\t\tNULL\t
latency\tbigint unsigned\tYES\t\tNULL\t
operation\tvarchar(32)\tNO\t\tNULL\t
requested\tbigint\tYES\t\tNULL\t'
)

expect_output \
    "sys.latest_file_io SHOW COLUMNS" \
    "$show_columns_formatted_expected" \
    "SHOW COLUMNS FROM sys.latest_file_io;"

expect_output \
    "sys.latest_file_io DESCRIBE" \
    "$show_columns_formatted_expected" \
    "DESCRIBE sys.latest_file_io;"

expect_output \
    "sys.x latest_file_io SHOW COLUMNS" \
    "$show_columns_raw_expected" \
    "SHOW COLUMNS FROM sys.\`x\$latest_file_io\`;"

expect_output \
    "sys.x latest_file_io DESCRIBE" \
    "$show_columns_raw_expected" \
    "DESCRIBE sys.\`x\$latest_file_io\`;"

expect_output \
    "sys latest file io SHOW FULL formatted columns" \
    "$(printf '%b' 'latency\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\noperation\tvarchar(32)\tutf8mb4_0900_ai_ci\tNO\t\tNULL\t\tselect,insert,update,references\t\nrequested\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.latest_file_io
      WHERE Field IN ('latency', 'operation', 'requested');"

expect_output \
    "sys.x latest file io SHOW FULL raw columns" \
    "$(printf '%b' 'latency\tbigint unsigned\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\noperation\tvarchar(32)\tutf8mb4_0900_ai_ci\tNO\t\tNULL\t\tselect,insert,update,references\t\nrequested\tbigint\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.\`x\$latest_file_io\`
      WHERE Field IN ('latency', 'operation', 'requested');"

expect_output \
    "sys latest file io SHOW INDEX" \
    "$(printf '%b' '\n')" \
    "SHOW INDEX FROM sys.latest_file_io;
     SHOW INDEX FROM sys.\`x\$latest_file_io\`; "

expect_output \
    "sys latest file io empty rows" \
    "$(printf '%b' '0\n0')" \
    "SELECT COUNT(*) FROM sys.latest_file_io;
     SELECT COUNT(*) FROM sys.\`x\$latest_file_io\`;"

expect_output \
    "sys latest file io selected schema rows" \
    "$(printf '%b' '0\n0')" \
    "USE sys;
     SELECT COUNT(*) FROM latest_file_io;
     SELECT COUNT(*) FROM \`x\$latest_file_io\`;"

expect_output \
    "sys latest file io INFORMATION_SCHEMA.COLUMNS" \
    "$(printf '%b' 'latest_file_io\tthread\t1\tYES\tvarchar(317)\tutf8mb4\tutf8mb4_0900_ai_ci\nlatest_file_io\tfile\t2\tYES\tvarchar(512)\tutf8mb4\tutf8mb4_0900_ai_ci\nlatest_file_io\tlatency\t3\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\nlatest_file_io\toperation\t4\tNO\tvarchar(32)\tutf8mb4\tutf8mb4_0900_ai_ci\nlatest_file_io\trequested\t5\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\nx$latest_file_io\tthread\t1\tYES\tvarchar(317)\tutf8mb4\tutf8mb4_0900_ai_ci\nx$latest_file_io\tfile\t2\tYES\tvarchar(512)\tutf8mb4\tutf8mb4_0900_ai_ci\nx$latest_file_io\tlatency\t3\tYES\tbigint unsigned\tNULL\tNULL\nx$latest_file_io\toperation\t4\tNO\tvarchar(32)\tutf8mb4\tutf8mb4_0900_ai_ci\nx$latest_file_io\trequested\t5\tYES\tbigint\tNULL\tNULL')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COALESCE(CHARACTER_SET_NAME, 'NULL'), COALESCE(COLLATION_NAME, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('latest_file_io', 'x\$latest_file_io')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "sys latest file io INFORMATION_SCHEMA.TABLES" \
    "$(printf '%b' 'sys\tlatest_file_io\tVIEW\tNULL\tNULL\tNULL\tVIEW\nsys\tx$latest_file_io\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, DATA_LENGTH,
            TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('latest_file_io', 'x\$latest_file_io')
      ORDER BY TABLE_NAME;"

expect_show_table_status_row "latest_file_io"
expect_show_table_status_row "x\$latest_file_io"

expect_output \
    "sys latest file io INFORMATION_SCHEMA.VIEWS" \
    "$(printf '%b' 'sys\tlatest_file_io\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nsys\tx$latest_file_io\tNONE\tNO\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('latest_file_io', 'x\$latest_file_io')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys latest file io dependency metadata" \
    "$(printf '%b' 'sys\tlatest_file_io\tinformation_schema\tPROCESSLIST\nsys\tlatest_file_io\tperformance_schema\tevents_waits_history_long\nsys\tlatest_file_io\tperformance_schema\tglobal_variables\nsys\tlatest_file_io\tperformance_schema\tthreads\nsys\tx$latest_file_io\tinformation_schema\tPROCESSLIST\nsys\tx$latest_file_io\tperformance_schema\tevents_waits_history_long\nsys\tx$latest_file_io\tperformance_schema\tthreads')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ('latest_file_io', 'x\$latest_file_io')
      ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;"

expect_output \
    "sys latest file io routine dependency metadata" \
    "$(printf '%b' 'sys\tlatest_file_io\tsys\tformat_path')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('latest_file_io', 'x\$latest_file_io')
      ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME;"

expect_output \
    "sys latest file io empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('latest_file_io', 'x\$latest_file_io')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('latest_file_io', 'x\$latest_file_io')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('latest_file_io', 'x\$latest_file_io')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys'
            AND TABLE_NAME IN ('latest_file_io', 'x\$latest_file_io'));"

expect_contains \
    "sys.latest_file_io SHOW CREATE VIEW" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`latest_file_io\`" \
    "SHOW CREATE VIEW sys.latest_file_io;"

expect_contains \
    "sys.latest_file_io formatted file path" \
    "\`sys\`.\`format_path\`(\`performance_schema\`.\`events_waits_history_long\`.\`OBJECT_NAME\`) AS \`file\`" \
    "SHOW CREATE VIEW sys.latest_file_io;"

expect_contains \
    "sys.x latest_file_io SHOW CREATE TABLE" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`x\$latest_file_io\`" \
    "USE sys; SHOW CREATE TABLE \`x\$latest_file_io\`;"

expect_contains \
    "sys.x latest_file_io raw file path" \
    "\`performance_schema\`.\`events_waits_history_long\`.\`OBJECT_NAME\` AS \`file\`" \
    "SHOW CREATE VIEW sys.\`x\$latest_file_io\`;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.latest_file_io; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.latest_file_io SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_latest_file_io_views_expectations: ok"
