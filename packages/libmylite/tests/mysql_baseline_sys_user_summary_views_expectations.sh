#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_user_summary_views_expectations: $1" >&2
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

views="'user_summary', 'x\$user_summary',
       'user_summary_by_file_io', 'x\$user_summary_by_file_io',
       'user_summary_by_file_io_type', 'x\$user_summary_by_file_io_type',
       'user_summary_by_stages', 'x\$user_summary_by_stages',
       'user_summary_by_statement_latency', 'x\$user_summary_by_statement_latency',
       'user_summary_by_statement_type', 'x\$user_summary_by_statement_type'"

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "user_summary SHOW COLUMNS" \
    "$(printf '%b' 'user\tvarchar(32)\tYES\t\tNULL\t\nstatements\tdecimal(64,0)\tYES\t\tNULL\t\nstatement_latency\tvarchar(11)\tYES\t\tNULL\t\nstatement_avg_latency\tvarchar(11)\tYES\t\tNULL\t\ntable_scans\tdecimal(65,0)\tYES\t\tNULL\t\nfile_ios\tdecimal(64,0)\tYES\t\tNULL\t\nfile_io_latency\tvarchar(11)\tYES\t\tNULL\t\ncurrent_connections\tdecimal(41,0)\tYES\t\tNULL\t\ntotal_connections\tdecimal(41,0)\tYES\t\tNULL\t\nunique_hosts\tbigint\tNO\t\t0\t\ncurrent_memory\tvarchar(11)\tYES\t\tNULL\t\ntotal_memory_allocated\tvarchar(11)\tYES\t\tNULL\t')" \
    "SHOW COLUMNS FROM sys.user_summary;"

expect_output \
    "x user_summary SHOW COLUMNS" \
    "$(printf '%b' 'user\tvarchar(32)\tYES\t\tNULL\t\nstatements\tdecimal(64,0)\tYES\t\tNULL\t\nstatement_latency\tdecimal(64,0)\tYES\t\tNULL\t\nstatement_avg_latency\tdecimal(65,4)\tNO\t\t0.0000\t\ntable_scans\tdecimal(65,0)\tYES\t\tNULL\t\nfile_ios\tdecimal(64,0)\tYES\t\tNULL\t\nfile_io_latency\tdecimal(64,0)\tYES\t\tNULL\t\ncurrent_connections\tdecimal(41,0)\tYES\t\tNULL\t\ntotal_connections\tdecimal(41,0)\tYES\t\tNULL\t\nunique_hosts\tbigint\tNO\t\t0\t\ncurrent_memory\tdecimal(63,0)\tYES\t\tNULL\t\ntotal_memory_allocated\tdecimal(64,0)\tYES\t\tNULL\t')" \
    "SHOW COLUMNS FROM sys.\`x\$user_summary\`;"

expect_output \
    "user_summary_by_file_io_type SHOW COLUMNS" \
    "$(printf '%b' 'user\tvarchar(32)\tYES\t\tNULL\t\nevent_name\tvarchar(128)\tNO\t\tNULL\t\ntotal\tbigint unsigned\tNO\t\tNULL\t\nlatency\tvarchar(11)\tYES\t\tNULL\t\nmax_latency\tvarchar(11)\tYES\t\tNULL\t')" \
    "SHOW COLUMNS FROM sys.user_summary_by_file_io_type;"

expect_output \
    "x user_summary_by_statement_latency raw metadata" \
    "$(printf '%b' 'max_latency\tdecimal(42,0)\tYES\tNULL\nstatement_avg_latency\tdecimal(65,4)\tNO\t0.0000')" \
    "SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COALESCE(COLUMN_DEFAULT, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND ((TABLE_NAME = 'x\$user_summary_by_statement_latency'
              AND COLUMN_NAME = 'max_latency')
          OR (TABLE_NAME = 'x\$user_summary'
              AND COLUMN_NAME = 'statement_avg_latency'))
      ORDER BY COLUMN_NAME;"

expect_output \
    "user summary column counts" \
    "$(printf '%b' 'user_summary\t12\nuser_summary_by_file_io\t3\nuser_summary_by_file_io_type\t5\nuser_summary_by_stages\t5\nuser_summary_by_statement_latency\t10\nuser_summary_by_statement_type\t11\nx$user_summary\t12\nx$user_summary_by_file_io\t3\nx$user_summary_by_file_io_type\t5\nx$user_summary_by_stages\t5\nx$user_summary_by_statement_latency\t10\nx$user_summary_by_statement_type\t11')" \
    "SELECT TABLE_NAME, COUNT(*)
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ($views)
      GROUP BY TABLE_NAME
      ORDER BY TABLE_NAME;"

