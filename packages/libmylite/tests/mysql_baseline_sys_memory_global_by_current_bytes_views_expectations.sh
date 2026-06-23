#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_memory_global_by_current_bytes_views_expectations: $1" >&2
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
'event_name\tvarchar(128)\tNO\t\tNULL\t
current_count\tbigint\tNO\t\tNULL\t
current_alloc\tvarchar(11)\tYES\t\tNULL\t
current_avg_alloc\tvarchar(11)\tYES\t\tNULL\t
high_count\tbigint\tNO\t\tNULL\t
high_alloc\tvarchar(11)\tYES\t\tNULL\t
high_avg_alloc\tvarchar(11)\tYES\t\tNULL\t'
)

raw_show_columns=$(
    printf '%b' \
'event_name\tvarchar(128)\tNO\t\tNULL\t
current_count\tbigint\tNO\t\tNULL\t
current_alloc\tbigint\tNO\t\tNULL\t
current_avg_alloc\tdecimal(23,4)\tNO\t\t0.0000\t
high_count\tbigint\tNO\t\tNULL\t
high_alloc\tbigint\tNO\t\tNULL\t
high_avg_alloc\tdecimal(23,4)\tNO\t\t0.0000\t'
)

expect_output \
    "sys memory global current bytes SHOW COLUMNS" \
    "$formatted_show_columns" \
    "SHOW COLUMNS FROM sys.memory_global_by_current_bytes;"

expect_output \
    "sys x memory global current bytes SHOW COLUMNS" \
    "$raw_show_columns" \
    "SHOW COLUMNS FROM sys.\`x\$memory_global_by_current_bytes\`;"

expect_output \
    "sys memory global current bytes SHOW FULL columns" \
    "$(printf '%b' 'event_name\tvarchar(128)\tutf8mb4_0900_ai_ci\tNO\t\tNULL\t\tselect,insert,update,references\t\ncurrent_count\tbigint\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\ncurrent_alloc\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\ncurrent_avg_alloc\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\nhigh_count\tbigint\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\nhigh_alloc\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t\nhigh_avg_alloc\tvarchar(11)\tutf8mb3_general_ci\tYES\t\tNULL\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.memory_global_by_current_bytes;"

expect_output \
    "sys x memory global current bytes SHOW FULL columns" \
    "$(printf '%b' 'event_name\tvarchar(128)\tutf8mb4_0900_ai_ci\tNO\t\tNULL\t\tselect,insert,update,references\t\ncurrent_count\tbigint\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\ncurrent_alloc\tbigint\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\ncurrent_avg_alloc\tdecimal(23,4)\tNULL\tNO\t\t0.0000\t\tselect,insert,update,references\t\nhigh_count\tbigint\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\nhigh_alloc\tbigint\tNULL\tNO\t\tNULL\t\tselect,insert,update,references\t\nhigh_avg_alloc\tdecimal(23,4)\tNULL\tNO\t\t0.0000\t\tselect,insert,update,references\t')" \
    "SHOW FULL COLUMNS FROM sys.\`x\$memory_global_by_current_bytes\`;"

