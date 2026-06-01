#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_host_summary_by_statement_latency_views_expectations: $1" >&2
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
total\tdecimal(42,0)\tYES\t\tNULL\t
total_latency\tvarchar(11)\tYES\t\tNULL\t
max_latency\tvarchar(11)\tYES\t\tNULL\t
lock_latency\tvarchar(11)\tYES\t\tNULL\t
cpu_latency\tvarchar(11)\tYES\t\tNULL\t
rows_sent\tdecimal(42,0)\tYES\t\tNULL\t
rows_examined\tdecimal(42,0)\tYES\t\tNULL\t
rows_affected\tdecimal(42,0)\tYES\t\tNULL\t
full_scans\tdecimal(43,0)\tYES\t\tNULL\t'
)

raw_show_columns=$(
    printf '%b' \
'host\tvarchar(255)\tYES\t\tNULL\t
total\tdecimal(42,0)\tYES\t\tNULL\t
total_latency\tdecimal(42,0)\tYES\t\tNULL\t
max_latency\tbigint unsigned\tYES\t\tNULL\t
lock_latency\tdecimal(42,0)\tYES\t\tNULL\t
cpu_latency\tdecimal(42,0)\tYES\t\tNULL\t
rows_sent\tdecimal(42,0)\tYES\t\tNULL\t
rows_examined\tdecimal(42,0)\tYES\t\tNULL\t
rows_affected\tdecimal(42,0)\tYES\t\tNULL\t
full_scans\tdecimal(43,0)\tYES\t\tNULL\t'
)

expect_output \
    "sys host summary by statement latency SHOW COLUMNS" \
    "$formatted_show_columns" \
    "SHOW COLUMNS FROM sys.host_summary_by_statement_latency;"

expect_output \
    "sys x host summary by statement latency SHOW COLUMNS" \
    "$raw_show_columns" \
    "SHOW COLUMNS FROM sys.\`x\$host_summary_by_statement_latency\`;"

expect_output \
    "sys host summary by statement latency SHOW FULL columns" \
    "$(printf '%b' 'host\tvarchar(255)\tascii_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\ntotal\tdecimal(42,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\ntotal_latency\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\nmax_latency\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\nlock_latency\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\ncpu_latency\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\nrows_sent\tdecimal(42,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\nrows_examined\tdecimal(42,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\nrows_affected\tdecimal(42,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\nfull_scans\tdecimal(43,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.host_summary_by_statement_latency;"

expect_output \
    "sys x host summary by statement latency SHOW FULL columns" \
    "$(printf '%b' 'host\tvarchar(255)\tascii_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\ntotal\tdecimal(42,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\ntotal_latency\tdecimal(42,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\nmax_latency\tbigint unsigned\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\nlock_latency\tdecimal(42,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\ncpu_latency\tdecimal(42,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\nrows_sent\tdecimal(42,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\nrows_examined\tdecimal(42,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\nrows_affected\tdecimal(42,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\nfull_scans\tdecimal(43,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.\`x\$host_summary_by_statement_latency\`;"

