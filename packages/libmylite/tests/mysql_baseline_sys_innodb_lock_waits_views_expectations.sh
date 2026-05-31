#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_innodb_lock_waits_views_expectations: $1" >&2
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
'wait_started\tdatetime\tYES\t\tNULL\t
wait_age\ttime\tYES\t\tNULL\t
wait_age_secs\tbigint\tYES\t\tNULL\t
locked_table\tmediumtext\tYES\t\tNULL\t
locked_table_schema\tvarchar(64)\tYES\t\tNULL\t
locked_table_name\tvarchar(64)\tYES\t\tNULL\t
locked_table_partition\tvarchar(64)\tYES\t\tNULL\t
locked_table_subpartition\tvarchar(64)\tYES\t\tNULL\t
locked_index\tvarchar(64)\tYES\t\tNULL\t
locked_type\tvarchar(32)\tNO\t\tNULL\t
waiting_trx_id\tbigint unsigned\tNO\t\t0\t
waiting_trx_started\tdatetime\tNO\t\t0000-00-00 00:00:00\t
waiting_trx_age\ttime\tYES\t\tNULL\t
waiting_trx_rows_locked\tbigint unsigned\tNO\t\t0\t
waiting_trx_rows_modified\tbigint unsigned\tNO\t\t0\t
waiting_pid\tbigint unsigned\tNO\t\t0\t
waiting_query\tlongtext\tYES\t\tNULL\t
waiting_lock_id\tvarchar(128)\tNO\t\tNULL\t
waiting_lock_mode\tvarchar(32)\tNO\t\tNULL\t
blocking_trx_id\tbigint unsigned\tNO\t\t0\t
blocking_pid\tbigint unsigned\tNO\t\t0\t
blocking_query\tlongtext\tYES\t\tNULL\t
blocking_lock_id\tvarchar(128)\tNO\t\tNULL\t
blocking_lock_mode\tvarchar(32)\tNO\t\tNULL\t
blocking_trx_started\tdatetime\tNO\t\t0000-00-00 00:00:00\t
blocking_trx_age\ttime\tYES\t\tNULL\t
blocking_trx_rows_locked\tbigint unsigned\tNO\t\t0\t
blocking_trx_rows_modified\tbigint unsigned\tNO\t\t0\t
sql_kill_blocking_query\tvarchar(33)\tNO\t\t\t
sql_kill_blocking_connection\tvarchar(27)\tNO\t\t\t'
)

show_columns_raw_expected=$(printf '%s\n' "$show_columns_formatted_expected" | sed \
    -e 's/waiting_query\tlongtext/waiting_query\tvarchar(1024)/' \
    -e 's/blocking_query\tlongtext/blocking_query\tvarchar(1024)/')

expect_output \
    "sys.innodb_lock_waits SHOW COLUMNS" \
    "$show_columns_formatted_expected" \
    "SHOW COLUMNS FROM sys.innodb_lock_waits;"

expect_output \
    "sys.innodb_lock_waits DESCRIBE" \
    "$show_columns_formatted_expected" \
    "DESCRIBE sys.innodb_lock_waits;"

expect_output \
    "sys.x innodb_lock_waits SHOW COLUMNS" \
    "$show_columns_raw_expected" \
    "SHOW COLUMNS FROM sys.\`x\$innodb_lock_waits\`;"

expect_output \
    "sys.x innodb_lock_waits DESCRIBE" \
    "$show_columns_raw_expected" \
    "DESCRIBE sys.\`x\$innodb_lock_waits\`;"

expect_output \
    "sys innodb lock waits SHOW FULL query columns" \
    "$(printf '%b' 'waiting_query\tlongtext\tutf8mb4_0900_ai_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\nblocking_query\tlongtext\tutf8mb4_0900_ai_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\nsql_kill_blocking_query\tvarchar(33)\tutf8mb4_0900_ai_ci\tNO\t\t\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.innodb_lock_waits
      WHERE Field IN ('waiting_query', 'blocking_query', 'sql_kill_blocking_query');"

expect_output \
    "sys.x innodb lock waits SHOW FULL query columns" \
    "$(printf '%b' 'waiting_query\tvarchar(1024)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\nblocking_query\tvarchar(1024)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\nsql_kill_blocking_query\tvarchar(33)\tutf8mb4_0900_ai_ci\tNO\t\t\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.\`x\$innodb_lock_waits\`
      WHERE Field IN ('waiting_query', 'blocking_query', 'sql_kill_blocking_query');"

