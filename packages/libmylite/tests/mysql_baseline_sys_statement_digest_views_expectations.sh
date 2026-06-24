#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_statement_digest_views_expectations: $1" >&2
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

views="'statement_analysis', 'x\$statement_analysis',
       'statements_with_errors_or_warnings', 'x\$statements_with_errors_or_warnings',
       'statements_with_full_table_scans', 'x\$statements_with_full_table_scans',
       'statements_with_runtimes_in_95th_percentile',
       'x\$statements_with_runtimes_in_95th_percentile'"

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "statement_analysis SHOW COLUMNS" \
    "$(printf '%b' 'query\tlongtext\tYES\t\tNULL\t\nfull_scan\tvarchar(1)\tNO\t\t\t\ntotal_latency\tvarchar(11)\tYES\t\tNULL\t\nmax_controlled_memory\tvarchar(11)\tYES\t\tNULL\t\nfirst_seen\ttimestamp(6)\tNO\t\tNULL\t')" \
    "SHOW COLUMNS FROM sys.statement_analysis
      WHERE Field IN ('query', 'full_scan', 'total_latency',
                      'max_controlled_memory', 'first_seen');"

expect_output \
    "x statement_analysis SHOW COLUMNS" \
    "$(printf '%b' 'exec_secondary_count\tbigint unsigned\tNO\t\tNULL\t\ntotal_latency\tbigint unsigned\tNO\t\tNULL\t\nmax_controlled_memory\tbigint unsigned\tNO\t\tNULL\t\nfirst_seen\ttimestamp(6)\tNO\t\tNULL\t')" \
    "SHOW COLUMNS FROM sys.\`x\$statement_analysis\`
      WHERE Field IN ('exec_secondary_count', 'total_latency',
                      'max_controlled_memory', 'first_seen');"

expect_output \
    "statements errors SHOW COLUMNS" \
    "$(printf '%b' 'query\tlongtext\tYES\t\tNULL\t\ndb\tvarchar(64)\tYES\t\tNULL\t\nexec_count\tbigint unsigned\tNO\t\tNULL\t\nerrors\tbigint unsigned\tNO\t\tNULL\t\nerror_pct\tdecimal(27,4)\tNO\t\t0.0000\t\nwarnings\tbigint unsigned\tNO\t\tNULL\t\nwarning_pct\tdecimal(27,4)\tNO\t\t0.0000\t\nfirst_seen\ttimestamp(6)\tNO\t\tNULL\t\nlast_seen\ttimestamp(6)\tNO\t\tNULL\t\ndigest\tvarchar(64)\tYES\t\tNULL\t')" \
    "SHOW COLUMNS FROM sys.statements_with_errors_or_warnings;"