expect_output \
    "sys host summary by statement latency INFORMATION_SCHEMA.COLUMNS" \
    "$(printf '%b' 'host_summary_by_statement_latency\thost\t1\tYES\tvarchar(255)\tascii\tascii_general_ci\t255\t255\tNULL\tNULL\nhost_summary_by_statement_latency\ttotal\t2\tYES\tdecimal(42,0)\tNULL\tNULL\tNULL\tNULL\t42\t0\nhost_summary_by_statement_latency\ttotal_latency\t3\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\t11\t33\tNULL\tNULL\nhost_summary_by_statement_latency\tmax_latency\t4\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\t11\t33\tNULL\tNULL\nhost_summary_by_statement_latency\tlock_latency\t5\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\t11\t33\tNULL\tNULL\nhost_summary_by_statement_latency\tcpu_latency\t6\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\t11\t33\tNULL\tNULL\nhost_summary_by_statement_latency\trows_sent\t7\tYES\tdecimal(42,0)\tNULL\tNULL\tNULL\tNULL\t42\t0\nhost_summary_by_statement_latency\trows_examined\t8\tYES\tdecimal(42,0)\tNULL\tNULL\tNULL\tNULL\t42\t0\nhost_summary_by_statement_latency\trows_affected\t9\tYES\tdecimal(42,0)\tNULL\tNULL\tNULL\tNULL\t42\t0\nhost_summary_by_statement_latency\tfull_scans\t10\tYES\tdecimal(43,0)\tNULL\tNULL\tNULL\tNULL\t43\t0\nx$host_summary_by_statement_latency\thost\t1\tYES\tvarchar(255)\tascii\tascii_general_ci\t255\t255\tNULL\tNULL\nx$host_summary_by_statement_latency\ttotal\t2\tYES\tdecimal(42,0)\tNULL\tNULL\tNULL\tNULL\t42\t0\nx$host_summary_by_statement_latency\ttotal_latency\t3\tYES\tdecimal(42,0)\tNULL\tNULL\tNULL\tNULL\t42\t0\nx$host_summary_by_statement_latency\tmax_latency\t4\tYES\tbigint unsigned\tNULL\tNULL\tNULL\tNULL\t20\t0\nx$host_summary_by_statement_latency\tlock_latency\t5\tYES\tdecimal(42,0)\tNULL\tNULL\tNULL\tNULL\t42\t0\nx$host_summary_by_statement_latency\tcpu_latency\t6\tYES\tdecimal(42,0)\tNULL\tNULL\tNULL\tNULL\t42\t0\nx$host_summary_by_statement_latency\trows_sent\t7\tYES\tdecimal(42,0)\tNULL\tNULL\tNULL\tNULL\t42\t0\nx$host_summary_by_statement_latency\trows_examined\t8\tYES\tdecimal(42,0)\tNULL\tNULL\tNULL\tNULL\t42\t0\nx$host_summary_by_statement_latency\trows_affected\t9\tYES\tdecimal(42,0)\tNULL\tNULL\tNULL\tNULL\t42\t0\nx$host_summary_by_statement_latency\tfull_scans\t10\tYES\tdecimal(43,0)\tNULL\tNULL\tNULL\tNULL\t43\t0')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COALESCE(CHARACTER_SET_NAME, 'NULL'), COALESCE(COLLATION_NAME, 'NULL'),
            COALESCE(CHARACTER_MAXIMUM_LENGTH, 'NULL'),
            COALESCE(CHARACTER_OCTET_LENGTH, 'NULL'),
            COALESCE(NUMERIC_PRECISION, 'NULL'), COALESCE(NUMERIC_SCALE, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('host_summary_by_statement_latency', 'x\$host_summary_by_statement_latency')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "sys host summary by statement latency INFORMATION_SCHEMA.TABLES" \
    "$(printf '%b' 'sys\thost_summary_by_statement_latency\tVIEW\tNULL\tNULL\tNULL\tVIEW\nsys\tx$host_summary_by_statement_latency\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, DATA_LENGTH,
            TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('host_summary_by_statement_latency', 'x\$host_summary_by_statement_latency')
      ORDER BY TABLE_NAME;"

expect_show_table_status_row "host_summary_by_statement_latency"
expect_show_table_status_row "x\$host_summary_by_statement_latency"

expect_output \
    "sys host summary by statement latency INFORMATION_SCHEMA.VIEWS" \
    "$(printf '%b' 'sys\thost_summary_by_statement_latency\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nsys\tx$host_summary_by_statement_latency\tNONE\tNO\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('host_summary_by_statement_latency', 'x\$host_summary_by_statement_latency')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys host summary by statement latency table dependency metadata" \
    "$(printf '%b' 'sys\thost_summary_by_statement_latency\tperformance_schema\tevents_statements_summary_by_host_by_event_name\nsys\tx$host_summary_by_statement_latency\tperformance_schema\tevents_statements_summary_by_host_by_event_name')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ('host_summary_by_statement_latency', 'x\$host_summary_by_statement_latency')
      ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;"

expect_output \
    "sys host summary by statement latency empty routine dependency metadata" \
    "" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('host_summary_by_statement_latency', 'x\$host_summary_by_statement_latency')
      ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME;"

expect_output \
    "sys host summary by statement latency empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0\t0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'host_summary_by_statement_latency'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$host_summary_by_statement_latency'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'host_summary_by_statement_latency'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$host_summary_by_statement_latency'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'host_summary_by_statement_latency'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$host_summary_by_statement_latency'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'host_summary_by_statement_latency'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'x\$host_summary_by_statement_latency');"

expect_contains \
    "sys host summary by statement latency SHOW CREATE VIEW" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`host_summary_by_statement_latency\`" \
    "SHOW CREATE VIEW sys.host_summary_by_statement_latency;"

expect_contains \
    "sys host summary by statement latency SHOW CREATE source" \
    "from \`performance_schema\`.\`events_statements_summary_by_host_by_event_name\` group by" \
    "SHOW CREATE VIEW sys.host_summary_by_statement_latency;"

expect_contains \
    "sys x host summary by statement latency SHOW CREATE VIEW" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`x\$host_summary_by_statement_latency\`" \
    "SHOW CREATE VIEW sys.\`x\$host_summary_by_statement_latency\`;"

expect_contains \
    "sys host summary by statement latency SHOW CREATE TABLE selected schema" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`host_summary_by_statement_latency\`" \
    "USE sys; SHOW CREATE TABLE host_summary_by_statement_latency;"

expect_contains \
    "sys x host summary by statement latency SHOW CREATE TABLE selected schema" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`x\$host_summary_by_statement_latency\`" \
    "USE sys; SHOW CREATE TABLE \`x\$host_summary_by_statement_latency\`;"

expect_output \
    "sys host summary by statement latency has runtime rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.host_summary_by_statement_latency;"

expect_output \
    "sys x host summary by statement latency has runtime rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.\`x\$host_summary_by_statement_latency\`;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.host_summary_by_statement_latency; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys host summary by statement latency SELECT status: expected [0	-1], got [$status]"
fi

status=$(run_mysql "SELECT COUNT(*) FROM sys.\`x\$host_summary_by_statement_latency\`; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys x host summary by statement latency SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_host_summary_by_statement_latency_views_expectations: ok"