expect_output \
    "sys innodb lock waits SHOW INDEX" \
    "$(printf '%b' '\n')" \
    "SHOW INDEX FROM sys.innodb_lock_waits;
     SHOW INDEX FROM sys.\`x\$innodb_lock_waits\`; "

expect_output \
    "sys innodb lock waits empty rows" \
    "$(printf '%b' '0\n0')" \
    "SELECT COUNT(*) FROM sys.innodb_lock_waits;
     SELECT COUNT(*) FROM sys.\`x\$innodb_lock_waits\`;"

expect_output \
    "sys innodb lock waits selected schema rows" \
    "$(printf '%b' '0\n0')" \
    "USE sys;
     SELECT COUNT(*) FROM innodb_lock_waits;
     SELECT COUNT(*) FROM \`x\$innodb_lock_waits\`;"

expect_output \
    "sys innodb lock waits INFORMATION_SCHEMA.COLUMNS sample" \
    "$(printf '%b' 'innodb_lock_waits\twaiting_query\t17\tYES\tlongtext\tutf8mb4\tutf8mb4_0900_ai_ci\ninnodb_lock_waits\tblocking_query\t22\tYES\tlongtext\tutf8mb4\tutf8mb4_0900_ai_ci\ninnodb_lock_waits\tsql_kill_blocking_query\t29\tNO\tvarchar(33)\tutf8mb4\tutf8mb4_0900_ai_ci\nx$innodb_lock_waits\twaiting_query\t17\tYES\tvarchar(1024)\tutf8mb3\tutf8mb3_general_ci\nx$innodb_lock_waits\tblocking_query\t22\tYES\tvarchar(1024)\tutf8mb3\tutf8mb3_general_ci\nx$innodb_lock_waits\tsql_kill_blocking_query\t29\tNO\tvarchar(33)\tutf8mb4\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COALESCE(CHARACTER_SET_NAME, 'NULL'), COALESCE(COLLATION_NAME, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('innodb_lock_waits', 'x\$innodb_lock_waits')
        AND COLUMN_NAME IN ('waiting_query', 'blocking_query', 'sql_kill_blocking_query')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "sys innodb lock waits INFORMATION_SCHEMA.COLUMNS count" \
    "60" \
    "SELECT COUNT(*)
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('innodb_lock_waits', 'x\$innodb_lock_waits');"

expect_output \
    "sys innodb lock waits INFORMATION_SCHEMA.TABLES" \
    "$(printf '%b' 'sys\tinnodb_lock_waits\tVIEW\tNULL\tNULL\tNULL\tVIEW\nsys\tx$innodb_lock_waits\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, DATA_LENGTH,
            TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('innodb_lock_waits', 'x\$innodb_lock_waits')
      ORDER BY TABLE_NAME;"

expect_show_table_status_row "innodb_lock_waits"
expect_show_table_status_row "x\$innodb_lock_waits"

expect_output \
    "sys innodb lock waits INFORMATION_SCHEMA.VIEWS" \
    "$(printf '%b' 'sys\tinnodb_lock_waits\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nsys\tx$innodb_lock_waits\tNONE\tNO\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('innodb_lock_waits', 'x\$innodb_lock_waits')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys innodb lock waits dependency metadata" \
    "$(printf '%b' 'sys\tinnodb_lock_waits\tinformation_schema\tINNODB_TRX\nsys\tinnodb_lock_waits\tperformance_schema\tdata_lock_waits\nsys\tinnodb_lock_waits\tperformance_schema\tdata_locks\nsys\tinnodb_lock_waits\tsys\tsys_config\nsys\tx$innodb_lock_waits\tinformation_schema\tINNODB_TRX\nsys\tx$innodb_lock_waits\tperformance_schema\tdata_lock_waits\nsys\tx$innodb_lock_waits\tperformance_schema\tdata_locks')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ('innodb_lock_waits', 'x\$innodb_lock_waits')
      ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;"

expect_output \
    "sys innodb lock waits routine dependency metadata" \
    "$(printf '%b' 'sys\tinnodb_lock_waits\tsys\tformat_statement\nsys\tinnodb_lock_waits\tsys\tquote_identifier\nsys\tx$innodb_lock_waits\tsys\tquote_identifier')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('innodb_lock_waits', 'x\$innodb_lock_waits')
      ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME;"

expect_output \
    "sys innodb lock waits empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('innodb_lock_waits', 'x\$innodb_lock_waits')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('innodb_lock_waits', 'x\$innodb_lock_waits')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('innodb_lock_waits', 'x\$innodb_lock_waits')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys'
            AND TABLE_NAME IN ('innodb_lock_waits', 'x\$innodb_lock_waits'));"

expect_contains \
    "sys.innodb_lock_waits SHOW CREATE VIEW" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`innodb_lock_waits\`" \
    "SHOW CREATE VIEW sys.innodb_lock_waits;"

expect_contains \
    "sys.innodb_lock_waits formatted waiting query" \
    "\`sys\`.\`format_statement\`(\`r\`.\`trx_query\`) AS \`waiting_query\`" \
    "SHOW CREATE VIEW sys.innodb_lock_waits;"

expect_contains \
    "sys.x innodb_lock_waits SHOW CREATE TABLE" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`x\$innodb_lock_waits\`" \
    "USE sys; SHOW CREATE TABLE \`x\$innodb_lock_waits\`;"

expect_contains \
    "sys.x innodb_lock_waits raw waiting query" \
    "\`r\`.\`trx_query\` AS \`waiting_query\`" \
    "SHOW CREATE VIEW sys.\`x\$innodb_lock_waits\`;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.innodb_lock_waits; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.innodb_lock_waits SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_innodb_lock_waits_views_expectations: ok"
