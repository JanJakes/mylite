#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_ps_digest_helper_views_expectations: $1" >&2
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

expect_output \
    "sys digest avg latency SHOW COLUMNS" \
    "$(printf '%b' 'cnt\tbigint\tNO\t\t0\t\navg_us\tdecimal(21,0)\tYES\t\tNULL\t')" \
    "SHOW COLUMNS FROM sys.\`x\$ps_digest_avg_latency_distribution\`;"

expect_output \
    "sys digest percentile SHOW COLUMNS" \
    "$(printf '%b' 'avg_us\tdecimal(21,0)\tYES\t\tNULL\t\npercentile\tdecimal(46,4)\tNO\t\t0.0000\t')" \
    "SHOW COLUMNS FROM sys.\`x\$ps_digest_95th_percentile_by_avg_us\`;"

expect_output \
    "sys digest avg latency SHOW FULL COLUMNS" \
    "$(printf '%b' 'cnt\tbigint\tNULL\tNO\t\t0\t\tselect,insert,update,references\t\navg_us\tdecimal(21,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.\`x\$ps_digest_avg_latency_distribution\`;"

expect_output \
    "sys digest percentile SHOW FULL COLUMNS" \
    "$(printf '%b' 'avg_us\tdecimal(21,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\npercentile\tdecimal(46,4)\tNULL\tNO\t\t0.0000\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.\`x\$ps_digest_95th_percentile_by_avg_us\`;"

expect_output \
    "sys digest helper INFORMATION_SCHEMA.COLUMNS" \
    "$(printf '%b' 'x$ps_digest_95th_percentile_by_avg_us\tavg_us\t1\tYES\tdecimal(21,0)\tNULL\tNULL\tNULL\tNULL\t21\t0\nx$ps_digest_95th_percentile_by_avg_us\tpercentile\t2\tNO\tdecimal(46,4)\tNULL\tNULL\tNULL\tNULL\t46\t4\nx$ps_digest_avg_latency_distribution\tcnt\t1\tNO\tbigint\tNULL\tNULL\tNULL\tNULL\t19\t0\nx$ps_digest_avg_latency_distribution\tavg_us\t2\tYES\tdecimal(21,0)\tNULL\tNULL\tNULL\tNULL\t21\t0')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COALESCE(CHARACTER_SET_NAME, 'NULL'), COALESCE(COLLATION_NAME, 'NULL'),
            COALESCE(CHARACTER_MAXIMUM_LENGTH, 'NULL'),
            COALESCE(CHARACTER_OCTET_LENGTH, 'NULL'),
            COALESCE(NUMERIC_PRECISION, 'NULL'), COALESCE(NUMERIC_SCALE, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('x\$ps_digest_avg_latency_distribution',
                           'x\$ps_digest_95th_percentile_by_avg_us')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "sys digest helper INFORMATION_SCHEMA.TABLES" \
    "$(printf '%b' 'sys\tx$ps_digest_95th_percentile_by_avg_us\tVIEW\tNULL\tNULL\tNULL\tVIEW\nsys\tx$ps_digest_avg_latency_distribution\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, COALESCE(ENGINE, 'NULL'),
            COALESCE(TABLE_ROWS, 'NULL'), COALESCE(DATA_LENGTH, 'NULL'), TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('x\$ps_digest_avg_latency_distribution',
                           'x\$ps_digest_95th_percentile_by_avg_us')
      ORDER BY TABLE_NAME;"

expect_show_table_status_row "x\$ps_digest_avg_latency_distribution"
expect_show_table_status_row "x\$ps_digest_95th_percentile_by_avg_us"

expect_output \
    "sys digest helper INFORMATION_SCHEMA.VIEWS" \
    "$(printf '%b' 'sys\tx$ps_digest_95th_percentile_by_avg_us\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nsys\tx$ps_digest_avg_latency_distribution\tNONE\tNO\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('x\$ps_digest_avg_latency_distribution',
                           'x\$ps_digest_95th_percentile_by_avg_us')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys digest helper table dependency metadata" \
    "$(printf '%b' 'sys\tx$ps_digest_95th_percentile_by_avg_us\tperformance_schema\tevents_statements_summary_by_digest\nsys\tx$ps_digest_95th_percentile_by_avg_us\tsys\tx$ps_digest_avg_latency_distribution\nsys\tx$ps_digest_avg_latency_distribution\tperformance_schema\tevents_statements_summary_by_digest')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ('x\$ps_digest_avg_latency_distribution',
                          'x\$ps_digest_95th_percentile_by_avg_us')
      ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;"

