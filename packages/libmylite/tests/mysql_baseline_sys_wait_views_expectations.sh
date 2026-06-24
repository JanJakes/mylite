#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_wait_views_expectations: $1" >&2
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

views="'wait_classes_global_by_avg_latency', 'x\$wait_classes_global_by_avg_latency',
       'wait_classes_global_by_latency', 'x\$wait_classes_global_by_latency',
       'waits_by_host_by_latency', 'x\$waits_by_host_by_latency',
       'waits_by_user_by_latency', 'x\$waits_by_user_by_latency',
       'waits_global_by_latency', 'x\$waits_global_by_latency'"

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "wait class formatted SHOW COLUMNS" \
    "$(printf '%b' 'event_class\tvarchar(128)\tYES\t\tNULL\t\ntotal\tdecimal(42,0)\tYES\t\tNULL\t\ntotal_latency\tvarchar(11)\tYES\t\tNULL\t\nmin_latency\tvarchar(11)\tYES\t\tNULL\t\navg_latency\tvarchar(11)\tYES\t\tNULL\t\nmax_latency\tvarchar(11)\tYES\t\tNULL\t')" \
    "SHOW COLUMNS FROM sys.wait_classes_global_by_avg_latency;"

expect_output \
    "wait class raw SHOW COLUMNS" \
    "$(printf '%b' 'event_class\tvarchar(128)\tYES\t\tNULL\t\ntotal\tdecimal(42,0)\tYES\t\tNULL\t\ntotal_latency\tdecimal(42,0)\tYES\t\tNULL\t\nmin_latency\tbigint unsigned\tYES\t\tNULL\t\navg_latency\tdecimal(46,4)\tNO\t\t0.0000\t\nmax_latency\tbigint unsigned\tYES\t\tNULL\t')" \
    "SHOW COLUMNS FROM sys.\`x\$wait_classes_global_by_latency\`;"

expect_output \
    "waits by host formatted SHOW COLUMNS" \
    "$(printf '%b' 'host\tvarchar(255)\tYES\t\tNULL\t\nevent\tvarchar(128)\tNO\t\tNULL\t\ntotal\tbigint unsigned\tNO\t\tNULL\t\ntotal_latency\tvarchar(11)\tYES\t\tNULL\t\navg_latency\tvarchar(11)\tYES\t\tNULL\t\nmax_latency\tvarchar(11)\tYES\t\tNULL\t')" \
    "SHOW COLUMNS FROM sys.waits_by_host_by_latency;"

expect_output \
    "waits global raw SHOW COLUMNS" \
    "$(printf '%b' 'events\tvarchar(128)\tNO\t\tNULL\t\ntotal\tbigint unsigned\tNO\t\tNULL\t\ntotal_latency\tbigint unsigned\tNO\t\tNULL\t\navg_latency\tbigint unsigned\tNO\t\tNULL\t\nmax_latency\tbigint unsigned\tNO\t\tNULL\t')" \
    "SHOW COLUMNS FROM sys.\`x\$waits_global_by_latency\`;"

expect_output \
    "wait view column counts" \
    "$(printf '%b' 'wait_classes_global_by_avg_latency\t6\nwait_classes_global_by_latency\t6\nwaits_by_host_by_latency\t6\nwaits_by_user_by_latency\t6\nwaits_global_by_latency\t5\nx$wait_classes_global_by_avg_latency\t6\nx$wait_classes_global_by_latency\t6\nx$waits_by_host_by_latency\t6\nx$waits_by_user_by_latency\t6\nx$waits_global_by_latency\t5')" \
    "SELECT TABLE_NAME, COUNT(*)
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ($views)
      GROUP BY TABLE_NAME
      ORDER BY TABLE_NAME;"