expect_output \
    "sys memory global current bytes INFORMATION_SCHEMA.COLUMNS" \
    "$(printf '%b' 'memory_global_by_current_bytes\tevent_name\t1\tNO\tvarchar(128)\tutf8mb4\tutf8mb4_0900_ai_ci\t128\t512\tNULL\tNULL\nmemory_global_by_current_bytes\tcurrent_count\t2\tNO\tbigint\tNULL\tNULL\tNULL\tNULL\t19\t0\nmemory_global_by_current_bytes\tcurrent_alloc\t3\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\t11\t33\tNULL\tNULL\nmemory_global_by_current_bytes\tcurrent_avg_alloc\t4\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\t11\t33\tNULL\tNULL\nmemory_global_by_current_bytes\thigh_count\t5\tNO\tbigint\tNULL\tNULL\tNULL\tNULL\t19\t0\nmemory_global_by_current_bytes\thigh_alloc\t6\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\t11\t33\tNULL\tNULL\nmemory_global_by_current_bytes\thigh_avg_alloc\t7\tYES\tvarchar(11)\tutf8mb3\tutf8mb3_general_ci\t11\t33\tNULL\tNULL\nx$memory_global_by_current_bytes\tevent_name\t1\tNO\tvarchar(128)\tutf8mb4\tutf8mb4_0900_ai_ci\t128\t512\tNULL\tNULL\nx$memory_global_by_current_bytes\tcurrent_count\t2\tNO\tbigint\tNULL\tNULL\tNULL\tNULL\t19\t0\nx$memory_global_by_current_bytes\tcurrent_alloc\t3\tNO\tbigint\tNULL\tNULL\tNULL\tNULL\t19\t0\nx$memory_global_by_current_bytes\tcurrent_avg_alloc\t4\tNO\tdecimal(23,4)\tNULL\tNULL\tNULL\tNULL\t23\t4\nx$memory_global_by_current_bytes\thigh_count\t5\tNO\tbigint\tNULL\tNULL\tNULL\tNULL\t19\t0\nx$memory_global_by_current_bytes\thigh_alloc\t6\tNO\tbigint\tNULL\tNULL\tNULL\tNULL\t19\t0\nx$memory_global_by_current_bytes\thigh_avg_alloc\t7\tNO\tdecimal(23,4)\tNULL\tNULL\tNULL\tNULL\t23\t4')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COALESCE(CHARACTER_SET_NAME, 'NULL'), COALESCE(COLLATION_NAME, 'NULL'),
            COALESCE(CHARACTER_MAXIMUM_LENGTH, 'NULL'),
            COALESCE(CHARACTER_OCTET_LENGTH, 'NULL'),
            COALESCE(NUMERIC_PRECISION, 'NULL'), COALESCE(NUMERIC_SCALE, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('memory_global_by_current_bytes', 'x\$memory_global_by_current_bytes')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "sys memory global current bytes INFORMATION_SCHEMA.TABLES" \
    "$(printf '%b' 'sys\tmemory_global_by_current_bytes\tVIEW\tNULL\tNULL\tNULL\tVIEW\nsys\tx$memory_global_by_current_bytes\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, COALESCE(ENGINE, 'NULL'),
            COALESCE(TABLE_ROWS, 'NULL'), COALESCE(DATA_LENGTH, 'NULL'), TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('memory_global_by_current_bytes', 'x\$memory_global_by_current_bytes')
      ORDER BY TABLE_NAME;"

expect_show_table_status_row "memory_global_by_current_bytes"
expect_show_table_status_row "x\$memory_global_by_current_bytes"

expect_output \
    "sys memory global current bytes INFORMATION_SCHEMA.VIEWS" \
    "$(printf '%b' 'sys\tmemory_global_by_current_bytes\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nsys\tx$memory_global_by_current_bytes\tNONE\tYES\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('memory_global_by_current_bytes', 'x\$memory_global_by_current_bytes')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys memory global current bytes table dependency metadata" \
    "$(printf '%b' 'sys\tmemory_global_by_current_bytes\tperformance_schema\tmemory_summary_global_by_event_name\nsys\tx$memory_global_by_current_bytes\tperformance_schema\tmemory_summary_global_by_event_name')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ('memory_global_by_current_bytes', 'x\$memory_global_by_current_bytes')
      ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME;"

expect_output \
    "sys memory global current bytes empty routine dependency metadata" \
    "" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('memory_global_by_current_bytes', 'x\$memory_global_by_current_bytes')
      ORDER BY TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME;"

expect_output \
    "sys memory global current bytes empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0\t0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'memory_global_by_current_bytes'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$memory_global_by_current_bytes'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'memory_global_by_current_bytes'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$memory_global_by_current_bytes'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'memory_global_by_current_bytes'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'x\$memory_global_by_current_bytes'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'memory_global_by_current_bytes'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'x\$memory_global_by_current_bytes');"

expect_contains \
    "sys memory global current bytes SHOW CREATE VIEW" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`memory_global_by_current_bytes\`" \
    "SHOW CREATE VIEW sys.memory_global_by_current_bytes;"

expect_contains \
    "sys memory global current bytes SHOW CREATE source" \
    "from \`performance_schema\`.\`memory_summary_global_by_event_name\` where" \
    "SHOW CREATE VIEW sys.memory_global_by_current_bytes;"

expect_contains \
    "sys memory global current bytes SHOW CREATE filter/order" \
    "where (\`performance_schema\`.\`memory_summary_global_by_event_name\`.\`CURRENT_NUMBER_OF_BYTES_USED\` > 0) order by \`performance_schema\`.\`memory_summary_global_by_event_name\`.\`CURRENT_NUMBER_OF_BYTES_USED\` desc" \
    "SHOW CREATE VIEW sys.memory_global_by_current_bytes;"

expect_contains \
    "sys x memory global current bytes SHOW CREATE VIEW" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`x\$memory_global_by_current_bytes\`" \
    "SHOW CREATE VIEW sys.\`x\$memory_global_by_current_bytes\`;"

expect_contains \
    "sys memory global current bytes SHOW CREATE TABLE selected schema" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`memory_global_by_current_bytes\`" \
    "USE sys; SHOW CREATE TABLE memory_global_by_current_bytes;"

expect_contains \
    "sys x memory global current bytes SHOW CREATE TABLE selected schema" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`x\$memory_global_by_current_bytes\`" \
    "USE sys; SHOW CREATE TABLE \`x\$memory_global_by_current_bytes\`;"

expect_output \
    "sys memory global current bytes has runtime rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.memory_global_by_current_bytes;"

expect_output \
    "sys x memory global current bytes has runtime rows" \
    "1" \
    "SELECT COUNT(*) > 0 FROM sys.\`x\$memory_global_by_current_bytes\`;"

expect_output \
    "sys memory global current bytes selected rows" \
    "1" \
    "USE sys; SELECT COUNT(*) > 0 FROM memory_global_by_current_bytes;"

expect_output \
    "sys x memory global current bytes selected rows" \
    "1" \
    "USE sys; SELECT COUNT(*) > 0 FROM \`x\$memory_global_by_current_bytes\`;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.memory_global_by_current_bytes; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys memory global current bytes SELECT status: expected [0	-1], got [$status]"
fi

status=$(run_mysql "SELECT COUNT(*) FROM sys.\`x\$memory_global_by_current_bytes\`; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys x memory global current bytes SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_memory_global_by_current_bytes_views_expectations: ok"