expect_output \
    "sys digest helper empty routine dependency metadata" \
    "" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('x\$ps_digest_avg_latency_distribution',
                           'x\$ps_digest_95th_percentile_by_avg_us')
      ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME;"

expect_output \
    "sys digest helper empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0\t0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$ps_digest_avg_latency_distribution'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$ps_digest_95th_percentile_by_avg_us'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$ps_digest_avg_latency_distribution'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$ps_digest_95th_percentile_by_avg_us'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$ps_digest_avg_latency_distribution'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$ps_digest_95th_percentile_by_avg_us'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys'
            AND TABLE_NAME = 'x\$ps_digest_avg_latency_distribution'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys'
            AND TABLE_NAME = 'x\$ps_digest_95th_percentile_by_avg_us');"

expect_contains \
    "sys digest avg latency SHOW CREATE VIEW" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`x\$ps_digest_avg_latency_distribution\`" \
    "SHOW CREATE VIEW sys.\`x\$ps_digest_avg_latency_distribution\`;"

expect_contains \
    "sys digest avg latency SHOW CREATE source" \
    "select count(0) AS \`cnt\`,round((\`performance_schema\`.\`events_statements_summary_by_digest\`.\`AVG_TIMER_WAIT\` / 1000000),0) AS \`avg_us\` from \`performance_schema\`.\`events_statements_summary_by_digest\` group by \`avg_us\`" \
    "SHOW CREATE VIEW sys.\`x\$ps_digest_avg_latency_distribution\`;"

expect_contains \
    "sys digest percentile SHOW CREATE VIEW" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`x\$ps_digest_95th_percentile_by_avg_us\`" \
    "SHOW CREATE VIEW sys.\`x\$ps_digest_95th_percentile_by_avg_us\`;"

expect_contains \
    "sys digest percentile SHOW CREATE helper dependency" \
    "\`sys\`.\`x\$ps_digest_avg_latency_distribution\` \`s1\` join \`sys\`.\`x\$ps_digest_avg_latency_distribution\` \`s2\`" \
    "SHOW CREATE VIEW sys.\`x\$ps_digest_95th_percentile_by_avg_us\`;"

expect_contains \
    "sys digest avg latency SHOW CREATE TABLE selected schema" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`x\$ps_digest_avg_latency_distribution\`" \
    "USE sys; SHOW CREATE TABLE \`x\$ps_digest_avg_latency_distribution\`;"

expect_contains \
    "sys digest percentile SHOW CREATE TABLE selected schema" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`x\$ps_digest_95th_percentile_by_avg_us\`" \
    "USE sys; SHOW CREATE TABLE \`x\$ps_digest_95th_percentile_by_avg_us\`;"

expect_output \
    "sys digest avg latency has runtime rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.\`x\$ps_digest_avg_latency_distribution\`;"

expect_output \
    "sys digest percentile has runtime rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.\`x\$ps_digest_95th_percentile_by_avg_us\`;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.\`x\$ps_digest_avg_latency_distribution\`; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys digest avg latency SELECT status: expected [0	-1], got [$status]"
fi

status=$(run_mysql "SELECT COUNT(*) FROM sys.\`x\$ps_digest_95th_percentile_by_avg_us\`; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys digest percentile SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_ps_digest_helper_views_expectations: ok"
