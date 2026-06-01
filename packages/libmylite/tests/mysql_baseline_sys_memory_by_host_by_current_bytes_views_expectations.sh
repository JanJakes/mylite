#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_memory_by_host_by_current_bytes_views_expectations: $1" >&2
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
current_count_used\tdecimal(41,0)\tYES\t\tNULL\t
current_allocated\tvarchar(11)\tYES\t\tNULL\t
current_avg_alloc\tvarchar(11)\tYES\t\tNULL\t
current_max_alloc\tvarchar(11)\tYES\t\tNULL\t
total_allocated\tvarchar(11)\tYES\t\tNULL\t'
)

raw_show_columns=$(
    printf '%b' \
'host\tvarchar(255)\tYES\t\tNULL\t
current_count_used\tdecimal(41,0)\tYES\t\tNULL\t
current_allocated\tdecimal(41,0)\tYES\t\tNULL\t
current_avg_alloc\tdecimal(45,4)\tNO\t\t0.0000\t
current_max_alloc\tbigint\tYES\t\tNULL\t
total_allocated\tdecimal(42,0)\tYES\t\tNULL\t'
)

expect_output \
    "sys memory by host current bytes SHOW COLUMNS" \
    "$formatted_show_columns" \
    "SHOW COLUMNS FROM sys.memory_by_host_by_current_bytes;"

expect_output \
    "sys x memory by host current bytes SHOW COLUMNS" \
    "$raw_show_columns" \
    "SHOW COLUMNS FROM sys.\`x\$memory_by_host_by_current_bytes\`;"

expect_output \
    "sys memory by host current bytes SHOW FULL columns" \
    "$(printf '%b' 'host\tvarchar(255)\tascii_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\ncurrent_count_used\tdecimal(41,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\ncurrent_allocated\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\ncurrent_avg_alloc\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\ncurrent_max_alloc\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\ntotal_allocated\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.memory_by_host_by_current_bytes;"

expect_output \
    "sys x memory by host current bytes SHOW FULL columns" \
    "$(printf '%b' 'host\tvarchar(255)\tascii_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\ncurrent_count_used\tdecimal(41,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\ncurrent_allocated\tdecimal(41,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\ncurrent_avg_alloc\tdecimal(45,4)\tNULL\tNO\t\t0.0000\t\tselect,insert,update,references\t\ncurrent_max_alloc\tbigint\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t\ntotal_allocated\tdecimal(42,0)\tNULL\tYES\t\tNULL\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.\`x\$memory_by_host_by_current_bytes\`;"