expect_output \
    "wait special column metadata" \
    "$(printf '%b' 'host\tvarchar(255)\tYES\tascii\tascii_general_ci\nuser\tvarchar(32)\tYES\tutf8mb4\tutf8mb4_bin\nevents\tvarchar(128)\tNO\tutf8mb4\tutf8mb4_0900_ai_ci\navg_latency\tdecimal(46,4)\tNO\tNULL\tNULL')" \
    "SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE,
            COALESCE(CHARACTER_SET_NAME, 'NULL'),
            COALESCE(COLLATION_NAME, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND ((TABLE_NAME = 'waits_by_host_by_latency' AND COLUMN_NAME = 'host')
          OR (TABLE_NAME = 'waits_global_by_latency' AND COLUMN_NAME = 'events')
          OR (TABLE_NAME = 'waits_by_user_by_latency' AND COLUMN_NAME = 'user')
          OR (TABLE_NAME = 'x\$wait_classes_global_by_latency'
              AND COLUMN_NAME = 'avg_latency'))
      ORDER BY TABLE_NAME, COLUMN_NAME;"

expect_output \
    "wait view INFORMATION_SCHEMA.TABLES" \
    "$(printf '%b' 'wait_classes_global_by_avg_latency\tVIEW\tNULL\tNULL\tVIEW\nwait_classes_global_by_latency\tVIEW\tNULL\tNULL\tVIEW\nwaits_by_host_by_latency\tVIEW\tNULL\tNULL\tVIEW\nwaits_by_user_by_latency\tVIEW\tNULL\tNULL\tVIEW\nwaits_global_by_latency\tVIEW\tNULL\tNULL\tVIEW\nx$wait_classes_global_by_avg_latency\tVIEW\tNULL\tNULL\tVIEW\nx$wait_classes_global_by_latency\tVIEW\tNULL\tNULL\tVIEW\nx$waits_by_host_by_latency\tVIEW\tNULL\tNULL\tVIEW\nx$waits_by_user_by_latency\tVIEW\tNULL\tNULL\tVIEW\nx$waits_global_by_latency\tVIEW\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ($views)
      ORDER BY TABLE_NAME;"

for view in wait_classes_global_by_avg_latency x\$wait_classes_global_by_avg_latency \
    wait_classes_global_by_latency x\$wait_classes_global_by_latency \
    waits_by_host_by_latency x\$waits_by_host_by_latency \
    waits_by_user_by_latency x\$waits_by_user_by_latency \
    waits_global_by_latency x\$waits_global_by_latency; do
    expect_show_table_status_row "$view"
done

expect_output \
    "wait view INFORMATION_SCHEMA.VIEWS" \
    "$(printf '%b' 'wait_classes_global_by_avg_latency\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nwait_classes_global_by_latency\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nwaits_by_host_by_latency\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nwaits_by_user_by_latency\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nwaits_global_by_latency\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nx$wait_classes_global_by_avg_latency\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nx$wait_classes_global_by_latency\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nx$waits_by_host_by_latency\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nx$waits_by_user_by_latency\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nx$waits_global_by_latency\tNONE\tYES\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ($views)
      ORDER BY TABLE_NAME;"

expect_output \
    "wait view dependency metadata" \
    "$(printf '%b' 'wait_classes_global_by_avg_latency\tperformance_schema\tevents_waits_summary_global_by_event_name\nwait_classes_global_by_latency\tperformance_schema\tevents_waits_summary_global_by_event_name\nwaits_by_host_by_latency\tperformance_schema\tevents_waits_summary_by_host_by_event_name\nwaits_by_user_by_latency\tperformance_schema\tevents_waits_summary_by_user_by_event_name\nwaits_global_by_latency\tperformance_schema\tevents_waits_summary_global_by_event_name\nx$wait_classes_global_by_avg_latency\tperformance_schema\tevents_waits_summary_global_by_event_name\nx$wait_classes_global_by_latency\tperformance_schema\tevents_waits_summary_global_by_event_name\nx$waits_by_host_by_latency\tperformance_schema\tevents_waits_summary_by_host_by_event_name\nx$waits_by_user_by_latency\tperformance_schema\tevents_waits_summary_by_user_by_event_name\nx$waits_global_by_latency\tperformance_schema\tevents_waits_summary_global_by_event_name')" \
    "SELECT VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ($views)
      ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;"

expect_output \
    "wait empty routine dependency metadata" \
    "" \
    "SELECT TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ($views)
      ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME;"

expect_output \
    "wait empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN ($views)),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN ($views)),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME IN ($views)),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME IN ($views));"

expect_contains \
    "wait_classes SHOW CREATE" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`wait_classes_global_by_avg_latency\`" \
    "SHOW CREATE VIEW sys.wait_classes_global_by_avg_latency;"

expect_contains \
    "wait_classes SHOW CREATE avg expression" \
    "order by ifnull((sum(\`performance_schema\`.\`events_waits_summary_global_by_event_name\`.\`SUM_TIMER_WAIT\`) / nullif(sum(\`performance_schema\`.\`events_waits_summary_global_by_event_name\`.\`COUNT_STAR\`),0)),0) desc" \
    "SHOW CREATE VIEW sys.\`x\$wait_classes_global_by_avg_latency\`;"

expect_contains \
    "waits global SHOW CREATE event alias" \
    "\`EVENT_NAME\` AS \`event\`" \
    "SHOW CREATE VIEW sys.waits_global_by_latency;"

expect_contains \
    "selected schema SHOW CREATE" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`waits_by_host_by_latency\`" \
    "USE sys; SHOW CREATE TABLE waits_by_host_by_latency;"

printf '%s\n' "mysql_baseline_sys_wait_views_expectations: ok"
