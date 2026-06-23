#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_metrics_view_expectations: $1" >&2
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
    "sys.metrics SHOW COLUMNS" \
    "$(printf '%b' 'Variable_name\tvarchar(193)\tNO\t\t\t\nVariable_value\ttext\tYES\t\tNULL\t\nType\tvarchar(210)\tNO\t\t\t\nEnabled\tvarchar(7)\tNO\t\t\t')" \
    "SHOW COLUMNS FROM sys.metrics;"

expect_output \
    "sys.metrics SHOW FULL COLUMNS" \
    "$(printf '%b' 'Variable_name\tvarchar(193)\tutf8mb4_0900_ai_ci\tNO\t\t\t\tselect,insert,update,references\t\nVariable_value\ttext\tutf8mb4_0900_ai_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\nType\tvarchar(210)\tutf8mb3_general_ci\tNO\t\t\t\tselect,insert,update,references\t\nEnabled\tvarchar(7)\tutf8mb4_0900_ai_ci\tNO\t\t\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.metrics;"

expect_output \
    "sys.metrics INFORMATION_SCHEMA.COLUMNS" \
    "$(printf '%b' 'Variable_name\t1\tNO\tvarchar(193)\tutf8mb4\tutf8mb4_0900_ai_ci\t193\t772\nVariable_value\t2\tYES\ttext\tutf8mb4\tutf8mb4_0900_ai_ci\t65535\t65535\nType\t3\tNO\tvarchar(210)\tutf8mb3\tutf8mb3_general_ci\t210\t630\nEnabled\t4\tNO\tvarchar(7)\tutf8mb4\tutf8mb4_0900_ai_ci\t7\t28')" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            CHARACTER_SET_NAME, COLLATION_NAME,
            CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'metrics'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "sys.metrics INFORMATION_SCHEMA.TABLES" \
    "$(printf '%b' 'sys\tmetrics\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, COALESCE(ENGINE, 'NULL'),
            COALESCE(TABLE_ROWS, 'NULL'), COALESCE(DATA_LENGTH, 'NULL'), TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'metrics';"

expect_show_table_status_row "metrics"

expect_output \
    "sys.metrics INFORMATION_SCHEMA.VIEWS" \
    "$(printf '%b' 'sys\tmetrics\tNONE\tNO\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'metrics';"

expect_output \
    "sys.metrics table dependency metadata" \
    "$(printf '%b' 'sys\tmetrics\tinformation_schema\tINNODB_METRICS\nsys\tmetrics\tperformance_schema\tglobal_status\nsys\tmetrics\tperformance_schema\tmemory_summary_global_by_event_name\nsys\tmetrics\tperformance_schema\tsetup_instruments')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME = 'metrics'
      ORDER BY VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;"

expect_output \
    "sys.metrics empty routine dependency metadata" \
    "" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'metrics'
      ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME;"

expect_output \
    "sys.metrics empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'metrics'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'metrics'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'metrics'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'metrics');"

expect_contains \
    "sys.metrics SHOW CREATE VIEW" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`metrics\`" \
    "SHOW CREATE VIEW sys.metrics;"

expect_contains \
    "sys.metrics SHOW CREATE global status source" \
    "select lower(\`performance_schema\`.\`global_status\`.\`VARIABLE_NAME\`) AS \`Variable_name\`" \
    "SHOW CREATE VIEW sys.metrics;"

expect_contains \
    "sys.metrics SHOW CREATE memory source" \
    "\`performance_schema\`.\`memory_summary_global_by_event_name\`" \
    "SHOW CREATE VIEW sys.metrics;"

expect_contains \
    "sys.metrics SHOW CREATE TABLE selected schema" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`metrics\`" \
    "USE sys; SHOW CREATE TABLE metrics;"

expect_output \
    "sys.metrics has runtime rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.metrics;"

expect_output \
    "sys.metrics has global status rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.metrics WHERE Type = 'Global Status';"

expect_output \
    "sys.metrics has InnoDB metric rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.metrics WHERE Type LIKE 'InnoDB Metrics - %';"

expect_output \
    "sys.metrics has Performance Schema rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.metrics WHERE Type = 'Performance Schema';"

expect_output \
    "sys.metrics has system time rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.metrics WHERE Type = 'System Time';"

expect_output \
    "sys.metrics sample global status row" \
    "$(printf '%b' 'aborted_clients\tGlobal Status\tYES')" \
    "SELECT Variable_name, Type, Enabled
       FROM sys.metrics
      WHERE Variable_name = 'aborted_clients';"

status=$(run_mysql "SELECT COUNT(*) FROM sys.metrics; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.metrics SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_metrics_view_expectations: ok"