expect_output \
    "sys memory by host current bytes INFORMATION_SCHEMA.COLUMNS" \
    "$(printf '%b' 'memory_by_host_by_current_bytes\thost\t1\tYES\tvarchar(255)\tascii\tascii_general_ci\t255\t255\tNULL\tNULL\nmemory_by_host_by_current_bytes\tcurrent_count_used\t2\tYES\tdecimal(41,0)\tNULL\tNULL\tNULL\tNULL\t41\t0\nmemory_by_host_by_current_bytes\tcurrent_allocated\t3\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\t11\t33\tNULL\tNULL\nmemory_by_host_by_current_bytes\tcurrent_avg_alloc\t4\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\t11\t33\tNULL\tNULL\nmemory_by_host_by_current_bytes\tcurrent_max_alloc\t5\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\t11\t33\tNULL\tNULL\nmemory_by_host_by_current_bytes\ttotal_allocated\t6\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\t11\t33\tNULL\tNULL\nx$memory_by_host_by_current_bytes\thost\t1\tYES\tvarchar(255)\tascii\tascii_general_ci\t255\t255\tNULL\tNULL\nx$memory_by_host_by_current_bytes\tcurrent_count_used\t2\tYES\tdecimal(41,0)\tNULL\tNULL\tNULL\tNULL\t41\t0\nx$memory_by_host_by_current_bytes\tcurrent_allocated\t3\tYES\tdecimal(41,0)\tNULL\tNULL\tNULL\tNULL\t41\t0\nx$memory_by_host_by_current_bytes\tcurrent_avg_alloc\t4\tNO\tdecimal(45,4)\tNULL\tNULL\tNULL\tNULL\t45\t4\nx$memory_by_host_by_current_bytes\tcurrent_max_alloc\t5\tYES\tbigint\tNULL\tNULL\tNULL\tNULL\t19\t0\nx$memory_by_host_by_current_bytes\ttotal_allocated\t6\tYES\tdecimal(42,0)\tNULL\tNULL\tNULL\tNULL\t42\t0')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COALESCE(CHARACTER_SET_NAME, 'NULL'), COALESCE(COLLATION_NAME, 'NULL'),
            COALESCE(CHARACTER_MAXIMUM_LENGTH, 'NULL'),
            COALESCE(CHARACTER_OCTET_LENGTH, 'NULL'),
            COALESCE(NUMERIC_PRECISION, 'NULL'), COALESCE(NUMERIC_SCALE, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('memory_by_host_by_current_bytes', 'x\$memory_by_host_by_current_bytes')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "sys memory by host current bytes INFORMATION_SCHEMA.TABLES" \
    "$(printf '%b' 'sys\tmemory_by_host_by_current_bytes\tVIEW\tNULL\tNULL\tNULL\tVIEW\nsys\tx$memory_by_host_by_current_bytes\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, COALESCE(ENGINE, 'NULL'),
            COALESCE(TABLE_ROWS, 'NULL'), COALESCE(DATA_LENGTH, 'NULL'), TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('memory_by_host_by_current_bytes', 'x\$memory_by_host_by_current_bytes')
      ORDER BY TABLE_NAME;"

expect_show_table_status_row "memory_by_host_by_current_bytes"
expect_show_table_status_row "x\$memory_by_host_by_current_bytes"

expect_output \
    "sys memory by host current bytes INFORMATION_SCHEMA.VIEWS" \
    "$(printf '%b' 'sys\tmemory_by_host_by_current_bytes\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nsys\tx$memory_by_host_by_current_bytes\tNONE\tNO\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('memory_by_host_by_current_bytes', 'x\$memory_by_host_by_current_bytes')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys memory by host current bytes table dependency metadata" \
    "$(printf '%b' 'sys\tmemory_by_host_by_current_bytes\tperformance_schema\tmemory_summary_by_host_by_event_name\nsys\tx$memory_by_host_by_current_bytes\tperformance_schema\tmemory_summary_by_host_by_event_name')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ('memory_by_host_by_current_bytes', 'x\$memory_by_host_by_current_bytes')
      ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;"

expect_output \
    "sys memory by host current bytes empty routine dependency metadata" \
    "" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('memory_by_host_by_current_bytes', 'x\$memory_by_host_by_current_bytes')
      ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME;"

expect_output \
    "sys memory by host current bytes empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0\t0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'memory_by_host_by_current_bytes'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$memory_by_host_by_current_bytes'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'memory_by_host_by_current_bytes'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$memory_by_host_by_current_bytes'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'memory_by_host_by_current_bytes'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$memory_by_host_by_current_bytes'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'memory_by_host_by_current_bytes'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'x\$memory_by_host_by_current_bytes');"

expect_contains \
    "sys memory by host current bytes SHOW CREATE VIEW" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`memory_by_host_by_current_bytes\`" \
    "SHOW CREATE VIEW sys.memory_by_host_by_current_bytes;"

expect_contains \
    "sys memory by host current bytes SHOW CREATE source" \
    "from \`performance_schema\`.\`memory_summary_by_host_by_event_name\` group by" \
    "SHOW CREATE VIEW sys.memory_by_host_by_current_bytes;"

expect_contains \
    "sys memory by host current bytes SHOW CREATE grouping" \
    "group by if((\`performance_schema\`.\`memory_summary_by_host_by_event_name\`.\`HOST\` is null),'background',\`performance_schema\`.\`memory_summary_by_host_by_event_name\`.\`HOST\`) order by sum(\`performance_schema\`.\`memory_summary_by_host_by_event_name\`.\`CURRENT_NUMBER_OF_BYTES_USED\`) desc" \
    "SHOW CREATE VIEW sys.memory_by_host_by_current_bytes;"

expect_contains \
    "sys x memory by host current bytes SHOW CREATE VIEW" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`x\$memory_by_host_by_current_bytes\`" \
    "SHOW CREATE VIEW sys.\`x\$memory_by_host_by_current_bytes\`;"

expect_contains \
    "sys memory by host current bytes SHOW CREATE TABLE selected schema" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`memory_by_host_by_current_bytes\`" \
    "USE sys; SHOW CREATE TABLE memory_by_host_by_current_bytes;"

expect_contains \
    "sys x memory by host current bytes SHOW CREATE TABLE selected schema" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`x\$memory_by_host_by_current_bytes\`" \
    "USE sys; SHOW CREATE TABLE \`x\$memory_by_host_by_current_bytes\`;"

expect_output \
    "sys memory by host current bytes has runtime rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.memory_by_host_by_current_bytes;"

expect_output \
    "sys x memory by host current bytes has runtime rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.\`x\$memory_by_host_by_current_bytes\`;"

expect_output \
    "sys memory by host current bytes selected rows" \
    "1" \
    "USE sys; SELECT COUNT(*) > 0 FROM memory_by_host_by_current_bytes;"

expect_output \
    "sys x memory by host current bytes selected rows" \
    "1" \
    "USE sys; SELECT COUNT(*) > 0 FROM \`x\$memory_by_host_by_current_bytes\`;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.memory_by_host_by_current_bytes; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys memory by host current bytes SELECT status: expected [0	-1], got [$status]"
fi

status=$(run_mysql "SELECT COUNT(*) FROM sys.\`x\$memory_by_host_by_current_bytes\`; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys x memory by host current bytes SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_memory_by_host_by_current_bytes_views_expectations: ok"
