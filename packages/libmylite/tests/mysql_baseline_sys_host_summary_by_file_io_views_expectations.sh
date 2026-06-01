#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_host_summary_by_file_io_views_expectations: $1" >&2
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

formatted_show_columns=$(
    printf '%b' \
'host\tvarchar(255)\tYES\t\tNULL\t
ios\tdecimal(42,0)\tYES\t\tNULL\t
io_latency\tvarchar(11)\tYES\t\tNULL\t'
)

raw_show_columns=$(
    printf '%b' \
'host\tvarchar(255)\tYES\t\tNULL\t
ios\tdecimal(42,0)\tYES\t\tNULL\t
io_latency\tdecimal(42,0)\tYES\t\tNULL\t'
)

expect_output \
    "sys host summary by file io SHOW COLUMNS" \
    "$formatted_show_columns" \
    "SHOW COLUMNS FROM sys.host_summary_by_file_io;"

expect_output \
    "sys x host summary by file io SHOW COLUMNS" \
    "$raw_show_columns" \
    "SHOW COLUMNS FROM sys.\`x\$host_summary_by_file_io\`;"

expect_output \
    "sys host summary by file io SHOW FULL columns" \
    "$(printf '%b' 'host\tvarchar(255)\tascii_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\nios\tdecimal(42,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\nio_latency\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.host_summary_by_file_io;"

expect_output \
    "sys x host summary by file io SHOW FULL columns" \
    "$(printf '%b' 'host\tvarchar(255)\tascii_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\nios\tdecimal(42,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\nio_latency\tdecimal(42,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.\`x\$host_summary_by_file_io\`;"

expect_output \
    "sys host summary by file io INFORMATION_SCHEMA.COLUMNS" \
    "$(printf '%b' 'host_summary_by_file_io\thost\t1\tYES\tvarchar(255)\tascii\tascii_general_ci\nhost_summary_by_file_io\tios\t2\tYES\tdecimal(42,0)\tNULL\tNULL\nhost_summary_by_file_io\tio_latency\t3\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\nx$host_summary_by_file_io\thost\t1\tYES\tvarchar(255)\tascii\tascii_general_ci\nx$host_summary_by_file_io\tios\t2\tYES\tdecimal(42,0)\tNULL\tNULL\nx$host_summary_by_file_io\tio_latency\t3\tYES\tdecimal(42,0)\tNULL\tNULL')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COALESCE(CHARACTER_SET_NAME, 'NULL'), COALESCE(COLLATION_NAME, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('host_summary_by_file_io', 'x\$host_summary_by_file_io')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "sys host summary by file io INFORMATION_SCHEMA.TABLES" \
    "$(printf '%b' 'sys\thost_summary_by_file_io\tVIEW\tNULL\tNULL\tNULL\tVIEW\nsys\tx$host_summary_by_file_io\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, DATA_LENGTH,
            TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('host_summary_by_file_io', 'x\$host_summary_by_file_io')
      ORDER BY TABLE_NAME;"

expect_show_table_status_row "host_summary_by_file_io"
expect_show_table_status_row "x\$host_summary_by_file_io"

expect_output \
    "sys host summary by file io INFORMATION_SCHEMA.VIEWS" \
    "$(printf '%b' 'sys\thost_summary_by_file_io\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nsys\tx$host_summary_by_file_io\tNONE\tNO\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('host_summary_by_file_io', 'x\$host_summary_by_file_io')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys host summary by file io table dependency metadata" \
    "$(printf '%b' 'sys\thost_summary_by_file_io\tperformance_schema\tevents_waits_summary_by_host_by_event_name\nsys\tx$host_summary_by_file_io\tperformance_schema\tevents_waits_summary_by_host_by_event_name')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ('host_summary_by_file_io', 'x\$host_summary_by_file_io')
      ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;"

expect_output \
    "sys host summary by file io empty routine dependency metadata" \
    "" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('host_summary_by_file_io', 'x\$host_summary_by_file_io')
      ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME;"

expect_output \
    "sys host summary by file io empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0\t0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'host_summary_by_file_io'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$host_summary_by_file_io'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'host_summary_by_file_io'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$host_summary_by_file_io'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'host_summary_by_file_io'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$host_summary_by_file_io'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'host_summary_by_file_io'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'x\$host_summary_by_file_io');"

expect_contains \
    "sys host summary by file io SHOW CREATE VIEW" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`host_summary_by_file_io\`" \
    "SHOW CREATE VIEW sys.host_summary_by_file_io;"

expect_contains \
    "sys host summary by file io SHOW CREATE source" \
    "from \`performance_schema\`.\`events_waits_summary_by_host_by_event_name\` where" \
    "SHOW CREATE VIEW sys.host_summary_by_file_io;"

expect_contains \
    "sys x host summary by file io SHOW CREATE VIEW" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`x\$host_summary_by_file_io\`" \
    "SHOW CREATE VIEW sys.\`x\$host_summary_by_file_io\`;"

expect_contains \
    "sys host summary by file io SHOW CREATE TABLE selected schema" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`host_summary_by_file_io\`" \
    "USE sys; SHOW CREATE TABLE host_summary_by_file_io;"

expect_contains \
    "sys x host summary by file io SHOW CREATE TABLE selected schema" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`x\$host_summary_by_file_io\`" \
    "USE sys; SHOW CREATE TABLE \`x\$host_summary_by_file_io\`;"

expect_output \
    "sys host summary by file io has runtime rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.host_summary_by_file_io;"

expect_output \
    "sys x host summary by file io has runtime rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.\`x\$host_summary_by_file_io\`;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.host_summary_by_file_io; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys host summary by file io SELECT status: expected [0	-1], got [$status]"
fi

status=$(run_mysql "SELECT COUNT(*) FROM sys.\`x\$host_summary_by_file_io\`; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys x host summary by file io SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_host_summary_by_file_io_views_expectations: ok"