expect_output \
    "full scan formatted/raw SHOW COLUMNS" \
    "$(printf '%b' 'statements_with_full_table_scans\ttotal_latency\tvarchar(11)\tYES\tNULL\nstatements_with_full_table_scans\tno_index_used_pct\tdecimal(24,0)\tNO\t0\nstatements_with_full_table_scans\trows_sent_avg\tdecimal(21,0)\tYES\tNULL\nx$statements_with_full_table_scans\ttotal_latency\tbigint unsigned\tNO\tNULL\nx$statements_with_full_table_scans\tno_index_used_pct\tdecimal(24,0)\tNO\t0\nx$statements_with_full_table_scans\trows_sent_avg\tdecimal(21,0)\tYES\tNULL')" \
    "SELECT TABLE_NAME, COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE,
            COALESCE(COLUMN_DEFAULT, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('statements_with_full_table_scans',
                           'x\$statements_with_full_table_scans')
        AND COLUMN_NAME IN ('total_latency', 'rows_sent_avg',
                            'no_index_used_pct')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "runtime percentile formatted/raw SHOW COLUMNS" \
    "$(printf '%b' 'statements_with_runtimes_in_95th_percentile\tfull_scan\tvarchar(1)\tNO\t\nstatements_with_runtimes_in_95th_percentile\ttotal_latency\tvarchar(11)\tYES\tNULL\nstatements_with_runtimes_in_95th_percentile\trows_sent_avg\tdecimal(21,0)\tNO\t0\nx$statements_with_runtimes_in_95th_percentile\tfull_scan\tvarchar(1)\tNO\t\nx$statements_with_runtimes_in_95th_percentile\ttotal_latency\tbigint unsigned\tNO\tNULL\nx$statements_with_runtimes_in_95th_percentile\trows_sent_avg\tdecimal(21,0)\tNO\t0')" \
    "SELECT TABLE_NAME, COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE,
            COALESCE(COLUMN_DEFAULT, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('statements_with_runtimes_in_95th_percentile',
                           'x\$statements_with_runtimes_in_95th_percentile')
        AND COLUMN_NAME IN ('full_scan', 'total_latency', 'rows_sent_avg')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "statement digest column counts" \
    "$(printf '%b' 'statement_analysis\t26\nstatements_with_errors_or_warnings\t10\nstatements_with_full_table_scans\t14\nstatements_with_runtimes_in_95th_percentile\t16\nx$statement_analysis\t27\nx$statements_with_errors_or_warnings\t10\nx$statements_with_full_table_scans\t14\nx$statements_with_runtimes_in_95th_percentile\t16')" \
    "SELECT TABLE_NAME, COUNT(*)
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ($views)
      GROUP BY TABLE_NAME
      ORDER BY TABLE_NAME;"

expect_output \
    "statement digest INFORMATION_SCHEMA.TABLES" \
    "$(printf '%b' 'statement_analysis\tVIEW\tNULL\tNULL\tVIEW\nstatements_with_errors_or_warnings\tVIEW\tNULL\tNULL\tVIEW\nstatements_with_full_table_scans\tVIEW\tNULL\tNULL\tVIEW\nstatements_with_runtimes_in_95th_percentile\tVIEW\tNULL\tNULL\tVIEW\nx$statement_analysis\tVIEW\tNULL\tNULL\tVIEW\nx$statements_with_errors_or_warnings\tVIEW\tNULL\tNULL\tVIEW\nx$statements_with_full_table_scans\tVIEW\tNULL\tNULL\tVIEW\nx$statements_with_runtimes_in_95th_percentile\tVIEW\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ($views)
      ORDER BY TABLE_NAME;"

for view in statement_analysis x\$statement_analysis \
    statements_with_errors_or_warnings x\$statements_with_errors_or_warnings \
    statements_with_full_table_scans x\$statements_with_full_table_scans \
    statements_with_runtimes_in_95th_percentile \
    x\$statements_with_runtimes_in_95th_percentile; do
    expect_show_table_status_row "$view"
done

expect_output \
    "statement digest INFORMATION_SCHEMA.VIEWS" \
    "$(printf '%b' 'statement_analysis\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nstatements_with_errors_or_warnings\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nstatements_with_full_table_scans\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nstatements_with_runtimes_in_95th_percentile\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nx$statement_analysis\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nx$statements_with_errors_or_warnings\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nx$statements_with_full_table_scans\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nx$statements_with_runtimes_in_95th_percentile\tNONE\tYES\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ($views)
      ORDER BY TABLE_NAME;"

expect_output \
    "statement digest dependency metadata" \
    "$(printf '%b' 'statement_analysis\tperformance_schema\tevents_statements_summary_by_digest\nstatement_analysis\tsys\tsys_config\nstatements_with_errors_or_warnings\tperformance_schema\tevents_statements_summary_by_digest\nstatements_with_errors_or_warnings\tsys\tsys_config\nstatements_with_full_table_scans\tperformance_schema\tevents_statements_summary_by_digest\nstatements_with_full_table_scans\tsys\tsys_config\nstatements_with_runtimes_in_95th_percentile\tperformance_schema\tevents_statements_summary_by_digest\nstatements_with_runtimes_in_95th_percentile\tsys\tsys_config\nstatements_with_runtimes_in_95th_percentile\tsys\tx$ps_digest_95th_percentile_by_avg_us\nx$statement_analysis\tperformance_schema\tevents_statements_summary_by_digest\nx$statements_with_errors_or_warnings\tperformance_schema\tevents_statements_summary_by_digest\nx$statements_with_full_table_scans\tperformance_schema\tevents_statements_summary_by_digest\nx$statements_with_runtimes_in_95th_percentile\tperformance_schema\tevents_statements_summary_by_digest\nx$statements_with_runtimes_in_95th_percentile\tsys\tx$ps_digest_95th_percentile_by_avg_us')" \
    "SELECT VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ($views)
      ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;"

expect_output \
    "statement digest routine dependency metadata" \
    "$(printf '%b' 'statement_analysis\tsys\tformat_statement\nstatements_with_errors_or_warnings\tsys\tformat_statement\nstatements_with_full_table_scans\tsys\tformat_statement\nstatements_with_runtimes_in_95th_percentile\tsys\tformat_statement')" \
    "SELECT TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ($views)
      ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME;"

expect_output \
    "statement digest empty index and constraints" \
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
    "statement_analysis SHOW CREATE VIEW" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`statement_analysis\`" \
    "SHOW CREATE VIEW sys.statement_analysis;"

expect_contains \
    "x statement_analysis SHOW CREATE raw counter" \
    "\`COUNT_SECONDARY\` AS \`exec_secondary_count\`" \
    "SHOW CREATE VIEW sys.\`x\$statement_analysis\`;"

expect_contains \
    "runtime percentile SHOW CREATE helper dependency" \
    "\`sys\`.\`x\$ps_digest_95th_percentile_by_avg_us\`" \
    "SHOW CREATE VIEW sys.statements_with_runtimes_in_95th_percentile;"

expect_contains \
    "selected schema SHOW CREATE" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`statements_with_full_table_scans\`" \
    "USE sys; SHOW CREATE TABLE statements_with_full_table_scans;"

mysql_has_statement_digest_rows=$(run_mysql "SELECT COUNT(*) > 0 FROM sys.statement_analysis;")
if [ "$mysql_has_statement_digest_rows" != "1" ]; then
    fail "expected MySQL runtime to expose live statement digest rows"
fi

status=$(run_mysql "SELECT COUNT(*) FROM sys.statement_analysis; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.statement_analysis SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_statement_digest_views_expectations: ok"
