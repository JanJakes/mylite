#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_statement_sort_temp_views_expectations: $1" >&2
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

views="'statements_with_sorting', 'x\$statements_with_sorting',
       'statements_with_temp_tables', 'x\$statements_with_temp_tables'"

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "statements_with_sorting SHOW COLUMNS" \
    "$(printf '%b' 'query\tlongtext\tYES\t\tNULL\t\ndb\tvarchar(64)\tYES\t\tNULL\t\nexec_count\tbigint unsigned\tNO\t\tNULL\t\ntotal_latency\tvarchar(11)\tYES\t\tNULL\t\nsort_merge_passes\tbigint unsigned\tNO\t\tNULL\t\navg_sort_merges\tdecimal(21,0)\tNO\t\t0\t\nsorts_using_scans\tbigint unsigned\tNO\t\tNULL\t\nsort_using_range\tbigint unsigned\tNO\t\tNULL\t\nrows_sorted\tbigint unsigned\tNO\t\tNULL\t\navg_rows_sorted\tdecimal(21,0)\tNO\t\t0\t\nfirst_seen\ttimestamp(6)\tNO\t\tNULL\t\nlast_seen\ttimestamp(6)\tNO\t\tNULL\t\ndigest\tvarchar(64)\tYES\t\tNULL\t')" \
    "SHOW COLUMNS FROM sys.statements_with_sorting;"

expect_output \
    "x statements_with_sorting latency metadata" \
    "$(printf '%b' 'total_latency\tbigint unsigned\tNO\tNULL\navg_sort_merges\tdecimal(21,0)\tNO\t0\navg_rows_sorted\tdecimal(21,0)\tNO\t0')" \
    "SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COALESCE(COLUMN_DEFAULT, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'x\$statements_with_sorting'
        AND COLUMN_NAME IN ('total_latency', 'avg_sort_merges',
                            'avg_rows_sorted')
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "statements_with_temp_tables SHOW COLUMNS" \
    "$(printf '%b' 'query\tlongtext\tYES\t\tNULL\t\ndb\tvarchar(64)\tYES\t\tNULL\t\nexec_count\tbigint unsigned\tNO\t\tNULL\t\ntotal_latency\tvarchar(11)\tYES\t\tNULL\t\nmemory_tmp_tables\tbigint unsigned\tNO\t\tNULL\t\ndisk_tmp_tables\tbigint unsigned\tNO\t\tNULL\t\navg_tmp_tables_per_query\tdecimal(21,0)\tNO\t\t0\t\ntmp_tables_to_disk_pct\tdecimal(24,0)\tNO\t\t0\t\nfirst_seen\ttimestamp(6)\tNO\t\tNULL\t\nlast_seen\ttimestamp(6)\tNO\t\tNULL\t\ndigest\tvarchar(64)\tYES\t\tNULL\t')" \
    "SHOW COLUMNS FROM sys.statements_with_temp_tables;"

expect_output \
    "x statements_with_temp_tables latency metadata" \
    "$(printf '%b' 'total_latency\tbigint unsigned\tNO\tNULL\navg_tmp_tables_per_query\tdecimal(21,0)\tNO\t0\ntmp_tables_to_disk_pct\tdecimal(24,0)\tNO\t0')" \
    "SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COALESCE(COLUMN_DEFAULT, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'x\$statements_with_temp_tables'
        AND COLUMN_NAME IN ('total_latency', 'avg_tmp_tables_per_query',
                            'tmp_tables_to_disk_pct')
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "statement sorting/temp column counts" \
    "$(printf '%b' 'statements_with_sorting\t13\nstatements_with_temp_tables\t11\nx$statements_with_sorting\t13\nx$statements_with_temp_tables\t11')" \
    "SELECT TABLE_NAME, COUNT(*)
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ($views)
      GROUP BY TABLE_NAME
      ORDER BY TABLE_NAME;"

expect_output \
    "statement sorting/temp INFORMATION_SCHEMA.TABLES" \
    "$(printf '%b' 'statements_with_sorting\tVIEW\tNULL\tNULL\tVIEW\nstatements_with_temp_tables\tVIEW\tNULL\tNULL\tVIEW\nx$statements_with_sorting\tVIEW\tNULL\tNULL\tVIEW\nx$statements_with_temp_tables\tVIEW\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ($views)
      ORDER BY TABLE_NAME;"

for view in statements_with_sorting x\$statements_with_sorting \
    statements_with_temp_tables x\$statements_with_temp_tables; do
    expect_show_table_status_row "$view"
done

expect_output \
    "statement sorting/temp INFORMATION_SCHEMA.VIEWS" \
    "$(printf '%b' 'statements_with_sorting\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nstatements_with_temp_tables\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nx$statements_with_sorting\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nx$statements_with_temp_tables\tNONE\tYES\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ($views)
      ORDER BY TABLE_NAME;"

expect_output \
    "statement sorting/temp dependency metadata" \
    "$(printf '%b' 'statements_with_sorting\tperformance_schema\tevents_statements_summary_by_digest\nstatements_with_sorting\tsys\tsys_config\nstatements_with_temp_tables\tperformance_schema\tevents_statements_summary_by_digest\nstatements_with_temp_tables\tsys\tsys_config\nx$statements_with_sorting\tperformance_schema\tevents_statements_summary_by_digest\nx$statements_with_temp_tables\tperformance_schema\tevents_statements_summary_by_digest')" \
    "SELECT VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ($views)
      ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;"

expect_output \
    "statement sorting/temp routine dependency metadata" \
    "$(printf '%b' 'statements_with_sorting\tsys\tformat_statement\nstatements_with_temp_tables\tsys\tformat_statement')" \
    "SELECT TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ($views)
      ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME;"

expect_output \
    "statement sorting/temp empty index and constraints" \
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
    "statements_with_sorting SHOW CREATE VIEW" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`statements_with_sorting\`" \
    "SHOW CREATE VIEW sys.statements_with_sorting;"

expect_contains \
    "x statements_with_sorting SHOW CREATE raw query" \
    "\`DIGEST_TEXT\` AS \`query\`" \
    "SHOW CREATE VIEW sys.\`x\$statements_with_sorting\`;"

expect_contains \
    "statements_with_temp_tables SHOW CREATE ordering" \
    "\`SUM_CREATED_TMP_DISK_TABLES\` desc" \
    "SHOW CREATE VIEW sys.statements_with_temp_tables;"

expect_contains \
    "selected schema SHOW CREATE" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`statements_with_temp_tables\`" \
    "USE sys; SHOW CREATE TABLE statements_with_temp_tables;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.statements_with_sorting; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.statements_with_sorting SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_statement_sort_temp_views_expectations: ok"