expect_output \
    "user summary INFORMATION_SCHEMA.TABLES" \
    "$(printf '%b' 'user_summary\tVIEW\tNULL\tNULL\tVIEW\nuser_summary_by_file_io\tVIEW\tNULL\tNULL\tVIEW\nuser_summary_by_file_io_type\tVIEW\tNULL\tNULL\tVIEW\nuser_summary_by_stages\tVIEW\tNULL\tNULL\tVIEW\nuser_summary_by_statement_latency\tVIEW\tNULL\tNULL\tVIEW\nuser_summary_by_statement_type\tVIEW\tNULL\tNULL\tVIEW\nx$user_summary\tVIEW\tNULL\tNULL\tVIEW\nx$user_summary_by_file_io\tVIEW\tNULL\tNULL\tVIEW\nx$user_summary_by_file_io_type\tVIEW\tNULL\tNULL\tVIEW\nx$user_summary_by_stages\tVIEW\tNULL\tNULL\tVIEW\nx$user_summary_by_statement_latency\tVIEW\tNULL\tNULL\tVIEW\nx$user_summary_by_statement_type\tVIEW\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ($views)
      ORDER BY TABLE_NAME;"

for view in user_summary x\$user_summary \
    user_summary_by_file_io x\$user_summary_by_file_io \
    user_summary_by_file_io_type x\$user_summary_by_file_io_type \
    user_summary_by_stages x\$user_summary_by_stages \
    user_summary_by_statement_latency x\$user_summary_by_statement_latency \
    user_summary_by_statement_type x\$user_summary_by_statement_type; do
    expect_show_table_status_row "$view"
done

expect_output \
    "user summary INFORMATION_SCHEMA.VIEWS" \
    "$(printf '%b' 'user_summary\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nuser_summary_by_file_io\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nuser_summary_by_file_io_type\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nuser_summary_by_stages\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nuser_summary_by_statement_latency\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nuser_summary_by_statement_type\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nx$user_summary\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nx$user_summary_by_file_io\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nx$user_summary_by_file_io_type\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nx$user_summary_by_stages\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nx$user_summary_by_statement_latency\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nx$user_summary_by_statement_type\tNONE\tYES\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ($views)
      ORDER BY TABLE_NAME;"

expect_output \
    "user summary dependency metadata" \
    "$(printf '%b' 'user_summary\tperformance_schema\taccounts\nuser_summary\tsys\tx$memory_by_user_by_current_bytes\nuser_summary\tsys\tx$user_summary_by_file_io\nuser_summary\tsys\tx$user_summary_by_statement_latency\nuser_summary_by_file_io\tperformance_schema\tevents_waits_summary_by_user_by_event_name\nuser_summary_by_file_io_type\tperformance_schema\tevents_waits_summary_by_user_by_event_name\nuser_summary_by_stages\tperformance_schema\tevents_stages_summary_by_user_by_event_name\nuser_summary_by_statement_latency\tperformance_schema\tevents_statements_summary_by_user_by_event_name\nuser_summary_by_statement_type\tperformance_schema\tevents_statements_summary_by_user_by_event_name\nx$user_summary\tperformance_schema\taccounts\nx$user_summary\tsys\tx$memory_by_user_by_current_bytes\nx$user_summary\tsys\tx$user_summary_by_file_io\nx$user_summary\tsys\tx$user_summary_by_statement_latency\nx$user_summary_by_file_io\tperformance_schema\tevents_waits_summary_by_user_by_event_name\nx$user_summary_by_file_io_type\tperformance_schema\tevents_waits_summary_by_user_by_event_name\nx$user_summary_by_stages\tperformance_schema\tevents_stages_summary_by_user_by_event_name\nx$user_summary_by_statement_latency\tperformance_schema\tevents_statements_summary_by_user_by_event_name\nx$user_summary_by_statement_type\tperformance_schema\tevents_statements_summary_by_user_by_event_name')" \
    "SELECT VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ($views)
      ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;"

expect_output \
    "user summary empty routine dependency metadata" \
    "" \
    "SELECT TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ($views)
      ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME;"

expect_output \
    "user summary empty index and constraints" \
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
    "user_summary SHOW CREATE VIEW" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`user_summary\`" \
    "SHOW CREATE VIEW sys.user_summary;"

expect_contains \
    "user_summary SHOW CREATE left join" \
    "left join \`sys\`.\`x\$user_summary_by_statement_latency\` \`stmt\`" \
    "SHOW CREATE VIEW sys.user_summary;"

expect_contains \
    "x user_summary_by_file_io_type SHOW CREATE raw latency" \
    "\`SUM_TIMER_WAIT\` AS \`latency\`" \
    "SHOW CREATE VIEW sys.\`x\$user_summary_by_file_io_type\`;"

expect_contains \
    "selected schema SHOW CREATE" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`user_summary\`" \
    "USE sys; SHOW CREATE TABLE user_summary;"

expect_output \
    "sys user summary has runtime rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.user_summary;"

printf '%s\n' "mysql_baseline_sys_user_summary_views_expectations: ok"
